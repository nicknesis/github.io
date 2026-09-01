#pragma once

// ── WiFi credentials ────────────────────────────────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ── Timezone (POSIX TZ string) ──────────────────────────────────────────
// America/New_York, with automatic EST/EDT daylight-saving switching.
#define LOCAL_TZ "EST5EDT,M3.2.0,M11.1.0"

// ── MTA GTFS-realtime feed ───────────────────────────────────────────────
// The A/C/E feed carries C train updates. Full feed list is here:
// https://api.mta.info/#/subwayRealTimeFeeds
// (No API key is required for these feeds.)
#define MTA_FEED_URL "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs-ace"

// ── Station to watch ─────────────────────────────────────────────────────
// Look up your station's stop_id in the MTA's static GTFS "stops.txt":
// https://www.mta.info/developers -> Subway static feed -> stops.txt
// Subway stop_ids end in N (uptown/northbound) or S (downtown/southbound).
// The example below is 168 St on the 8th Ave line (parent stop_id "A15").
#define STOP_ID_UPTOWN   "A15N"
#define STOP_ID_DOWNTOWN "A15S"

// Route letter this display tracks (used to filter the shared A/C/E feed).
#define TARGET_ROUTE_ID "C"

// How many upcoming arrivals to keep per direction.
#define MAX_ARRIVALS_PER_DIRECTION 2

// ── Timing ────────────────────────────────────────────────────────────────
#define FETCH_INTERVAL_MS 30000  // how often to re-poll the MTA feed
#define LOGO_DISPLAY_MS    4000  // how long the C bullet stays on screen
#define TIMES_DISPLAY_MS   6000  // how long arrival times stay on screen

// ── Matrix panel wiring (HUB75, ESP32-HUB75-MatrixPanel-I2S-DMA defaults) ──
// See README.md for the full wiring table and power notes.
#define PANEL_WIDTH  64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN   1

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN  23
#define B_PIN  19
#define C_PIN   5
#define D_PIN  17
#define E_PIN  -1  // not used on 1/16-scan 64x32 panels
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16
