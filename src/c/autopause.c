#include "autopause.h"
#define DEFAULT_RUN_THRESHOLD_KMH 3.0
#define DEFAULT_CYCLE_THRESHOLD_KMH 5.0
static double s_run_threshold_kmh = DEFAULT_RUN_THRESHOLD_KMH;
static double s_cycle_threshold_kmh = DEFAULT_CYCLE_THRESHOLD_KMH;
static bool s_is_paused = false;
void autopause_set_run_threshold_kmh(double threshold_kmh) { s_run_threshold_kmh = threshold_kmh; }
void autopause_set_cycle_threshold_kmh(double threshold_kmh) { s_cycle_threshold_kmh = threshold_kmh; }
double autopause_get_run_threshold_kmh(void) { return s_run_threshold_kmh; }
double autopause_get_cycle_threshold_kmh(void) { return s_cycle_threshold_kmh; }
bool autopause_evaluate(ActivityType activity_type, double speed_kmh, bool *out_is_now_paused) {
  double threshold = (activity_type == ACTIVITY_TYPE_CYCLING) ? s_cycle_threshold_kmh : s_run_threshold_kmh;
  bool should_be_paused = speed_kmh < threshold;
  bool transitioned = (should_be_paused != s_is_paused);
  s_is_paused = should_be_paused;
  if (out_is_now_paused) *out_is_now_paused = s_is_paused;
  return transitioned;
}
bool autopause_is_paused(void) { return s_is_paused; }
void autopause_reset(void) { s_is_paused = false; }
