#include "activity_window.h"
#include "../theme.h"
#include "../activity_manager.h"
#include "../appmessage_protocol.h"
#include "map_view.h"
#include "../units.h"
#include "stop_confirm_dialog.h"

static Window *s_window;
static TextLayer *s_gps_status_layer;
static TextLayer *s_duration_layer;
static TextLayer *s_pace_layer;
static TextLayer *s_hr_layer;
static TextLayer *s_altitude_layer;
static ActivityType s_current_activity_type;
static AppTimer *s_ui_refresh_timer;

#define UI_REFRESH_INTERVAL_MS 1000
#define HR_SAMPLE_PERIOD_SECONDS 1

static void refresh_ui(void) {
  GpsStatus gps_status = activity_manager_get_gps_status();

  static char gps_buf[16];
  snprintf(
    gps_buf,
    sizeof(gps_buf),
    "GPS: %s",
    gps_status == GPS_STATUS_FIXED ? "Fixed" :
    gps_status == GPS_STATUS_LOST ? "Lost" : "Searching"
  );
  text_layer_set_text(s_gps_status_layer, gps_buf);

  uint32_t elapsed = activity_manager_get_elapsed_seconds();
  static char duration_buf[16];
  snprintf(
    duration_buf,
    sizeof(duration_buf),
    "%02lu:%02lu:%02lu",
    (unsigned long)(elapsed / 3600),
    (unsigned long)((elapsed / 60) % 60),
    (unsigned long)(elapsed % 60)
  );
  text_layer_set_text(s_duration_layer, duration_buf);

  double pace_sec_per_km = activity_manager_get_average_pace_sec_per_km();
  static char pace_buf[24];
  units_format_pace(pace_sec_per_km, pace_buf, sizeof(pace_buf));
  text_layer_set_text(s_pace_layer, pace_buf);

  int hr = activity_manager_get_hr_bpm();
  static char hr_buf[16];
  if (hr > 0) {
    snprintf(hr_buf, sizeof(hr_buf), "%d bpm", hr);
  } else {
    snprintf(hr_buf, sizeof(hr_buf), "HR --");
  }
  text_layer_set_text(s_hr_layer, hr_buf);

  int elev_gain_m = (int)(activity_manager_get_elevation_gain_m() + 0.5);
  static char alt_buf[24];
  snprintf(alt_buf, sizeof(alt_buf), "Elev +%dm", elev_gain_m);
  text_layer_set_text(s_altitude_layer, alt_buf);
}

static void ui_refresh_timer_callback(void *data) {
  activity_manager_tick_second();
  refresh_ui();
  s_ui_refresh_timer = app_timer_register(UI_REFRESH_INTERVAL_MS, ui_refresh_timer_callback, NULL);
}

static void health_event_handler(HealthEventType event, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: health event=%d", (int)event);

  if (event == HealthEventHeartRateUpdate) {
    HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
    APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: HR update value=%ld", (long)hr);

    if (hr > 0) {
      activity_manager_set_hr_bpm((int)hr);
      refresh_ui();
    }
  }
}

static void init_hrm(void) {
  bool hrm_supported = health_service_metric_accessible(HealthMetricHeartRateBPM, time_start_of_today(), time(NULL)) == HealthServiceAccessibilityMaskAvailable;
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: HR accessibility exact=%d", hrm_supported ? 1 : 0);

  health_service_events_subscribe(health_event_handler, NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: health events subscribed");

  health_service_set_heart_rate_sample_period(HR_SAMPLE_PERIOD_SECONDS);
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: requested HR sample period=%d", HR_SAMPLE_PERIOD_SECONDS);

  HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: initial HR peek=%ld", (long)hr);

  if (hr > 0) {
    activity_manager_set_hr_bpm((int)hr);
  }
}

static void deinit_hrm(void) {
  health_service_set_heart_rate_sample_period(0);
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: HR sample period reset");

  health_service_events_unsubscribe();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: health events unsubscribed");
}

static void gps_fix_received(double lat, double lon, double alt_raw, double speed_kmh) {
  APP_LOG(
    APP_LOG_LEVEL_INFO,
    "GoLog: gps_fix_received lat=%ld lon=%ld alt=%ld speed=%ld",
    (long)(lat * 1000000),
    (long)(lon * 1000000),
    (long)(alt_raw * 100),
    (long)(speed_kmh * 100)
  );

  activity_manager_process_gps_fix(lat, lon, alt_raw, speed_kmh);
  map_view_add_point(lat, lon);
  refresh_ui();
}

static void start_or_toggle_pause(const char *button_name) {
  ActivityState state = activity_manager_get_state();

  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: %s clicked, state=%d", button_name, (int)state);

  if (state == ACTIVITY_STATE_IDLE) {
    APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: sending START command, type=%d", (int)s_current_activity_type);
    activity_manager_start(s_current_activity_type);
    app_message_protocol_send_command(ACTIVITY_COMMAND_START, s_current_activity_type);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: toggling pause, state=%d", (int)state);
    activity_manager_manual_toggle_pause();
  }

  refresh_ui();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  start_or_toggle_pause("SELECT");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  start_or_toggle_pause("DOWN");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: UP clicked");
  map_view_push();
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: BACK clicked");

  ActivityState state = activity_manager_get_state();
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: back pressed, state=%d", (int)state);

  if (state == ACTIVITY_STATE_RUNNING ||
      state == ACTIVITY_STATE_AUTOPAUSED ||
      state == ACTIVITY_STATE_MANUAL_PAUSED) {
    stop_confirm_dialog_push();
  } else {
    window_stack_pop(true);
  }
}

static void click_config_provider(void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: click_config_provider called");

  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: activity window load");

  golog_style_window(window);

  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  s_gps_status_layer = text_layer_create(GRect(0, 0, bounds.size.w, 24));
  text_layer_set_font(s_gps_status_layer, GOLOG_FONT_SMALL);
  golog_style_text_layer(s_gps_status_layer);
  text_layer_set_text_alignment(s_gps_status_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(s_gps_status_layer));

  s_duration_layer = text_layer_create(GRect(0, 24, bounds.size.w, 50));
  text_layer_set_font(s_duration_layer, GOLOG_FONT_LARGE);
  golog_style_text_layer(s_duration_layer);
  text_layer_set_text_alignment(s_duration_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(s_duration_layer));

  s_pace_layer = text_layer_create(GRect(0, 78, bounds.size.w, 28));
  text_layer_set_font(s_pace_layer, GOLOG_FONT_MEDIUM);
  golog_style_text_layer(s_pace_layer);
  text_layer_set_text_alignment(s_pace_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(s_pace_layer));

  s_hr_layer = text_layer_create(GRect(0, 108, bounds.size.w, 28));
  text_layer_set_font(s_hr_layer, GOLOG_FONT_MEDIUM);
  golog_style_text_layer(s_hr_layer);
  text_layer_set_text_alignment(s_hr_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(s_hr_layer));

  s_altitude_layer = text_layer_create(GRect(0, 138, bounds.size.w, 28));
  text_layer_set_font(s_altitude_layer, GOLOG_FONT_SMALL);
  golog_style_text_layer(s_altitude_layer);
  text_layer_set_text_alignment(s_altitude_layer, GTextAlignmentCenter);
  layer_add_child(root_layer, text_layer_get_layer(s_altitude_layer));

  app_message_protocol_set_gps_fix_handler(gps_fix_received);
  init_hrm();

  refresh_ui();
  s_ui_refresh_timer = app_timer_register(UI_REFRESH_INTERVAL_MS, ui_refresh_timer_callback, NULL);
}

static void window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: activity window unload");

  if (s_ui_refresh_timer) {
    app_timer_cancel(s_ui_refresh_timer);
    s_ui_refresh_timer = NULL;
  }

  deinit_hrm();

  text_layer_destroy(s_gps_status_layer);
  text_layer_destroy(s_duration_layer);
  text_layer_destroy(s_pace_layer);
  text_layer_destroy(s_hr_layer);
  text_layer_destroy(s_altitude_layer);
}

void activity_window_push(ActivityType activity_type) {
  APP_LOG(APP_LOG_LEVEL_INFO, "GoLog: push activity window, type=%d", (int)activity_type);

  s_current_activity_type = activity_type;
  s_window = window_create();

  window_set_click_config_provider(s_window, click_config_provider);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  window_stack_push(s_window, true);
}