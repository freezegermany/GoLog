#include <pebble.h>
#include "activity_select.h"
#include "workout_window.h"

static void prv_init(void) {
  activity_select_push();
}

static void prv_deinit(void) {
  workout_window_destroy();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}