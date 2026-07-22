#include "units.h"

static bool s_is_metric = true;
#define KM_TO_MILES 0.621371
#define SEC_PER_KM_TO_SEC_PER_MILE(x) ((x) / KM_TO_MILES)

void units_set_metric(bool is_metric) { s_is_metric = is_metric; }
bool units_is_metric(void) { return s_is_metric; }

double units_convert_distance(double distance_m) {
  double distance_km = distance_m / 1000.0;
  return s_is_metric ? distance_km : distance_km * KM_TO_MILES;
}

void units_format_pace(double pace_sec_per_km, char *out_buf, size_t buf_size) {
  double pace_sec = s_is_metric ? pace_sec_per_km : SEC_PER_KM_TO_SEC_PER_MILE(pace_sec_per_km);
  int minutes = (int)(pace_sec / 60);
  int seconds = (int)pace_sec % 60;
  if (pace_sec_per_km <= 0.0) {
    snprintf(out_buf, buf_size, "--:-- /%s", s_is_metric ? "km" : "mi");
    return;
  }
  snprintf(out_buf, buf_size, "%d:%02d /%s", minutes, seconds, s_is_metric ? "km" : "mi");
}
