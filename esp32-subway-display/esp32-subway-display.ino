// ESP32 + 64x32 HUB75 RGB matrix panel: MTA "C" train bullet logo with
// live-updating countdown to Manhattan-bound trains at Clinton-Washington Avs.
//
// See README.md for wiring, library requirements, and how to configure
// config.h for your WiFi network and station.

#include <WiFi.h>

#include "config.h"
#include "display_ui.h"
#include "mta_gtfs.h"

TrainArrivals arrivals;
unsigned long lastFetchMs = 0;
unsigned long lastScreenSwitchMs = 0;
bool showingLogo = true;

void connectWiFi() {
  Serial.println("Scanning for nearby WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("  (none found -- that's unusual, worth trying again)");
  } else {
    for (int i = 0; i < n; i++) {
      Serial.printf("  seen: \"%s\" (signal %d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  }
  Serial.printf("Target network from config.h: \"%s\"\n", WIFI_SSID);

  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    if (millis() - start > 15000) {
      Serial.println();
      Serial.printf("Still not connected after 15s. WiFi.status() = %d\n", WiFi.status());
      Serial.println("(1=network not found, 4=connect failed, 6=wrong password, 7=disconnected)");
      start = millis();
    }
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  displayInit();
  displaySplash();

  connectWiFi();

  configTzTime(LOCAL_TZ, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("NTP sync retrying...");
  }
  Serial.println("Time synced.");

  fetchArrivals(arrivals);
  lastFetchMs = millis();
  lastScreenSwitchMs = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastFetchMs >= FETCH_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      fetchArrivals(arrivals);
    } else {
      connectWiFi();
    }
    lastFetchMs = now;
  }

  unsigned long screenDuration = showingLogo ? LOGO_DISPLAY_MS : TIMES_DISPLAY_MS;
  if (now - lastScreenSwitchMs >= screenDuration) {
    showingLogo = !showingLogo;
    lastScreenSwitchMs = now;
  }

  if (showingLogo) {
    displayLogoScreen();
  } else {
    displayTimesScreen(arrivals);
  }

  delay(50);
}
