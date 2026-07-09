#include "settings_window.h"
#include "settings.h"
#include <pebble.h>

#define NUM_ROWS 3

static Window      *s_window;
static MenuLayer   *s_menu_layer;
static AppSettings *s_settings;

static char s_run_buf[24];
static char s_bike_buf[24];

static void prv_update_bufs(void) {
  snprintf(s_run_buf,  sizeof(s_run_buf),
    "Run: %d.%d km/h",
    s_settings->auto_pause_threshold_run  / 10,
    s_settings->auto_pause_threshold_run  % 10);
  snprintf(s_bike_buf, sizeof(s_bike_buf),
    "Bike: %d.%d km/h",
    s_settings->auto_pause_threshold_bike / 10,
    s_settings->auto_pause_threshold_bike % 10);
}

static uint16_t prv_num_rows(MenuLayer *ml, uint16_t sec, void *ctx) {
  (void)ml; (void)sec; (void)ctx;
  return NUM_ROWS;
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *index, void *context) {
  (void)context;
  prv_update_bufs();
  switch (index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Auto-Pause",
        s_settings->auto_pause_enabled ? "ON" : "OFF", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Pause below (Run)", s_run_buf, NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Pause below (Bike)", s_bike_buf, NULL);
      break;
  }
}

static void prv_select_click(MenuLayer *ml, MenuIndex *index, void *ctx) {
  (void)ml; (void)ctx;
  switch (index->row) {
    case 0:
      s_settings->auto_pause_enabled = !s_settings->auto_pause_enabled;
      break;
    case 1: {
      uint8_t steps[] = {3, 5, 8, 10, 15, 20};
      uint8_t n = 6, cur = s_settings->auto_pause_threshold_run, next = steps[0];
      for (uint8_t i = 0; i < n; i++)
        if (steps[i] == cur) { next = steps[(i+1)%n]; break; }
      s_settings->auto_pause_threshold_run = next;
      break;
    }
    case 2: {
      uint8_t steps[] = {3, 5, 8, 10, 15, 20};
      uint8_t n = 6, cur = s_settings->auto_pause_threshold_bike, next = steps[0];
      for (uint8_t i = 0; i < n; i++)
        if (steps[i] == cur) { next = steps[(i+1)%n]; break; }
      s_settings->auto_pause_threshold_bike = next;
      break;
    }
  }
  settings_save(s_settings);
  menu_layer_reload_data(s_menu_layer);
}

static void prv_window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);
  s_menu_layer  = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_num_rows,
    .draw_row     = prv_draw_row,
    .select_click = prv_select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  window_destroy(window);
  s_window = NULL;
}

void settings_window_push(AppSettings *settings) {
  s_settings = settings;
  s_window   = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}