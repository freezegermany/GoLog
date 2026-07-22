#pragma once
#include <pebble.h>
void altitude_tracker_reset(void);
double altitude_tracker_add_sample(double raw_altitude_m);
double altitude_tracker_get_smoothed(void);
double altitude_tracker_get_total_gain(void);
double altitude_tracker_get_total_loss(void);
