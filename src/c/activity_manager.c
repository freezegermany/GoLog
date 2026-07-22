#include "activity_manager.h"
#include "altitude.h"
#include "autopause.h"
#include "vibration.h"

#include <string.h>

#define PERSIST_KEY_STATE 100
#define PERSIST_KEY_ACTIVITY_TYPE 101
#define PERSIST_KEY_START_TIME 102
#define PERSIST_KEY_ELAPSED_SECONDS 103
#define PERSIST_KEY_DISTANCE_M 104

static ActivityState s_state = ACTIVITY_STATE_IDLE;
static ActivityType s_activity_type = ACTIVITY_TYPE_RUNNING;
static time_t s_start_epoch = 0;
static uint32_t s_elapsed_seconds = 0;
static double s_distance_m = 0.0;
static double s_last_lat = 0.0;
static double s_last_lon = 0.0;
static bool s_has_last_position = false;
static int s_hr_bpm = 0;
static GpsStatus s_gps_status = GPS_STATUS_SEARCHING;

#define EARTH_RADIUS_M 6371000.0
#define DEG_TO_RAD 0.017453292519943295

static double approx_distance_m(double lat1, double lon1, double lat2, double lon2) {
  double avg_lat_rad = (lat1 + lat2) * 0.5 * DEG_TO_RAD;
  double dx = (lon2 - lon1) * DEG_TO_RAD * EARTH_RADIUS_M * cos(avg_lat_rad);
  double dy = (lat2 - lat1) * DEG_TO_RAD * EARTH_RADIUS_M;
  return sqrt(dx * dx + dy * dy);
}

void activity_manager_persist_snapshot(void) {
  persist_write_int(PERSIST_KEY_STATE, (int)s_state);
  persist_write_int(PERSIST_KEY_ACTIVITY_TYPE, (int)s_activity_type);
  persist_write_int(PERSIST_KEY_START_TIME, (int)s_start_epoch);
  persist_write_int(PERSIST_KEY_ELAPSED_SECONDS, (int)s_elapsed_seconds);
  persist_write_data(PERSIST_KEY_DISTANCE_M, &s_distance_m, sizeof(double));
}

void activity_manager_clear_snapshot(void) {
  persist_delete(PERSIST_KEY_STATE);
  persist_delete(PERSIST_KEY_ACTIVITY_TYPE);
  persist_delete(PERSIST_KEY_START_TIME);
  persist_delete(PERSIST_KEY_ELAPSED_SECONDS);
  persist_delete(PERSIST_KEY_DISTANCE_M);
}

bool activity_manager_has_recoverable_snapshot(void) {
  if (!persist_exists(PERSIST_KEY_STATE)) {
    return false;
  }

  ActivityState saved_state = (ActivityState)persist_read_int(PERSIST_KEY_STATE);

  return saved_state == ACTIVITY_STATE_RUNNING ||
         saved_state == ACTIVITY_STATE_AUTOPAUSED ||
         saved_state == ACTIVITY_STATE_MANUAL_PAUSED ||
         saved_state == ACTIVITY_STATE_STOPPED;
}

void activity_manager_restore_from_snapshot(void) {
  s_state = (ActivityState)persist_read_int(PERSIST_KEY_STATE);
  s_activity_type = (ActivityType)persist_read_int(PERSIST_KEY_ACTIVITY_TYPE);
  s_start_epoch = (time_t)persist_read_int(PERSIST_KEY_START_TIME);
  s_elapsed_seconds = (uint32_t)persist_read_int(PERSIST_KEY_ELAPSED_SECONDS);
  persist_read_data(PERSIST_KEY_DISTANCE_M, &s_distance_m, sizeof(double));

  s_has_last_position = false;
  s_hr_bpm = 0;
  s_gps_status = GPS_STATUS_SEARCHING;

  altitude_tracker_reset();
  autopause_reset();
}

void activity_manager_start(ActivityType activity_type) {
  s_activity_type = activity_type;
  s_state = ACTIVITY_STATE_RUNNING;
  s_start_epoch = time(NULL);
  s_elapsed_seconds = 0;
  s_distance_m = 0.0;
  s_last_lat = 0.0;
  s_last_lon = 0.0;
  s_has_last_position = false;
  s_hr_bpm = 0;
  s_gps_status = GPS_STATUS_SEARCHING;

  altitude_tracker_reset();
  autopause_reset();

  activity_manager_persist_snapshot();
  vibration_feedback_manual_toggle();
}

void activity_manager_manual_toggle_pause(void) {
  if (s_state == ACTIVITY_STATE_RUNNING || s_state == ACTIVITY_STATE_AUTOPAUSED) {
    s_state = ACTIVITY_STATE_MANUAL_PAUSED;
  } else if (s_state == ACTIVITY_STATE_MANUAL_PAUSED) {
    s_state = ACTIVITY_STATE_RUNNING;
    autopause_reset();
  }

  vibration_feedback_manual_toggle();
  activity_manager_persist_snapshot();
}

void activity_manager_stop(void) {
  s_state = ACTIVITY_STATE_STOPPED;
  activity_manager_persist_snapshot();
  vibration_feedback_manual_toggle();
}

void activity_manager_discard(void) {
  s_state = ACTIVITY_STATE_IDLE;
  s_activity_type = ACTIVITY_TYPE_RUNNING;
  s_start_epoch = 0;
  s_elapsed_seconds = 0;
  s_distance_m = 0.0;
  s_last_lat = 0.0;
  s_last_lon = 0.0;
  s_has_last_position = false;
  s_hr_bpm = 0;
  s_gps_status = GPS_STATUS_SEARCHING;

  altitude_tracker_reset();
  autopause_reset();

  activity_manager_clear_snapshot();
}

void activity_manager_tick_second(void) {
  if (s_state == ACTIVITY_STATE_RUNNING) {
    s_elapsed_seconds++;
    activity_manager_persist_snapshot();
  }
}

void activity_manager_set_hr_bpm(int bpm) {
  s_hr_bpm = bpm;
}

void activity_manager_set_gps_status(GpsStatus status) {
  s_gps_status = status;
}

GpsStatus activity_manager_get_gps_status(void) {
  return s_gps_status;
}

void activity_manager_process_gps_fix(double lat, double lon, double raw_altitude_m, double speed_kmh) {
  if (s_state != ACTIVITY_STATE_RUNNING && s_state != ACTIVITY_STATE_AUTOPAUSED) {
    return;
  }

  s_gps_status = GPS_STATUS_FIXED;
  altitude_tracker_add_sample(raw_altitude_m);

  bool is_now_paused = false;
  bool transitioned = autopause_evaluate(s_activity_type, speed_kmh, &is_now_paused);

  if (transitioned) {
    if (is_now_paused) {
      s_state = ACTIVITY_STATE_AUTOPAUSED;
      vibration_feedback_autopause_start();
    } else {
      s_state = ACTIVITY_STATE_RUNNING;
      vibration_feedback_autopause_end();
    }
  }

  if (!is_now_paused && s_has_last_position) {
    s_distance_m += approx_distance_m(s_last_lat, s_last_lon, lat, lon);
  }

  s_last_lat = lat;
  s_last_lon = lon;
  s_has_last_position = true;

  activity_manager_persist_snapshot();
}

ActivityState activity_manager_get_state(void) {
  return s_state;
}

ActivityType activity_manager_get_activity_type(void) {
  return s_activity_type;
}

uint32_t activity_manager_get_elapsed_seconds(void) {
  return s_elapsed_seconds;
}

double activity_manager_get_distance_m(void) {
  return s_distance_m;
}

int activity_manager_get_hr_bpm(void) {
  return s_hr_bpm;
}

double activity_manager_get_average_pace_sec_per_km(void) {
  double distance_km = s_distance_m / 1000.0;
  if (distance_km <= 0.0001) {
    return 0.0;
  }
  return (double)s_elapsed_seconds / distance_km;
}

double activity_manager_get_elevation_gain_m(void) {
  return altitude_tracker_get_total_gain();
}

double activity_manager_get_elevation_loss_m(void) {
  return altitude_tracker_get_total_loss();
}