#include "settings.h"

#define PERSIST_KEY_SETTINGS 1

void settings_defaults(AppSettings *s) {
  s->auto_pause_enabled         = true;
  s->auto_pause_threshold_run   = 5;
  s->auto_pause_threshold_bike  = 10;
}

void settings_load(AppSettings *s) {
  settings_defaults(s);
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    persist_read_data(PERSIST_KEY_SETTINGS, s, sizeof(AppSettings));
  }
}

void settings_save(const AppSettings *s) {
  persist_write_data(PERSIST_KEY_SETTINGS, s, sizeof(AppSettings));
}