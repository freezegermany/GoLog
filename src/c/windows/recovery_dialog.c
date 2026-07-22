#include "recovery_dialog.h"
#include "../theme.h"
#include "../activity_manager.h"
#include "activity_window.h"

static Window *s_window;
static TextLayer *s_prompt_layer;
static TextLayer *s_hint_layer;

static void resume_activity(ClickRecognizerRef recognizer, void *context) {
  activity_manager_restore_from_snapshot();
  window_stack_remove(s_window, true);
  activity_window_push(activity_manager_get_activity_type());
}

static void discard_activity(ClickRecognizerRef recognizer, void *context) {
  activity_manager_discard();
  window_stack_remove(s_window, true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, resume_activity);
  window_single_click_subscribe(BUTTON_ID_BACK, discard_activity);
}

static void window_load(Window *window) {
  golog_style_window(window);

  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  s_prompt_layer = text_layer_create(GRect(0, 30, bounds.size.w, 60));
  text_layer_set_font(s_prompt_layer, GOLOG_FONT_MEDIUM);
  golog_style_text_layer(s_prompt_layer);
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text(s_prompt_layer, "Unsaved activity found. Resume?");
  layer_add_child(root_layer, text_layer_get_layer(s_prompt_layer));

  s_hint_layer = text_layer_create(GRect(0, 110, bounds.size.w, 40));
  text_layer_set_font(s_hint_layer, GOLOG_FONT_SMALL);
  golog_style_text_layer(s_hint_layer);
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, "Select: Resume\nBack: Discard");
  layer_add_child(root_layer, text_layer_get_layer(s_hint_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_prompt_layer);
  text_layer_destroy(s_hint_layer);
  s_prompt_layer = NULL;
  s_hint_layer = NULL;
  s_window = NULL;
}

void recovery_dialog_push(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}