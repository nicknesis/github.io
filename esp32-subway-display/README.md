# ESP32 Subway (C Train) LED Matrix Display

Drives a 64x32 HUB75 RGB LED matrix panel from an ESP32 dev board to show:

- The MTA "C" train bullet logo (blue circle, white "C"), and
- Live uptown/downtown arrival countdowns for a station of your choice,
  pulled straight from MTA's public GTFS-realtime feed and refreshed
  every 30 seconds.

The two screens alternate automatically (logo for a few seconds, then
times, on repeat).

## Hardware

- ESP32 dev board (any variant with enough free GPIOs; a plain
  "ESP32 DevKitC" style board works fine)
- 64x32 HUB75 RGB LED matrix panel (the "2048" in the panel's listing is
  just 64x32 = 2048 pixels; any standard 1/16-scan HUB75 panel of this
  size works)
- A separate 5V power supply for the panel, rated for at least 4A
  (a fully-lit 64x32 panel can draw well over 2A; don't power it from the
  ESP32's USB or 3.3V rail)
- Common ground between the panel's 5V supply and the ESP32

## Wiring

These are the library's default pins (defined in `config.h` — change them
there if your board's GPIOs are already spoken for):

| HUB75 pin | ESP32 GPIO |
|-----------|-----------|
| R1        | 25 |
| G1        | 26 |
| B1        | 27 |
| R2        | 14 |
| G2        | 12 |
| B2        | 13 |
| A         | 23 |
| B         | 19 |
| C         | 5  |
| D         | 17 |
| E         | not connected (1/16-scan 64x32 panels don't use it) |
| LAT       | 4  |
| OE        | 15 |
| CLK       | 16 |
| GND       | GND (also tie to the panel's 5V supply ground) |

Power the panel from its own 5V supply, not the ESP32.

## Arduino IDE setup

1. Install the ESP32 board package (Boards Manager → search "esp32", by
   Espressif Systems).
2. Install libraries via Library Manager:
   - **ESP32 HUB75 LED MATRIX PANEL DMA Display** (by mrfaptastic)
3. Board settings: any ESP32 Dev Module profile works; default flash size
   (4MB) and partition scheme are fine.
4. Open `esp32-subway-display.ino` — it will pick up `config.h`,
   `mta_gtfs.h/.cpp`, and `display_ui.h/.cpp` from the same sketch folder
   automatically.

## Configuration

Edit `config.h` before flashing:

- `WIFI_SSID` / `WIFI_PASSWORD` — your network credentials.
- `STOP_ID_UPTOWN` / `STOP_ID_DOWNTOWN` — the GTFS stop IDs for your
  station. Look these up in MTA's static GTFS `stops.txt`
  (linked from https://www.mta.info/developers). Subway stop IDs end in
  `N` (uptown/northbound) or `S` (downtown/southbound) — e.g. `A15N` /
  `A15S` for 168 St on the 8th Ave line, which the C train serves.
- `TARGET_ROUTE_ID` — defaults to `"C"`. The A/C/E feed carries all three
  routes; this filters to just the one you want. Change it (and the feed
  URL/stop IDs) if you'd rather track the A or E train.
- `MTA_FEED_URL` — defaults to the public A/C/E feed. See
  https://api.mta.info/#/subwayRealTimeFeeds for the full feed list if
  MTA ever changes the endpoint.
- `FETCH_INTERVAL_MS`, `LOGO_DISPLAY_MS`, `TIMES_DISPLAY_MS` — timing
  knobs.

No API key is needed — MTA's GTFS-realtime feeds have been public since
2019.

## How the live times work

MTA's feed is protobuf (GTFS-realtime), not JSON, and can run several
hundred KB during rush hour. Rather than pulling in a full protobuf
runtime, `mta_gtfs.cpp` walks the wire format by hand, field-by-field,
reading directly from the HTTP stream instead of buffering the whole
response — the ESP32 doesn't have the RAM to hold a large feed plus the
WiFi stack at once. It picks out just the fields it needs (route ID, stop
ID, arrival time) and ignores everything else.

## Known limitations / things to tweak

- **TLS**: the sketch uses `WiFiClientSecure::setInsecure()` rather than
  pinning MTA's certificate, since this is a public, read-only feed with
  no credentials involved. If you'd rather pin a cert, swap in
  `client.setCACert(...)` in `mta_gtfs.cpp` with MTA's current root CA.
- **Blocking fetch**: the feed fetch/parse (roughly 1–3 seconds) runs in
  the main loop, so the display briefly pauses during each refresh. For
  smoother animation, move `fetchArrivals()` onto a separate FreeRTOS
  task pinned to the other core.
- **Layout**: text layout constants live in `display_ui.cpp` — tweak
  `printLabelRow`/`printMinutesRow` positions if you want a different
  look, more arrivals per direction, or to add a "delayed"/service-alert
  banner using the feed's `alert` entities.
