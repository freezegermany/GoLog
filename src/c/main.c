#include <pebble.h>
#include "appmessage_protocol.h"
#include "windows/main_menu.h"
static void init(void) { app_message_protocol_init(); main_menu_window_init(); }
static void deinit(void) { window_stack_pop_all(false); }
int main(void) { init(); app_event_loop(); deinit(); }
