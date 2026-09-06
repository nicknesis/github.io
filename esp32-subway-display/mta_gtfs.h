#pragma once
#include <Arduino.h>
#include "config.h"

struct TrainArrivals {
  int minutes[MAX_ARRIVALS];
  int count = 0;
  bool valid = false;
};

// Fetches the MTA GTFS-realtime A/C/E feed and fills `out` with the next
// upcoming arrival times (in minutes from now) for TARGET_ROUTE_ID at
// STOP_ID_TARGET. Returns true on success.
bool fetchArrivals(TrainArrivals &out);
