#include "settings_window.h"
#include "../theme.h"
#include "../autopause.h"
#include "../units.h"
#include "../appmessage_protocol.h"

static Window *s_window;
static MenuLayer *s_menu_layer;
static bool s_gpx_export_enabled = false;

#define SETTINGS_NUM_ROWS 5
#define THRESHOLD_STEP_KMH 0.5

static void format_threshold_kmh(double value, char *buf, size_t size) {
  int whole = (int)value;
  int frac = (int)((value - whole) * 10.0 + 0.5);

  if (frac >= 10) {
    whole += 1;
    frac = 0;
  }

  snprintf(buf, size, "%d.%d km/h", whole, frac);
}

static void push_settings_to_phone(void) {
  app_message_protocol_send_settings_update(
    autopause_get_run_threshold_kmh(),
    autopause_get_cycle_threshold_kmh(),
    units_is_metric(),
    s_gpx_export_enabled
  );
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return SETTINGS_NUM_ROWS;
}

static void menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  static char buf[32];

  switch (cell_index->row) {
    case 0:
      format_threshold_kmh(autopause_get_run_threshold_kmh(), buf, sizeof(buf));
      menu_cell_basic_draw(ctx, cell_layer, "Autopause Running", buf, NULL);
      break;

    case 1:
      format_threshold_kmh(autopause_get_cycle_threshold_kmh(), buf, sizeof(buf));
      menu_cell_basic_draw(ctx, cell_layer, "Autopause Cycling", buf, NULL);
      break;

    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Units", units_is_metric() ? "Metric" : "Imperial", NULL);
      break;

    case 3:
      menu_cell_basic_draw(ctx, cell_layer, "Local GPX export", s_gpx_export_enabled ? "On" : "Off", NULL);
      break;

    case 4:
      menu_cell_basic_draw(ctx, cell_layer, "Connect to Strava", "Opens on phone", NULL);
      break;
  }
}

static int16_t menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return 50;
}

static void menu_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 2:
      units_set_metric(!units_is_metric());
      break;

    case 3:
      s_gpx_export_enabled = !s_gpx_export_enabled;
      break;

    case 4:
      app_message_protocol_request_strava_config();
      return;

    default:
      break;
  }

  push_settings_to_phone();
  menu_layer_reload_data(s_menu_layer);
}

static void adjust_threshold(uint16_t row, double delta) {
  if (row == 0) {
    double v = autopause_get_run_threshold_kmh() + delta;
    if (v < 0.5) {
      v = 0.5;
    }
    autopause_set_run_threshold_kmh(v);
  } else if (row == 1) {
    double v = autopause_get_cycle_threshold_kmh() + delta;
    if (v < 0.5) {
      v = 0.5;
    }
    autopause_set_cycle_threshold_kmh(v);
  }

  push_settings_to_phone();
  menu_layer_reload_data(s_menu_layer);
}

static void menu_up_click(ClickRecognizerRef recognizer, void *context) {
  MenuIndex idx = menu_layer_get_selected_index(s_menu_layer);

  if (idx.row == 0 || idx.row == 1) {
    adjust_threshold(idx.row, THRESHOLD_STEP_KMH);
  } else {
    menu_layer_set_selected_next(s_menu_layer, true, MenuRowAlignCenter, true);
  }
}

static void menu_down_click(ClickRecognizerRef recognizer, void *context) {
  MenuIndex idx = menu_layer_get_selected_index(s_menu_layer);

  if (idx.row == 0 || idx.row == 1) {
    adjust_threshold(idx.row, -THRESHOLD_STEP_KMH);
  } else {
    menu_layer_set_selected_next(s_menu_layer, false, MenuRowAlignCenter, true);
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, menu_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, menu_down_click);
}

static void window_load(Window *window) {
  golog_style_window(window);

  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = menu_get_num_rows,
    .draw_row = menu_draw_row,
    .get_cell_height = menu_get_cell_height,
    .select_click = menu_select_click,
  });

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  menu_layer_set_normal_colors(s_menu_layer, GOLOG_COLOR_BACKGROUND, GOLOG_COLOR_FOREGROUND);
  menu_layer_set_highlight_colors(s_menu_layer, GOLOG_COLOR_ACCENT, GOLOG_COLOR_BACKGROUND);
  layer_add_child(root_layer, menu_layer_get_layer(s_menu_layer));

  window_set_click_config_provider(window, click_config_provider);
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
}

void settings_window_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}