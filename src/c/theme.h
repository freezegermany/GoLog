#pragma once
#include <pebble.h>

#define GOLOG_COLOR_BACKGROUND GColorBlack
#define GOLOG_COLOR_FOREGROUND GColorWhite
#define GOLOG_COLOR_ACCENT GColorLightGray

#define GOLOG_FONT_LARGE fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49)
#define GOLOG_FONT_MEDIUM fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21)
#define GOLOG_FONT_SMALL fonts_get_system_font(FONT_KEY_GOTHIC_18)

static inline void golog_style_window(Window *window) {
  window_set_background_color(window, GOLOG_COLOR_BACKGROUND);
}

static inline void golog_style_text_layer(TextLayer *layer) {
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GOLOG_COLOR_FOREGROUND);
}
