var StravaAuth = require('./strava_auth');
var StravaSync = require('./strava_sync');
var StorageBuffer = require('./storage_buffer');
var GpsPolling = require('./gps_polling');

console.log('GoLog: PKJS index.js REBUILD v1 loaded');

var EVENT_TYPE_GPS_FIX = 1;
var EVENT_TYPE_SETTINGS_SYNC = 2;
var EVENT_TYPE_SYNC_STATUS = 3;

var ACTIVITY_COMMAND_START = 1;
var ACTIVITY_COMMAND_STOP = 2;
var ACTIVITY_COMMAND_DISCARD = 3;
var ACTIVITY_COMMAND_OPEN_STRAVA_CONFIG = 4;

var currentActivityId = null;
var currentActivityType = 0;
var currentSettings = {
  runThresholdKmh: 3.0,
  cycleThresholdKmh: 5.0,
  unitsMetric: true,
  gpxExportEnabled: false
};

function getThresholdForActivityType(activityType) {
  return activityType === 1 ? currentSettings.cycleThresholdKmh : currentSettings.runThresholdKmh;
}

function sendGpsFixToWatch(fix) {
  console.log('GoLog: sendGpsFixToWatch lat=' + fix.lat + ' lon=' + fix.lon);

  Pebble.sendAppMessage(
    {
      EVENT_TYPE: EVENT_TYPE_GPS_FIX,
      LAT: Math.round(fix.lat * 1000000),
      LON: Math.round(fix.lon * 1000000),
      ALT_RAW: Math.round((fix.altitude || 0) * 100),
      SPEED: Math.round((fix.speedKmh || 0) * 100)
    },
    function () { console.log('GoLog: GPS fix delivered to watch'); },
    function (e) { console.log('GoLog: failed to send GPS fix: ' + JSON.stringify(e)); }
  );

  if (currentActivityId) {
    StorageBuffer.appendTrackpoint(currentActivityId, fix);
  }
}

function pushSettingsToWatch() {
  Pebble.sendAppMessage(
    {
      EVENT_TYPE: EVENT_TYPE_SETTINGS_SYNC,
      AUTOPAUSE_RUN_THRESHOLD: Math.round(currentSettings.runThresholdKmh * 100),
      AUTOPAUSE_CYCLE_THRESHOLD: Math.round(currentSettings.cycleThresholdKmh * 100),
      UNITS_METRIC: currentSettings.unitsMetric ? 1 : 0,
      GPX_EXPORT_ENABLED: currentSettings.gpxExportEnabled ? 1 : 0
    },
    function () { console.log('GoLog: settings delivered to watch'); },
    function (e) { console.log('GoLog: failed to send settings: ' + JSON.stringify(e)); }
  );
}

function notifySyncStatus(statusCode) {
  Pebble.sendAppMessage(
    { EVENT_TYPE: EVENT_TYPE_SYNC_STATUS, SYNC_STATUS: statusCode },
    function () { console.log('GoLog: sync status delivered: ' + statusCode); },
    function (e) { console.log('GoLog: failed to send sync status: ' + JSON.stringify(e)); }
  );
}

Pebble.addEventListener('ready', function () {
  console.log('GoLog: PKJS ready fired');
  GpsPolling.warmup();
  pushSettingsToWatch();
  StravaSync.retryPending();
});

Pebble.addEventListener('showConfiguration', function () {
  console.log('GoLog: opening configuration page');
  StravaAuth.openConfigPage();
});

Pebble.addEventListener('webviewclosed', function (event) {
  console.log('GoLog: configuration page closed');
  StravaAuth.handleConfigClosed(event.response, function (success, error) {
    console.log(success ? 'GoLog: Strava connected successfully' : 'GoLog: Strava connection failed - ' + error);
    StravaSync.retryPending();
  });
});

Pebble.addEventListener('appmessage', function (event) {
  var payload = event.payload || {};
  var command = payload.ACTIVITY_COMMAND;
  var activityType = payload.ACTIVITY_TYPE;
  var syncRequest = payload.SYNC_REQUEST;

  console.log('GoLog: PKJS received appmessage ' + JSON.stringify(payload));

  if (command === ACTIVITY_COMMAND_START) {
    currentActivityType = typeof activityType === 'number' ? activityType : 0;
    currentActivityId = 'act_' + Date.now();
    var thresholdKmh = getThresholdForActivityType(currentActivityType);
    console.log('GoLog: starting GPS tracking threshold=' + thresholdKmh);
    GpsPolling.startTracking(thresholdKmh, sendGpsFixToWatch);
  } else if (command === ACTIVITY_COMMAND_STOP) {
    console.log('GoLog: stopping GPS polling');
    GpsPolling.stop();
  } else if (command === ACTIVITY_COMMAND_DISCARD) {
    console.log('GoLog: discarding activity');
    GpsPolling.stop();
    if (currentActivityId) StorageBuffer.clearTrackpoints(currentActivityId);
    currentActivityId = null;
    currentActivityType = 0;
  } else if (command === ACTIVITY_COMMAND_OPEN_STRAVA_CONFIG) {
    console.log('GoLog: ACTIVITY_COMMAND_OPEN_STRAVA_CONFIG received');
    StravaAuth.openConfigPage();
  }

  if (syncRequest === 1 && currentActivityId) {
    var metadata = { activityType: currentActivityType === 1 ? 'cycling' : 'running' };
    console.log('GoLog: queueing sync for ' + currentActivityId);
    StravaSync.requestSync(currentActivityId, metadata);
    notifySyncStatus(2);
  }
});

if (typeof window !== 'undefined') {
  window.addEventListener('online', function () {
    console.log('GoLog: online event fired');
    StravaSync.retryPending();
  });
}