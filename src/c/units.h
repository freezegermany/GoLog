#pragma once
#include <pebble.h>
void units_format_pace(double pace_sec_per_km, char *out_buf, size_t buf_size);
double units_convert_distance(double distance_m);
void units_set_metric(bool is_metric);
bool units_is_metric(void);
