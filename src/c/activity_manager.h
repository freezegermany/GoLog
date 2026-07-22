#pragma once

#include <pebble.h>
#include <math.h>
#include "activity_types.h"

typedef enum {
  GPS_STATUS_SEARCHING = 0,
  GPS_STATUS_FIXED = 1,
  GPS_STATUS_LOST = 2
} GpsStatus;

void activity_manager_start(ActivityType activity_type);
void activity_manager_manual_toggle_pause(void);
void activity_manager_stop(void);
void activity_manager_discard(void);
void activity_manager_tick_second(void);
void activity_manager_set_hr_bpm(int bpm);
void activity_manager_set_gps_status(GpsStatus status);
GpsStatus activity_manager_get_gps_status(void);
void activity_manager_process_gps_fix(double lat, double lon, double raw_altitude_m, double speed_kmh);

ActivityState activity_manager_get_state(void);
ActivityType activity_manager_get_activity_type(void);
uint32_t activity_manager_get_elapsed_seconds(void);
double activity_manager_get_distance_m(void);
int activity_manager_get_hr_bpm(void);
double activity_manager_get_average_pace_sec_per_km(void);
double activity_manager_get_elevation_gain_m(void);
double activity_manager_get_elevation_loss_m(void);

void activity_manager_persist_snapshot(void);
void activity_manager_clear_snapshot(void);
bool activity_manager_has_recoverable_snapshot(void);
void activity_manager_restore_from_snapshot(void);