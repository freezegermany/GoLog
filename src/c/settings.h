#pragma once
#include <pebble.h>

typedef struct {
  bool    auto_pause_enabled;
  uint8_t auto_pause_threshold_run;   // km/h x10, default 5 = 0.5 km/h
  uint8_t auto_pause_threshold_bike;  // km/h x10, default 10 = 1.0 km/h
} AppSettings;

void settings_load(AppSettings *s);
void settings_save(const AppSettings *s);
void settings_defaults(AppSettings *s);