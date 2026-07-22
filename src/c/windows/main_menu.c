#include "main_menu.h"
#include "../theme.h"
#include "activity_window.h"
#include "settings_window.h"
#include "../activity_manager.h"
#include "recovery_dialog.h"

static Window *s_window;
static MenuLayer *s_menu_layer;
static AppTimer *s_recovery_timer = NULL;

static const char *s_menu_titles[] = {
  "Running",
  "Cycling",
  "Settings"
};

#define MENU_NUM_ITEMS 3

static void show_recovery_dialog(void *data) {
  s_recovery_timer = NULL;
  recovery_dialog_push();
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return MENU_NUM_ITEMS;
}

static void menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  graphics_context_set_text_color(ctx, GOLOG_COLOR_FOREGROUND);
  menu_cell_basic_draw(ctx, cell_layer, s_menu_titles[cell_index->row], NULL, NULL);
}

static int16_t menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  return 44;
}

static void menu_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      activity_window_push(ACTIVITY_TYPE_RUNNING);
      break;
    case 1:
      activity_window_push(ACTIVITY_TYPE_CYCLING);
      break;
    case 2:
      settings_window_push();
      break;
  }
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

  if (activity_manager_has_recoverable_snapshot()) {
    s_recovery_timer = app_timer_register(100, show_recovery_dialog, NULL);
  }
}

static void window_unload(Window *window) {
  if (s_recovery_timer) {
    app_timer_cancel(s_recovery_timer);
    s_recovery_timer = NULL;
  }

  menu_layer_destroy(s_menu_layer);
}

void main_menu_window_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}