#include "sync_confirm_dialog.h"
#include "../theme.h"
#include "../appmessage_protocol.h"
#include "main_menu.h"
static Window *s_window; static TextLayer *s_prompt_layer; static TextLayer *s_hint_layer; static TextLayer *s_status_layer;
static void sync_status_received(int status) { const char *text = status == 1 ? "Synced!" : status == 2 ? "Queued (offline)" : "Sync failed"; text_layer_set_text(s_status_layer, text); }
static void confirm_sync(ClickRecognizerRef recognizer, void *context) { app_message_protocol_send_sync_request(true); text_layer_set_text(s_status_layer, "Syncing..."); }
static void decline_sync(ClickRecognizerRef recognizer, void *context) { window_stack_pop_all(true); main_menu_window_init(); }
static void click_config_provider(void *context) { window_single_click_subscribe(BUTTON_ID_SELECT, confirm_sync); window_single_click_subscribe(BUTTON_ID_BACK, decline_sync); }
static void window_load(Window *window) {
  golog_style_window(window); Layer *root_layer = window_get_root_layer(window); GRect bounds = layer_get_bounds(root_layer);
  s_prompt_layer = text_layer_create(GRect(0, 20, bounds.size.w, 50)); text_layer_set_font(s_prompt_layer, GOLOG_FONT_MEDIUM); golog_style_text_layer(s_prompt_layer); text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter); text_layer_set_text(s_prompt_layer, "Sync to Strava?"); layer_add_child(root_layer, text_layer_get_layer(s_prompt_layer));
  s_status_layer = text_layer_create(GRect(0, 75, bounds.size.w, 30)); text_layer_set_font(s_status_layer, GOLOG_FONT_SMALL); golog_style_text_layer(s_status_layer); text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter); layer_add_child(root_layer, text_layer_get_layer(s_status_layer));
  s_hint_layer = text_layer_create(GRect(0, 115, bounds.size.w, 40)); text_layer_set_font(s_hint_layer, GOLOG_FONT_SMALL); golog_style_text_layer(s_hint_layer); text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter); text_layer_set_text(s_hint_layer, "Select: Sync\nBack: Skip"); layer_add_child(root_layer, text_layer_get_layer(s_hint_layer));
  app_message_protocol_set_sync_status_handler(sync_status_received);
}
static void window_unload(Window *window) { text_layer_destroy(s_prompt_layer); text_layer_destroy(s_status_layer); text_layer_destroy(s_hint_layer); window_destroy(s_window); }
void sync_confirm_dialog_push(void) { s_window = window_create(); window_set_click_config_provider(s_window, click_config_provider); window_set_window_handlers(s_window, (WindowHandlers){ .load = window_load, .unload = window_unload }); window_stack_push(s_window, true); }
