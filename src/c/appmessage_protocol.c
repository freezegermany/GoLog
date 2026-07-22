#include "appmessage_protocol.h"

static GpsFixHandler s_gps_fix_handler = NULL;
static SyncStatusHandler s_sync_status_handler = NULL;

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  Tuple *event_type_tuple = dict_find(iterator, MSG_KEY_EVENT_TYPE);
  if (!event_type_tuple) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: inbox received without EVENT_TYPE");
    return;
  }

  int event_type = (int)event_type_tuple->value->int32;
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: inbox event_type=%d", event_type);

  if (event_type == EVENT_TYPE_GPS_FIX) {
    Tuple *lat_t = dict_find(iterator, MSG_KEY_LAT);
    Tuple *lon_t = dict_find(iterator, MSG_KEY_LON);
    Tuple *alt_t = dict_find(iterator, MSG_KEY_ALT_RAW);
    Tuple *speed_t = dict_find(iterator, MSG_KEY_SPEED);

    if (lat_t && lon_t && alt_t && speed_t && s_gps_fix_handler) {
      double lat = (double)lat_t->value->int32 / 1000000.0;
      double lon = (double)lon_t->value->int32 / 1000000.0;
      double alt = (double)alt_t->value->int32 / 100.0;
      double speed_kmh = (double)speed_t->value->int32 / 100.0;

      APP_LOG(
        APP_LOG_LEVEL_INFO,
        "GoLog: received GPS fix from JS lat=%ld lon=%ld alt=%ld speed=%ld",
        (long)lat_t->value->int32,
        (long)lon_t->value->int32,
        (long)alt_t->value->int32,
        (long)speed_t->value->int32
      );

      s_gps_fix_handler(lat, lon, alt, speed_kmh);
    } else {
      APP_LOG(
        APP_LOG_LEVEL_WARNING,
        "GoLog: GPS fix message incomplete or handler missing lat=%d lon=%d alt=%d speed=%d handler=%d",
        lat_t ? 1 : 0,
        lon_t ? 1 : 0,
        alt_t ? 1 : 0,
        speed_t ? 1 : 0,
        s_gps_fix_handler ? 1 : 0
      );
    }
  } else if (event_type == EVENT_TYPE_SETTINGS_SYNC) {
    Tuple *run_t = dict_find(iterator, MSG_KEY_AUTOPAUSE_RUN_THRESHOLD);
    Tuple *cycle_t = dict_find(iterator, MSG_KEY_AUTOPAUSE_CYCLE_THRESHOLD);
    Tuple *units_t = dict_find(iterator, MSG_KEY_UNITS_METRIC);

    APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: received settings sync");

    if (run_t) {
      extern void autopause_set_run_threshold_kmh(double);
      autopause_set_run_threshold_kmh((double)run_t->value->int32 / 100.0);
    }

    if (cycle_t) {
      extern void autopause_set_cycle_threshold_kmh(double);
      autopause_set_cycle_threshold_kmh((double)cycle_t->value->int32 / 100.0);
    }

    if (units_t) {
      extern void units_set_metric(bool);
      units_set_metric(units_t->value->int32 == 1);
    }
  } else if (event_type == EVENT_TYPE_SYNC_STATUS) {
    Tuple *status_t = dict_find(iterator, MSG_KEY_SYNC_STATUS);

    if (status_t && s_sync_status_handler) {
      APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: received sync status=%d", (int)status_t->value->int32);
      s_sync_status_handler((int)status_t->value->int32);
    } else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: sync status missing or handler absent");
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: unknown event_type=%d", event_type);
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: inbox dropped, reason=%d", reason);
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: outbox failed, reason=%d", reason);
}

void app_message_protocol_init(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: initializing AppMessage protocol");

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

void app_message_protocol_send_command(ActivityCommand command, ActivityType activity_type) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: outbox begin failed for command=%d reason=%d", (int)command, (int)result);
    return;
  }

  dict_write_int32(iter, MSG_KEY_ACTIVITY_COMMAND, (int32_t)command);
  dict_write_int32(iter, MSG_KEY_ACTIVITY_TYPE, (int32_t)activity_type);

  result = app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: sent command=%d activity_type=%d result=%d", (int)command, (int)activity_type, (int)result);
}

void app_message_protocol_request_strava_config(void) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: outbox begin failed for strava config reason=%d", (int)result);
    return;
  }

  dict_write_int32(iter, MSG_KEY_ACTIVITY_COMMAND, (int32_t)ACTIVITY_COMMAND_OPEN_STRAVA_CONFIG);

  result = app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: requested strava config result=%d", (int)result);
}

void app_message_protocol_send_sync_request(bool user_confirmed) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: outbox begin failed for sync request reason=%d", (int)result);
    return;
  }

  dict_write_int32(iter, MSG_KEY_SYNC_REQUEST, user_confirmed ? 1 : 0);

  result = app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: sent sync request result=%d", (int)result);
}

void app_message_protocol_send_settings_update(double run_threshold_kmh, double cycle_threshold_kmh, bool units_metric, bool gpx_export_enabled) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "GoLog: outbox begin failed for settings update reason=%d", (int)result);
    return;
  }

  dict_write_int32(iter, MSG_KEY_AUTOPAUSE_RUN_THRESHOLD, (int32_t)(run_threshold_kmh * 100));
  dict_write_int32(iter, MSG_KEY_AUTOPAUSE_CYCLE_THRESHOLD, (int32_t)(cycle_threshold_kmh * 100));
  dict_write_int32(iter, MSG_KEY_UNITS_METRIC, units_metric ? 1 : 0);
  dict_write_int32(iter, MSG_KEY_GPX_EXPORT_ENABLED, gpx_export_enabled ? 1 : 0);

  result = app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: sent settings update result=%d", (int)result);
}

void app_message_protocol_set_gps_fix_handler(GpsFixHandler handler) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: gps fix handler registered");
  s_gps_fix_handler = handler;
}

void app_message_protocol_set_sync_status_handler(SyncStatusHandler handler) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: sync status handler registered");
  s_sync_status_handler = handler;
}