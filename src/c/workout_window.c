#include "workout_window.h"
#include "settings.h"
#include <pebble.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define KEY_GPS_SPEED_KMH 1
#define KEY_GPS_DISTANCE_M 2
#define KEY_GPS_PACE_S 3
#define KEY_HR_BPM 4
#define KEY_HR_SOURCE 5
#define KEY_CMD_START 10
#define KEY_CMD_STOP 11
#define KEY_ACTIVITY_TYPE 12
#define KEY_GPS_LAT_E6 13
#define KEY_GPS_LON_E6 14
#define KEY_MAP_INDEX 20
#define KEY_MAP_LENGTH 21
#define KEY_MAP_CHUNK 22
#define KEY_MAP_CHUNK_SIZE 23
#define KEY_MAP_COMPLETE 24
#define KEY_MAP_WIDTH 25
#define KEY_MAP_HEIGHT 26
#define KEY_MAP_ROW_BYTES 27
#define KEY_MAP_IS_COLOR 28
#define KEY_GPS_BEARING_DEG 29
#define KEY_MAP_ZOOM 30
#define KEY_CMD_LAP 31
#define KEY_MAP_MODE 32

#define HEADER_H 28
#define ROW_H 28
#define FOOTER_H 22
#define SIDE_PAD 6

#define VIEW_ALL 0
#define VIEW_MAP 1
#define VIEW_BIGSPEED 2
#define NUM_VIEWS 3

#define MAX_CRUMBS 200
#define AUTO_PAUSE_TICKS 10

typedef enum { PAUSE_NONE = 0, PAUSE_MANUAL, PAUSE_AUTO } PauseState;

static Window *s_window = NULL;
static ActivityType s_activity_type = ACTIVITY_RUNNING;
static AppSettings s_settings;

static TextLayer *s_header_layer;
static TextLayer *s_elapsed_layer;
static TextLayer *s_speed_layer;
static TextLayer *s_speed_label_layer;
static TextLayer *s_distance_layer;
static TextLayer *s_pace_layer;
static TextLayer *s_hr_layer;
static TextLayer *s_status_layer;
static Layer *s_map_layer;
static TextLayer *s_map_speed_layer;
static TextLayer *s_map_distance_layer;
static TextLayer *s_map_hr_layer;
static TextLayer *s_map_status_layer;

static char s_header_buf[16];
static char s_elapsed_buf[14];
static char s_speed_buf[24];
static char s_distance_buf[24];
static char s_pace_buf[24];
static char s_hr_buf[24];
static char s_map_speed_buf[20];
static char s_diag_buf[32];

static int32_t s_speed_kmh = 0;
static uint32_t s_dist_m = 0;
static uint32_t s_pace_s = 0;
static uint32_t s_hr_bpm = 0;
static bool s_hr_is_ble = false;
static int32_t s_lat_e6 = 0;
static int32_t s_lon_e6 = 0;
static bool s_have_gps = false;
static AppTimer *s_elapsed_timer = NULL;
static uint32_t s_elapsed_s = 0;
static bool s_recording = false;
static PauseState s_pause_state = PAUSE_NONE;
static uint8_t s_view = VIEW_ALL;
static uint8_t s_slow_ticks = 0;
static uint8_t *s_map_buffer = NULL;
static uint32_t s_map_buffer_size = 0;
static uint16_t s_map_width = 0;
static uint16_t s_map_height = 0;
static uint16_t s_map_row_bytes = 0;
static bool s_map_is_color = false;
static bool s_map_ready = false;
static int32_t s_bearing_deg = -1;
static uint8_t s_map_zoom = 16;
static uint8_t s_map_mode = 0;
static uint32_t s_lap_count = 0;
static GPoint s_crumbs[MAX_CRUMBS];
static uint16_t s_crumb_count = 0;
static int32_t s_crumb_lat_min = 0, s_crumb_lat_max = 0;
static int32_t s_crumb_lon_min = 0, s_crumb_lon_max = 0;

static TextLayer *prv_make_tl(GRect frame, GColor fg, GColor bg, GFont font, GTextAlignment align) {
  TextLayer *tl = text_layer_create(frame);
  text_layer_set_text_color(tl, fg);
  text_layer_set_background_color(tl, bg);
  text_layer_set_font(tl, font);
  text_layer_set_text_alignment(tl, align);
  return tl;
}

static const char *prv_speed_unit(void) { return "km/h"; }

static void prv_send_cmd(uint32_t key, uint32_t value) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) return;
  dict_write_uint8(iter, key, (uint8_t)value);
  app_message_outbox_send();
}

static void prv_format_elapsed(uint32_t total_s, char *buf, size_t len) {
  uint32_t h = total_s / 3600;
  uint32_t m = (total_s % 3600) / 60;
  uint32_t s = total_s % 60;
  if (h > 0) snprintf(buf, len, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  else snprintf(buf, len, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

static uint32_t prv_scale_meters_for_width(int16_t width_px, double lat_deg, uint8_t zoom) {
  double meters_per_pixel = 156543.03392 * cos(lat_deg * 3.141592653589793 / 180.0) / (1 << zoom);
  if (meters_per_pixel < 0) meters_per_pixel = -meters_per_pixel;
  return (uint32_t)(meters_per_pixel * width_px);
}

static void prv_update_speed_label(void) {
  if (s_view == VIEW_BIGSPEED) snprintf(s_speed_buf, sizeof(s_speed_buf), "%ld", (long)s_speed_kmh);
  else snprintf(s_speed_buf, sizeof(s_speed_buf), "Speed %ld %s", (long)s_speed_kmh, prv_speed_unit());
  text_layer_set_text(s_speed_layer, s_speed_buf);
  snprintf(s_map_speed_buf, sizeof(s_map_speed_buf), "SPD %ld %s", (long)s_speed_kmh, prv_speed_unit());
  if (s_map_speed_layer) text_layer_set_text(s_map_speed_layer, s_map_speed_buf);
}

static void prv_update_distance_label(void) {
  if (s_dist_m < 1000) snprintf(s_distance_buf, sizeof(s_distance_buf), "Distance %lu m", (unsigned long)s_dist_m);
  else snprintf(s_distance_buf, sizeof(s_distance_buf), "Distance %lu.%02lu km", (unsigned long)(s_dist_m / 1000), (unsigned long)((s_dist_m % 1000) / 10));
  text_layer_set_text(s_distance_layer, s_distance_buf);
  if (s_map_distance_layer) text_layer_set_text(s_map_distance_layer, s_distance_buf);
}

static void prv_update_pace_label(void) {
  if (s_pace_s == 0) snprintf(s_pace_buf, sizeof(s_pace_buf), "Pace --:-- /km");
  else snprintf(s_pace_buf, sizeof(s_pace_buf), "Pace %02lu:%02lu /km", (unsigned long)(s_pace_s / 60), (unsigned long)(s_pace_s % 60));
  text_layer_set_text(s_pace_layer, s_pace_buf);
}

static void prv_update_hr_label(void) {
  if (s_hr_bpm == 0) snprintf(s_hr_buf, sizeof(s_hr_buf), "HR --- bpm");
  else if (s_hr_is_ble) snprintf(s_hr_buf, sizeof(s_hr_buf), "HR %lu bpm BLE", (unsigned long)s_hr_bpm);
  else snprintf(s_hr_buf, sizeof(s_hr_buf), "HR %lu bpm", (unsigned long)s_hr_bpm);
  text_layer_set_text(s_hr_layer, s_hr_buf);
  if (s_map_hr_layer) text_layer_set_text(s_map_hr_layer, s_hr_buf);
}

static void prv_update_status(void) {
  const char *txt;
  GColor col;
  if (!s_recording) {
    txt = "SELECT to start";
    col = GColorYellow;
  } else if (!s_have_gps) {
    txt = "Waiting for GPS";
    col = GColorYellow;
  } else if (!s_map_ready && s_view == VIEW_MAP) {
    txt = "GPS ok - loading map";
    col = GColorYellow;
  } else {
    switch (s_pause_state) {
      case PAUSE_NONE:
        snprintf(s_diag_buf, sizeof(s_diag_buf), "* REC HR:%lu L:%lu M:%u", (unsigned long)s_hr_bpm, (unsigned long)s_lap_count, (unsigned)s_map_mode);
        txt = s_diag_buf;
        col = GColorYellow;
        break;
      case PAUSE_MANUAL:
        txt = "|| PAUSED [SELECT]";
        col = GColorCyan;
        break;
      default:
        txt = "|| AUTO-PAUSE";
        col = GColorCyan;
        break;
    }
  }
  text_layer_set_text(s_status_layer, txt);
  text_layer_set_text_color(s_status_layer, col);
  if (s_map_status_layer) {
    text_layer_set_text(s_map_status_layer, txt);
    text_layer_set_text_color(s_map_status_layer, col);
  }
}

static void prv_add_crumb(int32_t lat_e6, int32_t lon_e6) {
  if (lat_e6 == 0 && lon_e6 == 0) return;
  int16_t cx = (int16_t)(lon_e6 / 1000);
  int16_t cy = (int16_t)(lat_e6 / 1000);
  if (s_crumb_count < MAX_CRUMBS) s_crumbs[s_crumb_count++] = GPoint(cx, cy);
  else {
    memmove(&s_crumbs[0], &s_crumbs[1], (MAX_CRUMBS - 1) * sizeof(GPoint));
    s_crumbs[MAX_CRUMBS - 1] = GPoint(cx, cy);
  }
  if (s_crumb_count == 1) {
    s_crumb_lat_min = s_crumb_lat_max = cy;
    s_crumb_lon_min = s_crumb_lon_max = cx;
  } else {
    if (cy < s_crumb_lat_min) s_crumb_lat_min = cy;
    if (cy > s_crumb_lat_max) s_crumb_lat_max = cy;
    if (cx < s_crumb_lon_min) s_crumb_lon_min = cx;
    if (cx > s_crumb_lon_max) s_crumb_lon_max = cx;
  }
}

static void prv_draw_fallback_track(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  if (s_crumb_count > 1) {
    int32_t lat_range = s_crumb_lat_max - s_crumb_lat_min;
    int32_t lon_range = s_crumb_lon_max - s_crumb_lon_min;
    if (lat_range == 0) lat_range = 1;
    if (lon_range == 0) lon_range = 1;
    int16_t pad = 8;
    int16_t draw_w = bounds.size.w - pad * 2;
    int16_t draw_h = bounds.size.h - pad * 2;
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    GPoint prev = GPoint(0, 0);
    for (uint16_t i = 0; i < s_crumb_count; i++) {
      int16_t px = pad + (int16_t)(((int32_t)(s_crumbs[i].x - s_crumb_lon_min) * draw_w) / lon_range);
      int16_t py = pad + draw_h - (int16_t)(((int32_t)(s_crumbs[i].y - s_crumb_lat_min) * draw_h) / lat_range);
      GPoint cur = GPoint(px, py);
      if (i > 0) graphics_draw_line(ctx, prev, cur);
      prev = cur;
    }
    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, prev, 4);
  } else if (s_have_gps) {
    GPoint c = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_line(ctx, GPoint(c.x - 10, c.y), GPoint(c.x + 10, c.y));
    graphics_draw_line(ctx, GPoint(c.x, c.y - 10), GPoint(c.x, c.y + 10));
    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, c, 3);
  } else {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, s_recording ? "Waiting for GPS..." : "Start activity\nfor map", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(10, bounds.size.h / 2 - 20, bounds.size.w - 20, 40), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void prv_map_layer_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_map_ready && s_map_buffer) {
    GBitmap *fb = graphics_capture_frame_buffer(ctx);
    if (fb) {
      const GBitmapFormat fb_format = gbitmap_get_format(fb);
      const bool fb_is_circular = fb_format == GBitmapFormat8BitCircular;
      const uint16_t fb_row_bytes = gbitmap_get_bytes_per_row(fb);
      const uint16_t copy_height = s_map_height < bounds.size.h ? s_map_height : bounds.size.h;
      if (fb_is_circular) {
        const uint16_t copy_width = s_map_width < bounds.size.w ? s_map_width : bounds.size.w;
        for (uint16_t y = 0; y < copy_height; y++) {
          GBitmapDataRowInfo row_info = gbitmap_get_data_row_info(fb, y);
          if (!row_info.data) continue;
          int16_t min_x = row_info.min_x < 0 ? 0 : row_info.min_x;
          int16_t max_x = row_info.max_x >= copy_width ? copy_width - 1 : row_info.max_x;
          if (min_x > max_x) continue;
          const uint8_t *src_row = s_map_buffer + y * s_map_row_bytes;
          for (int16_t x = min_x; x <= max_x; x++) {
            if (s_map_is_color) row_info.data[x] = src_row[x];
            else {
              const uint8_t src = src_row[x >> 3];
              const bool bit = ((src >> (7 - (x & 7))) & 0x1) != 0;
              row_info.data[x] = bit ? 0xFF : 0xC0;
            }
          }
        }
      } else if (!PBL_IF_COLOR_ELSE(true, false)) {
        uint8_t *fb_data = gbitmap_get_data(fb);
        const uint16_t safe_row_bytes = s_map_row_bytes < fb_row_bytes ? s_map_row_bytes : fb_row_bytes;
        for (uint16_t y = 0; y < copy_height; y++) memcpy(fb_data + y * fb_row_bytes, s_map_buffer + y * s_map_row_bytes, safe_row_bytes);
      } else {
        uint8_t *fb_data = gbitmap_get_data(fb);
        const uint16_t copy_width = s_map_width < bounds.size.w ? s_map_width : bounds.size.w;
        for (uint16_t y = 0; y < copy_height; y++) {
          const uint8_t *src_row = s_map_buffer + y * s_map_row_bytes;
          uint8_t *dst_row = fb_data + y * fb_row_bytes;
          if (s_map_is_color) memcpy(dst_row, src_row, copy_width);
          else {
            for (uint16_t x = 0; x < copy_width; x++) {
              const uint8_t src = src_row[x >> 3];
              const bool bit = ((src >> (7 - (x & 7))) & 0x1) != 0;
              dst_row[x] = bit ? 0xFF : 0xC0;
            }
          }
        }
      }
      graphics_release_frame_buffer(ctx, fb);
    }

    int16_t scale_px = bounds.size.w / 4;
    uint32_t scale_m = prv_scale_meters_for_width(scale_px, s_lat_e6 / 1000000.0, s_map_zoom);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_line(ctx, GPoint(10, bounds.size.h - 34), GPoint(10 + scale_px, bounds.size.h - 34));
    graphics_draw_line(ctx, GPoint(10, bounds.size.h - 38), GPoint(10, bounds.size.h - 30));
    graphics_draw_line(ctx, GPoint(10 + scale_px, bounds.size.h - 38), GPoint(10 + scale_px, bounds.size.h - 30));
    char scale_buf[24];
    if (scale_m >= 1000) snprintf(scale_buf, sizeof(scale_buf), "%lu.%01lu km", (unsigned long)(scale_m/1000), (unsigned long)((scale_m%1000)/100));
    else snprintf(scale_buf, sizeof(scale_buf), "%lu m", (unsigned long)scale_m);
    graphics_draw_text(ctx, scale_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(10, bounds.size.h - 52, 100, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    prv_draw_fallback_track(layer, ctx);
  }
}

static void prv_apply_view(void) {
  bool all = (s_view == VIEW_ALL);
  bool mapv = (s_view == VIEW_MAP);
  bool big = (s_view == VIEW_BIGSPEED);
  layer_set_hidden(text_layer_get_layer(s_header_layer), mapv);
  layer_set_hidden(text_layer_get_layer(s_elapsed_layer), mapv);
  layer_set_hidden(text_layer_get_layer(s_distance_layer), !all);
  layer_set_hidden(text_layer_get_layer(s_pace_layer), !all);
  layer_set_hidden(text_layer_get_layer(s_hr_layer), !all);
  layer_set_hidden(text_layer_get_layer(s_status_layer), mapv);
  layer_set_hidden(s_map_layer, !mapv);
  layer_set_hidden(text_layer_get_layer(s_map_speed_layer), !mapv);
  layer_set_hidden(text_layer_get_layer(s_map_distance_layer), !mapv);
  layer_set_hidden(text_layer_get_layer(s_map_hr_layer), !mapv);
  layer_set_hidden(text_layer_get_layer(s_map_status_layer), !mapv);

  GRect root_bounds = layer_get_bounds(window_get_root_layer(s_window));
  int16_t W = root_bounds.size.w;
  prv_update_speed_label();
  if (big) {
    snprintf(s_header_buf, sizeof(s_header_buf), "Speed (%s)", prv_speed_unit());
    text_layer_set_text(s_speed_label_layer, s_header_buf);
    text_layer_set_font(s_speed_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_set_frame(text_layer_get_layer(s_speed_label_layer), GRect(0, root_bounds.size.h / 2 - 56, W, 28));
    text_layer_set_text_alignment(s_speed_label_layer, GTextAlignmentCenter);
    layer_set_hidden(text_layer_get_layer(s_speed_label_layer), false);
    text_layer_set_font(s_speed_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    layer_set_frame(text_layer_get_layer(s_speed_layer), GRect(0, root_bounds.size.h / 2 - 18, W, 60));
    text_layer_set_text_alignment(s_speed_layer, GTextAlignmentCenter);
    layer_set_hidden(text_layer_get_layer(s_speed_layer), false);
  } else if (all) {
    layer_set_hidden(text_layer_get_layer(s_speed_label_layer), true);
    text_layer_set_font(s_speed_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    layer_set_frame(text_layer_get_layer(s_speed_layer), GRect(SIDE_PAD, HEADER_H + 4, W - SIDE_PAD * 2, ROW_H));
    text_layer_set_text_alignment(s_speed_layer, GTextAlignmentLeft);
    layer_set_hidden(text_layer_get_layer(s_speed_layer), false);
  } else {
    layer_set_hidden(text_layer_get_layer(s_speed_label_layer), true);
    layer_set_hidden(text_layer_get_layer(s_speed_layer), true);
  }
  prv_update_status();
}

static void prv_elapsed_tick(void *context) {
  (void)context;
  if (!s_recording) return;
  if (s_pause_state == PAUSE_NONE) {
    s_elapsed_s++;
    prv_format_elapsed(s_elapsed_s, s_elapsed_buf, sizeof(s_elapsed_buf));
    text_layer_set_text(s_elapsed_layer, s_elapsed_buf);
  }
  s_elapsed_timer = app_timer_register(1000, prv_elapsed_tick, NULL);
}

static void prv_start_activity(void) {
  if (s_recording) return;
  s_recording = true;
  s_pause_state = PAUSE_NONE;
  s_elapsed_s = 0;
  s_speed_kmh = 0;
  s_dist_m = 0;
  s_pace_s = 0;
  s_have_gps = false;
  s_map_ready = false;
  s_crumb_count = 0;
  s_slow_ticks = 0;
  prv_format_elapsed(0, s_elapsed_buf, sizeof(s_elapsed_buf));
  text_layer_set_text(s_elapsed_layer, s_elapsed_buf);
  prv_update_speed_label();
  prv_update_distance_label();
  prv_update_pace_label();
  prv_update_status();
  if (s_elapsed_timer) app_timer_cancel(s_elapsed_timer);
  s_elapsed_timer = app_timer_register(1000, prv_elapsed_tick, NULL);
  prv_send_cmd(KEY_ACTIVITY_TYPE, (uint32_t)s_activity_type);
  prv_send_cmd(KEY_CMD_START, 1);
  if (s_map_layer) layer_mark_dirty(s_map_layer);
}

static void prv_stop_activity(void) {
  if (!s_recording) return;
  s_recording = false;
  s_pause_state = PAUSE_NONE;
  if (s_elapsed_timer) {
    app_timer_cancel(s_elapsed_timer);
    s_elapsed_timer = NULL;
  }
  prv_send_cmd(KEY_CMD_STOP, 1);
  prv_update_status();
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; s_view = (s_view + NUM_VIEWS - 1) % NUM_VIEWS; prv_apply_view(); }
static void prv_down_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; s_view = (s_view + 1) % NUM_VIEWS; prv_apply_view(); }
static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (!s_recording) prv_start_activity();
  else if (s_pause_state == PAUSE_MANUAL) s_pause_state = PAUSE_NONE;
  else s_pause_state = PAUSE_MANUAL;
  prv_update_status();
}
static void prv_select_long_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; if (s_recording) prv_stop_activity(); }
static void prv_up_long_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; }
static void prv_down_long_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; }
static void prv_back_click(ClickRecognizerRef recognizer, void *context) { (void)recognizer; (void)context; if (!s_recording) window_stack_pop(true); }

static void prv_click_config_provider(void *ctx) {
  (void)ctx;
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, prv_select_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, prv_up_click);
  window_long_click_subscribe(BUTTON_ID_UP, 700, prv_up_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, prv_down_long_click, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  (void)context;
  Tuple *t;
  if ((t = dict_find(iter, KEY_GPS_SPEED_KMH))) s_speed_kmh = t->value->int32;
  if ((t = dict_find(iter, KEY_GPS_DISTANCE_M))) s_dist_m = (uint32_t)t->value->int32;
  if ((t = dict_find(iter, KEY_GPS_PACE_S))) s_pace_s = (uint32_t)t->value->int32;
  if ((t = dict_find(iter, KEY_HR_BPM))) s_hr_bpm = (uint32_t)t->value->int32;
  if ((t = dict_find(iter, KEY_HR_SOURCE))) s_hr_is_ble = t->value->int32 ? true : false;
  bool got_lat = false, got_lon = false;
  if ((t = dict_find(iter, KEY_GPS_LAT_E6))) { s_lat_e6 = t->value->int32; got_lat = true; }
  if ((t = dict_find(iter, KEY_GPS_LON_E6))) { s_lon_e6 = t->value->int32; got_lon = true; }
  if ((got_lat || got_lon) && (s_lat_e6 != 0 || s_lon_e6 != 0)) {
    s_have_gps = true;
    prv_add_crumb(s_lat_e6, s_lon_e6);
  }
  if ((t = dict_find(iter, KEY_GPS_BEARING_DEG))) s_bearing_deg = t->value->int32;
  if ((t = dict_find(iter, KEY_MAP_ZOOM))) s_map_zoom = (uint8_t)t->value->uint8;
  if ((t = dict_find(iter, KEY_MAP_MODE))) s_map_mode = (uint8_t)t->value->uint8;
  if ((t = dict_find(iter, KEY_MAP_LENGTH))) {
    uint32_t len = (uint32_t)t->value->int32;
    if (s_map_buffer) free(s_map_buffer);
    s_map_buffer = malloc(len);
    s_map_buffer_size = len;
    s_map_ready = false;
  }
  if ((t = dict_find(iter, KEY_MAP_WIDTH))) s_map_width = (uint16_t)t->value->int32;
  if ((t = dict_find(iter, KEY_MAP_HEIGHT))) s_map_height = (uint16_t)t->value->int32;
  if ((t = dict_find(iter, KEY_MAP_ROW_BYTES))) s_map_row_bytes = (uint16_t)t->value->int32;
  if ((t = dict_find(iter, KEY_MAP_IS_COLOR))) s_map_is_color = t->value->int32 ? true : false;
  Tuple *t_index = dict_find(iter, KEY_MAP_INDEX);
  Tuple *t_chunk = dict_find(iter, KEY_MAP_CHUNK);
  Tuple *t_chunk_size = dict_find(iter, KEY_MAP_CHUNK_SIZE);
  if (t_index && t_chunk && t_chunk_size && s_map_buffer) {
    uint32_t off = (uint32_t)t_index->value->int32;
    uint32_t sz = (uint32_t)t_chunk_size->value->int32;
    if (off + sz <= s_map_buffer_size) memcpy(s_map_buffer + off, t_chunk->value->data, sz);
  }
  if (dict_find(iter, KEY_MAP_COMPLETE)) s_map_ready = true;
  prv_update_speed_label();
  prv_update_distance_label();
  prv_update_pace_label();
  prv_update_hr_label();
  prv_update_status();
  if (s_map_layer) layer_mark_dirty(s_map_layer);
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) { (void)reason; (void)context; }

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int16_t W = bounds.size.w;
  int16_t H = bounds.size.h;
  GFont fbold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont freg = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  GFont fsm = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  s_header_layer = prv_make_tl(GRect(SIDE_PAD, 0, W - SIDE_PAD * 2 - 60, HEADER_H), GColorWhite, GColorBlack, fbold, GTextAlignmentLeft);
  snprintf(s_header_buf, sizeof(s_header_buf), "%s", s_activity_type == ACTIVITY_RUNNING ? "RUNNING" : "CYCLING");
  text_layer_set_text(s_header_layer, s_header_buf);
  layer_add_child(root, text_layer_get_layer(s_header_layer));

  s_elapsed_layer = prv_make_tl(GRect(W - 64, 0, 60, HEADER_H), GColorWhite, GColorBlack, freg, GTextAlignmentRight);
  snprintf(s_elapsed_buf, sizeof(s_elapsed_buf), "00:00");
  text_layer_set_text(s_elapsed_layer, s_elapsed_buf);
  layer_add_child(root, text_layer_get_layer(s_elapsed_layer));

  s_speed_label_layer = prv_make_tl(GRect(0, 26, W, 28), GColorWhite, GColorBlack, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GTextAlignmentCenter);
  text_layer_set_text(s_speed_label_layer, "Speed");
  layer_add_child(root, text_layer_get_layer(s_speed_label_layer));

  s_speed_layer = prv_make_tl(GRect(SIDE_PAD, HEADER_H + 4, W - SIDE_PAD * 2, ROW_H), GColorWhite, GColorBlack, fbold, GTextAlignmentLeft);
  text_layer_set_text(s_speed_layer, "0");
  layer_add_child(root, text_layer_get_layer(s_speed_layer));

  s_distance_layer = prv_make_tl(GRect(SIDE_PAD, HEADER_H + 4 + ROW_H, W - SIDE_PAD * 2, ROW_H), GColorWhite, GColorBlack, freg, GTextAlignmentLeft);
  text_layer_set_text(s_distance_layer, "Distance 0 m");
  layer_add_child(root, text_layer_get_layer(s_distance_layer));

  s_pace_layer = prv_make_tl(GRect(SIDE_PAD, HEADER_H + 4 + ROW_H * 2, W - SIDE_PAD * 2, ROW_H), GColorWhite, GColorBlack, freg, GTextAlignmentLeft);
  text_layer_set_text(s_pace_layer, "Pace --:-- /km");
  layer_add_child(root, text_layer_get_layer(s_pace_layer));

  s_hr_layer = prv_make_tl(GRect(SIDE_PAD, HEADER_H + 4 + ROW_H * 3, W - SIDE_PAD * 2, ROW_H), GColorRed, GColorBlack, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GTextAlignmentLeft);
  text_layer_set_text(s_hr_layer, "HR --- bpm");
  layer_add_child(root, text_layer_get_layer(s_hr_layer));

  s_status_layer = prv_make_tl(GRect(SIDE_PAD, H - FOOTER_H, W - SIDE_PAD * 2, FOOTER_H), GColorYellow, GColorBlack, fsm, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  s_map_layer = layer_create(GRect(0, 0, W, H - FOOTER_H));
  layer_set_update_proc(s_map_layer, prv_map_layer_draw);
  layer_add_child(root, s_map_layer);

  s_map_speed_layer = prv_make_tl(GRect(4, 2, W/3 - 6, 22), GColorBlack, GColorWhite, fsm, GTextAlignmentLeft);
  text_layer_set_text(s_map_speed_layer, "SPD 0");
  layer_add_child(root, text_layer_get_layer(s_map_speed_layer));

  s_map_distance_layer = prv_make_tl(GRect(W/3, 2, W/3, 22), GColorBlack, GColorWhite, fsm, GTextAlignmentCenter);
  text_layer_set_text(s_map_distance_layer, "Distance 0 m");
  layer_add_child(root, text_layer_get_layer(s_map_distance_layer));

  s_map_hr_layer = prv_make_tl(GRect(W - (W/3), 2, W/3 - 4, 22), GColorBlack, GColorWhite, fsm, GTextAlignmentRight);
  text_layer_set_text(s_map_hr_layer, "HR ---");
  layer_add_child(root, text_layer_get_layer(s_map_hr_layer));

  s_map_status_layer = prv_make_tl(GRect(SIDE_PAD, H - FOOTER_H, W - SIDE_PAD * 2, FOOTER_H), GColorYellow, GColorBlack, fsm, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_map_status_layer));

  window_set_click_config_provider(window, prv_click_config_provider);
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(1024, 256);

  s_view = VIEW_ALL;
  prv_update_speed_label();
  prv_update_distance_label();
  prv_update_pace_label();
  prv_update_hr_label();
  prv_apply_view();
}

static void prv_window_unload(Window *window) {
  (void)window;
  if (s_elapsed_timer) {
    app_timer_cancel(s_elapsed_timer);
    s_elapsed_timer = NULL;
  }
  text_layer_destroy(s_header_layer);
  text_layer_destroy(s_elapsed_layer);
  text_layer_destroy(s_speed_label_layer);
  text_layer_destroy(s_speed_layer);
  text_layer_destroy(s_distance_layer);
  text_layer_destroy(s_pace_layer);
  text_layer_destroy(s_hr_layer);
  text_layer_destroy(s_status_layer);
  if (s_map_buffer) free(s_map_buffer);
  layer_destroy(s_map_layer);
  text_layer_destroy(s_map_speed_layer);
  text_layer_destroy(s_map_distance_layer);
  text_layer_destroy(s_map_hr_layer);
  text_layer_destroy(s_map_status_layer);
  window_destroy(window);
  s_window = NULL;
}

void workout_window_push(ActivityType type, AppSettings *settings) {
  s_activity_type = type;
  s_settings = *settings;
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_window_load, .unload = prv_window_unload });
  window_stack_push(s_window, true);
}

void workout_window_destroy(void) {
  if (s_window) window_destroy(s_window);
}
