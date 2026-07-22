#pragma once
#include <pebble.h>
#include "activity_types.h"
void autopause_set_run_threshold_kmh(double threshold_kmh);
void autopause_set_cycle_threshold_kmh(double threshold_kmh);
double autopause_get_run_threshold_kmh(void);
double autopause_get_cycle_threshold_kmh(void);
bool autopause_evaluate(ActivityType activity_type, double speed_kmh, bool *out_is_now_paused);
bool autopause_is_paused(void);
void autopause_reset(void);
