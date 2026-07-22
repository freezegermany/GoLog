#include "map_view.h"
#include "../theme.h"
#include "../lib/pebbletrail_map.h"
static Window *s_map_window;
static void window_load(Window *window) { golog_style_window(window); pebbletrail_map_layer_init(window_get_root_layer(window)); }
static void window_unload(Window *window) { pebbletrail_map_layer_deinit(); window_destroy(s_map_window); }
void map_view_push(void) { s_map_window = window_create(); window_set_window_handlers(s_map_window, (WindowHandlers){ .load = window_load, .unload = window_unload }); window_stack_push(s_map_window, true); }
void map_view_add_point(double lat, double lon) { pebbletrail_map_add_track_point(lat, lon); }
