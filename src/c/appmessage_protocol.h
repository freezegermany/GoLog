#pragma once
#include <pebble.h>
#include "activity_types.h"
typedef enum {
  MSG_KEY_EVENT_TYPE = 0, MSG_KEY_ACTIVITY_TYPE, MSG_KEY_GPS_STATUS, MSG_KEY_LAT, MSG_KEY_LON, MSG_KEY_ALT_RAW, MSG_KEY_ALT_SMOOTH, MSG_KEY_SPEED, MSG_KEY_DURATION, MSG_KEY_DISTANCE, MSG_KEY_PACE, MSG_KEY_HR_BPM, MSG_KEY_AUTOPAUSE_RUN_THRESHOLD, MSG_KEY_AUTOPAUSE_CYCLE_THRESHOLD, MSG_KEY_UNITS_METRIC, MSG_KEY_GPX_EXPORT_ENABLED, MSG_KEY_ACTIVITY_COMMAND, MSG_KEY_SYNC_REQUEST, MSG_KEY_SYNC_STATUS,
} MessageKey;
typedef enum { EVENT_TYPE_GPS_FIX = 1, EVENT_TYPE_SETTINGS_SYNC = 2, EVENT_TYPE_SYNC_STATUS = 3 } PhoneEventType;
typedef enum { ACTIVITY_COMMAND_START = 1, ACTIVITY_COMMAND_STOP = 2, ACTIVITY_COMMAND_DISCARD = 3, ACTIVITY_COMMAND_OPEN_STRAVA_CONFIG = 4 } ActivityCommand;
void app_message_protocol_init(void);
void app_message_protocol_send_command(ActivityCommand command, ActivityType activity_type);
void app_message_protocol_request_strava_config(void);
void app_message_protocol_send_sync_request(bool user_confirmed);
void app_message_protocol_send_settings_update(double run_threshold_kmh, double cycle_threshold_kmh, bool units_metric, bool gpx_export_enabled);
typedef void (*GpsFixHandler)(double lat, double lon, double alt_raw, double speed_kmh);
typedef void (*SyncStatusHandler)(int status);
void app_message_protocol_set_gps_fix_handler(GpsFixHandler handler);
void app_message_protocol_set_sync_status_handler(SyncStatusHandler handler);
