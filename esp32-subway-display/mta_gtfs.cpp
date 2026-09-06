#include "mta_gtfs.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>

namespace {

// ── Minimal streaming protobuf reader ───────────────────────────────────
// GTFS-realtime feeds are protobuf. Rather than pulling in a full
// protobuf runtime (nanopb + generated GTFS-rt schema) for the handful of
// fields we actually need, we walk the wire format by hand, reading
// straight off the HTTP stream instead of buffering the whole feed --
// the A/C/E feed can run several hundred KB during rush hour, more than
// an ESP32 wants to hold in RAM at once.
//
// Field numbers below come straight from transit_realtime.proto:
//   FeedMessage      { repeated FeedEntity entity = 2; }
//   FeedEntity       { TripUpdate trip_update = 3; }
//   TripUpdate       { TripDescriptor trip = 1; repeated StopTimeUpdate stop_time_update = 2; }
//   TripDescriptor   { string route_id = 5; }
//   StopTimeUpdate   { StopTimeEvent arrival = 2; string stop_id = 4; }
//   StopTimeEvent    { int64 time = 2; }

int readByte(Stream &s, long &bytesLeft) {
  if (bytesLeft <= 0) return -1;
  unsigned long start = millis();
  while (!s.available()) {
    if (millis() - start > 8000) return -1;  // stalled connection / end of stream
    delay(1);
  }
  int b = s.read();
  if (b >= 0) bytesLeft--;
  return b;
}

bool readVarint(Stream &s, long &bytesLeft, uint64_t &out) {
  out = 0;
  for (int shift = 0; shift <= 63; shift += 7) {
    int b = readByte(s, bytesLeft);
    if (b < 0) return false;
    out |= (uint64_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) return true;
  }
  return false;
}

bool skipBytes(Stream &s, long &bytesLeft, long n) {
  for (long i = 0; i < n; i++) {
    if (readByte(s, bytesLeft) < 0) return false;
  }
  return true;
}

bool skipField(Stream &s, long &bytesLeft, int wireType) {
  switch (wireType) {
    case 0: {  // varint
      uint64_t v;
      return readVarint(s, bytesLeft, v);
    }
    case 1:  // 64-bit
      return skipBytes(s, bytesLeft, 8);
    case 2: {  // length-delimited
      uint64_t len;
      if (!readVarint(s, bytesLeft, len)) return false;
      return skipBytes(s, bytesLeft, (long)len);
    }
    case 5:  // 32-bit
      return skipBytes(s, bytesLeft, 4);
    default:
      return false;  // unknown wire type, bail on this message
  }
}

bool readString(Stream &s, long &bytesLeft, long n, char *buf, size_t bufSize) {
  size_t i = 0;
  for (long j = 0; j < n; j++) {
    int b = readByte(s, bytesLeft);
    if (b < 0) return false;
    if (i < bufSize - 1) buf[i++] = (char)b;
  }
  buf[i] = '\0';
  return true;
}

// ── transit_realtime.StopTimeEvent ──────────────────────────────────────
void parseStopTimeEvent(Stream &s, long len, time_t &outTime) {
  long left = len;
  outTime = 0;
  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;
    if (fieldNum == 2 && wireType == 0) {
      uint64_t v;
      if (!readVarint(s, left, v)) break;
      outTime = (time_t)v;
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

// ── transit_realtime.TripUpdate.StopTimeUpdate ──────────────────────────
struct StopTimeUpdateResult {
  char stopId[16] = {0};
  time_t arrival = 0;
  bool hasArrival = false;
};

void parseStopTimeUpdate(Stream &s, long len, StopTimeUpdateResult &out) {
  long left = len;
  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;
    if (fieldNum == 2 && wireType == 2) {
      uint64_t subLen;
      if (!readVarint(s, left, subLen)) break;
      long before = left;
      time_t t;
      parseStopTimeEvent(s, (long)subLen, t);
      out.arrival = t;
      out.hasArrival = true;
      left = before - (long)subLen;
    } else if (fieldNum == 4 && wireType == 2) {
      uint64_t strLen;
      if (!readVarint(s, left, strLen)) break;
      readString(s, left, (long)strLen, out.stopId, sizeof(out.stopId));
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

// ── transit_realtime.TripDescriptor ─────────────────────────────────────
void parseTripDescriptor(Stream &s, long len, char *routeId, size_t routeIdSize) {
  long left = len;
  routeId[0] = '\0';
  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;
    if (fieldNum == 5 && wireType == 2) {
      uint64_t strLen;
      if (!readVarint(s, left, strLen)) break;
      readString(s, left, (long)strLen, routeId, routeIdSize);
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

// ── transit_realtime.TripUpdate ─────────────────────────────────────────
// Assumes the TripDescriptor (field 1) is encoded before the
// StopTimeUpdate entries (field 2), which is how every MTA producer emits
// this feed in practice, even though protobuf doesn't guarantee field order.
void parseTripUpdate(Stream &s, long len, TrainArrivals &out) {
  long left = len;
  char routeId[8] = {0};
  bool routeMatches = false;

  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;

    if (fieldNum == 1 && wireType == 2) {
      uint64_t subLen;
      if (!readVarint(s, left, subLen)) break;
      long before = left;
      parseTripDescriptor(s, (long)subLen, routeId, sizeof(routeId));
      left = before - (long)subLen;
      routeMatches = (strcmp(routeId, TARGET_ROUTE_ID) == 0);
    } else if (fieldNum == 2 && wireType == 2) {
      uint64_t subLen;
      if (!readVarint(s, left, subLen)) break;
      long before = left;
      StopTimeUpdateResult stu;
      parseStopTimeUpdate(s, (long)subLen, stu);
      left = before - (long)subLen;

      if (routeMatches && stu.hasArrival) {
        time_t now = time(nullptr);
        int minutes = (int)((stu.arrival - now) / 60);
        if (minutes < 0) continue;

        if (strcmp(stu.stopId, STOP_ID_TARGET) == 0 &&
            out.count < MAX_ARRIVALS) {
          out.minutes[out.count++] = minutes;
        }
      }
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

// ── transit_realtime.FeedEntity ─────────────────────────────────────────
void parseEntity(Stream &s, long len, TrainArrivals &out) {
  long left = len;
  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;
    if (fieldNum == 3 && wireType == 2) {
      uint64_t subLen;
      if (!readVarint(s, left, subLen)) break;
      long before = left;
      parseTripUpdate(s, (long)subLen, out);
      left = before - (long)subLen;
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

// ── transit_realtime.FeedMessage ────────────────────────────────────────
void parseFeedMessage(Stream &s, long len, TrainArrivals &out) {
  long left = len;
  while (left > 0) {
    uint64_t tag;
    if (!readVarint(s, left, tag)) break;
    int fieldNum = tag >> 3, wireType = tag & 0x7;
    if (fieldNum == 2 && wireType == 2) {
      uint64_t subLen;
      if (!readVarint(s, left, subLen)) break;
      long before = left;
      parseEntity(s, (long)subLen, out);
      left = before - (long)subLen;
    } else if (!skipField(s, left, wireType)) {
      break;
    }
  }
}

void sortAscending(int *arr, int count) {
  for (int i = 1; i < count; i++) {
    int key = arr[i], j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

}  // namespace

bool fetchArrivals(TrainArrivals &out) {
  TrainArrivals fresh;

  WiFiClientSecure client;
  // This is a public, read-only data feed (no credentials ever cross this
  // connection), so we skip certificate validation rather than pin a root
  // CA that MTA could rotate out from under us. See README.md if you'd
  // rather pin a certificate.
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, MTA_FEED_URL)) {
    Serial.println("HTTP begin() failed");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("MTA feed request failed, HTTP %d\n", code);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  WiFiClient *stream = http.getStreamPtr();

  if (contentLength <= 0) {
    // Unknown/chunked length: use a generous cap and let the byte-level
    // stall timeout in readByte() stop us once the connection closes.
    contentLength = 2 * 1024 * 1024;
  }

  parseFeedMessage(*stream, contentLength, fresh);
  http.end();

  sortAscending(fresh.minutes, fresh.count);
  fresh.valid = true;
  out = fresh;

  Serial.printf("Fetched arrivals -- %d upcoming\n", fresh.count);
  return true;
}
