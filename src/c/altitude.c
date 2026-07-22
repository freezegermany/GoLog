#include "altitude.h"
#include <string.h>
#define ALTITUDE_WINDOW_SIZE 8
#define ALTITUDE_GAIN_LOSS_DEADBAND 1.0
static double s_window[ALTITUDE_WINDOW_SIZE];
static int s_window_count = 0;
static int s_window_index = 0;
static double s_last_smoothed = 0.0;
static double s_total_gain = 0.0;
static double s_total_loss = 0.0;
static bool s_has_previous = false;
static double s_previous_smoothed = 0.0;

void altitude_tracker_reset(void) {
  memset(s_window, 0, sizeof(s_window));
  s_window_count = 0;
  s_window_index = 0;
  s_last_smoothed = 0.0;
  s_total_gain = 0.0;
  s_total_loss = 0.0;
  s_has_previous = false;
  s_previous_smoothed = 0.0;
}

double altitude_tracker_add_sample(double raw_altitude_m) {
  s_window[s_window_index] = raw_altitude_m;
  s_window_index = (s_window_index + 1) % ALTITUDE_WINDOW_SIZE;
  if (s_window_count < ALTITUDE_WINDOW_SIZE) s_window_count++;
  double sum = 0.0;
  for (int i = 0; i < s_window_count; i++) sum += s_window[i];
  double smoothed = sum / (double)s_window_count;
  s_last_smoothed = smoothed;
  if (s_has_previous) {
    double delta = smoothed - s_previous_smoothed;
    if (delta > ALTITUDE_GAIN_LOSS_DEADBAND) s_total_gain += delta;
    else if (delta < -ALTITUDE_GAIN_LOSS_DEADBAND) s_total_loss += -delta;
  }
  s_previous_smoothed = smoothed;
  s_has_previous = true;
  return smoothed;
}

double altitude_tracker_get_smoothed(void) { return s_last_smoothed; }
double altitude_tracker_get_total_gain(void) { return s_total_gain; }
double altitude_tracker_get_total_loss(void) { return s_total_loss; }
