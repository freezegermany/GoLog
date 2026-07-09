#include "activity_select.h"
#include "workout_window.h"
#include "settings_window.h"
#include "settings.h"
#include <pebble.h>
#include <math.h>

// Rows: 0=Running, 1=Cycling, 2=Settings
#define NUM_ROWS 3

static Window *s_window;
static MenuLayer *s_menu_layer;
static AppSettings s_settings;

// Bluetooth warning layer (shown when disconnected)
static TextLayer *s_bt_warning_layer;
static bool s_bt_connected = false;

static void prv_update_bt_warning(void) {
  if (s_bt_warning_layer) {
    layer_set_hidden(text_layer_get_layer(s_bt_warning_layer), s_bt_connected);
  }
  if (s_menu_layer) {
    layer_set_hidden(menu_layer_get_layer(s_menu_layer), !s_bt_connected);
  }
}

static void prv_bt_handler(bool connected) {
  s_bt_connected = connected;
  prv_update_bt_warning();
  if (!connected) {
    vibes_short_pulse();
  }
}

static int16_t prv_row_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml; (void)idx; (void)ctx;
  return 56;
}

static uint16_t prv_num_rows(MenuLayer *ml, uint16_t sec, void *ctx) {
  (void)ml; (void)sec; (void)ctx;
  return NUM_ROWS;
}

static void prv_draw_icon(GContext *ctx, GRect b, int row) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  GPoint c = GPoint(b.origin.x + 14, b.origin.y + b.size.h / 2);
  if (row == 0) {
    graphics_fill_circle(ctx, c, 3);
    graphics_draw_line(ctx, GPoint(c.x, c.y + 3), GPoint(c.x, c.y + 12));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 6), GPoint(c.x - 6, c.y + 10));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 6), GPoint(c.x + 6, c.y + 8));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 12), GPoint(c.x - 5, c.y + 18));
    graphics_draw_line(ctx, GPoint(c.x, c.y + 12), GPoint(c.x + 6, c.y + 18));
  } else if (row == 1) {
    graphics_draw_circle(ctx, GPoint(c.x - 7, c.y + 7), 5);
    graphics_draw_circle(ctx, GPoint(c.x + 8, c.y + 7), 5);
    graphics_draw_line(ctx, GPoint(c.x - 7, c.y + 7), GPoint(c.x, c.y));
    graphics_draw_line(ctx, GPoint(c.x, c.y), GPoint(c.x + 6, c.y + 1));
    graphics_draw_line(ctx, GPoint(c.x + 6, c.y + 1), GPoint(c.x + 8, c.y + 7));
    graphics_draw_line(ctx, GPoint(c.x, c.y), GPoint(c.x - 2, c.y + 8));
  } else {
    graphics_draw_circle(ctx, c, 7);
    for (int i = 0; i < 8; ++i) {
      int32_t a = TRIG_MAX_ANGLE * i / 8;
      int16_t x1 = c.x + sin_lookup(a) * 9 / TRIG_MAX_RATIO;
      int16_t y1 = c.y - cos_lookup(a) * 9 / TRIG_MAX_RATIO;
      int16_t x2 = c.x + sin_lookup(a) * 12 / TRIG_MAX_RATIO;
      int16_t y2 = c.y - cos_lookup(a) * 12 / TRIG_MAX_RATIO;
      graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
    }
  }
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *ctx2) {
  (void)ctx2;
  bool hl = menu_cell_layer_is_highlighted(cell_layer);
  graphics_context_set_fill_color(ctx, hl ? GColorWhite : GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(cell_layer), 0, GCornerNone);
  graphics_context_set_text_color(ctx, hl ? GColorBlack : GColorWhite);
  graphics_context_set_stroke_color(ctx, hl ? GColorBlack : GColorWhite);
  graphics_context_set_fill_color(ctx, hl ? GColorBlack : GColorWhite);
  prv_draw_icon(ctx, layer_get_bounds(cell_layer), idx->row);
  const char *title = idx->row == 0 ? "Running" : (idx->row == 1 ? "Cycling" : "Settings");
  graphics_draw_text(ctx, title,
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(38, 8, 120, 40),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);
}

static void prv_select_click(MenuLayer *ml, MenuIndex *index, void *ctx) {
  (void)ml; (void)ctx;
  if (!s_bt_connected) return;
  switch (index->row) {
    case 0: workout_window_push(ACTIVITY_RUNNING, &s_settings); break;
    case 1: workout_window_push(ACTIVITY_CYCLING, &s_settings); break;
    case 2: settings_window_push(&s_settings); break;
  }
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_bt_connected = connection_service_peek_pebble_app_connection();

  s_bt_warning_layer = text_layer_create(GRect(0, 0, bounds.size.w, 30));
  text_layer_set_background_color(s_bt_warning_layer, GColorRed);
  text_layer_set_text_color(s_bt_warning_layer, GColorWhite);
  text_layer_set_font(s_bt_warning_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_bt_warning_layer, GTextAlignmentCenter);
  text_layer_set_text(s_bt_warning_layer, "No phone connection!");
  layer_add_child(root, text_layer_get_layer(s_bt_warning_layer));

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
    .get_cell_height = prv_row_height,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  prv_update_bt_warning();

  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = prv_bt_handler,
  });
}

static void prv_window_unload(Window *window) {
  (void)window;
  connection_service_unsubscribe();
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_bt_warning_layer);
  window_destroy(window);
  s_window = NULL;
}

void activity_select_push(void) {
  settings_load(&s_settings);
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}
