#include "pebbletrail_map.h"
#define MAX_TRACK_POINTS 512
typedef struct { double lat; double lon; } TrackPoint;
static Layer *s_map_layer;
static TrackPoint s_track_points[MAX_TRACK_POINTS];
static int s_track_point_count = 0;
static void map_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (s_track_point_count < 2) return;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int i = 1; i < s_track_point_count; i++) {
    GPoint p1 = GPoint((int16_t)((s_track_points[i - 1].lon + 180) / 360 * bounds.size.w), (int16_t)((90 - s_track_points[i - 1].lat) / 180 * bounds.size.h));
    GPoint p2 = GPoint((int16_t)((s_track_points[i].lon + 180) / 360 * bounds.size.w), (int16_t)((90 - s_track_points[i].lat) / 180 * bounds.size.h));
    graphics_draw_line(ctx, p1, p2);
  }
}
void pebbletrail_map_layer_init(Layer *parent_layer) {
  GRect bounds = layer_get_bounds(parent_layer);
  s_map_layer = layer_create(bounds);
  layer_set_update_proc(s_map_layer, map_layer_update_proc);
  layer_add_child(parent_layer, s_map_layer);
}
void pebbletrail_map_layer_deinit(void) { layer_destroy(s_map_layer); s_track_point_count = 0; }
void pebbletrail_map_add_track_point(double lat, double lon) {
  if (s_track_point_count >= MAX_TRACK_POINTS) return;
  s_track_points[s_track_point_count].lat = lat;
  s_track_points[s_track_point_count].lon = lon;
  s_track_point_count++;
  if (s_map_layer) layer_mark_dirty(s_map_layer);
}
