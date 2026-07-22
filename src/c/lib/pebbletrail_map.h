#pragma once
#include <pebble.h>
void pebbletrail_map_layer_init(Layer *parent_layer);
void pebbletrail_map_layer_deinit(void);
void pebbletrail_map_add_track_point(double lat, double lon);
