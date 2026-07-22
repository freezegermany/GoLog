#include "stop_confirm_dialog.h"
#include "../theme.h"
#include "../activity_manager.h"
#include "../appmessage_protocol.h"
#include "sync_confirm_dialog.h"
static Window *s_window; static TextLayer *s_prompt_layer; static TextLayer *s_hint_layer;
static void confirm_stop(ClickRecognizerRef recognizer, void *context) { activity_manager_stop(); app_message_protocol_send_command(ACTIVITY_COMMAND_STOP, activity_manager_get_activity_type()); window_stack_pop(true); sync_confirm_dialog_push(); }
static void cancel_stop(ClickRecognizerRef recognizer, void *context) { window_stack_pop(true); }
static void click_config_provider(void *context) { window_single_click_subscribe(BUTTON_ID_SELECT, confirm_stop); window_single_click_subscribe(BUTTON_ID_BACK, cancel_stop); }
static void window_load(Window *window) {
  golog_style_window(window); Layer *root_layer = window_get_root_layer(window); GRect bounds = layer_get_bounds(root_layer);
  s_prompt_layer = text_layer_create(GRect(0, 40, bounds.size.w, 50)); text_layer_set_font(s_prompt_layer, GOLOG_FONT_MEDIUM); golog_style_text_layer(s_prompt_layer); text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter); text_layer_set_text(s_prompt_layer, "Stop activity?"); layer_add_child(root_layer, text_layer_get_layer(s_prompt_layer));
  s_hint_layer = text_layer_create(GRect(0, 100, bounds.size.w, 50)); text_layer_set_font(s_hint_layer, GOLOG_FONT_SMALL); golog_style_text_layer(s_hint_layer); text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter); text_layer_set_text(s_hint_layer, "Select: Stop\nBack: Cancel"); layer_add_child(root_layer, text_layer_get_layer(s_hint_layer));
}
static void window_unload(Window *window) { text_layer_destroy(s_prompt_layer); text_layer_destroy(s_hint_layer); window_destroy(s_window); }
void stop_confirm_dialog_push(void) { s_window = window_create(); window_set_click_config_provider(s_window, click_config_provider); window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload }); window_stack_push(s_window, true); }
