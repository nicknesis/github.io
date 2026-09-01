#pragma once
#include <Arduino.h>
#include "config.h"

struct TrainArrivals {
  int uptownMinutes[MAX_ARRIVALS_PER_DIRECTION];
  int uptownCount = 0;
  int downtownMinutes[MAX_ARRIVALS_PER_DIRECTION];
  int downtownCount = 0;
  bool valid = false;
};

// Fetches the MTA GTFS-realtime A/C/E feed and fills `out` with the next
// upcoming arrival times (in minutes from now) for TARGET_ROUTE_ID at
// STOP_ID_UPTOWN / STOP_ID_DOWNTOWN. Returns true on success.
bool fetchArrivals(TrainArrivals &out);
