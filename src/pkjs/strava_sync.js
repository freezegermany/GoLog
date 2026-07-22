var StorageBuffer = require('./storage_buffer');
var GpxExport = require('./gpx_export');
var StravaAuth = require('./strava_auth');
var STRAVA_UPLOAD_ENDPOINT = 'https://www.strava.com/api/v3/uploads';
var MAX_RETRY_ATTEMPTS = 5;
var StravaSync = (function () {
  function isOnline() { return typeof navigator !== 'undefined' ? navigator.onLine : true; }
  function buildGpxAndUpload(activityId, metadata, accessToken, onDone) {
    var points = StorageBuffer.getTrackpoints(activityId);
    var gpxString = GpxExport.buildGpxString(points, metadata);
    var xhr = new XMLHttpRequest();
    xhr.open('POST', STRAVA_UPLOAD_ENDPOINT, true);
    xhr.setRequestHeader('Authorization', 'Bearer ' + accessToken);
    xhr.onload = function () { if (xhr.status >= 200 && xhr.status < 300) { StorageBuffer.removePendingSync(activityId); StorageBuffer.clearTrackpoints(activityId); onDone(true); } else { StorageBuffer.incrementAttempt(activityId); onDone(false); } };
    xhr.onerror = function () { StorageBuffer.incrementAttempt(activityId); onDone(false); };
    xhr.send(gpxString);
  }
  return {
    requestSync: function (activityId, metadata) { StorageBuffer.enqueuePendingSync(activityId, metadata); this.retryPending(); },
    retryPending: function () {
      if (!isOnline()) { console.log('GoLog: offline, deferring Strava sync retry'); return; }
      if (!StravaAuth.isAuthorized()) { console.log('GoLog: not connected to Strava yet, deferring sync retry'); return; }
      StravaAuth.getValidAccessToken(function (accessToken) {
        if (!accessToken) { console.log('GoLog: could not obtain valid Strava access token'); return; }
        var pending = StorageBuffer.getPendingSyncs();
        pending.forEach(function (entry) { if (entry.attempts >= MAX_RETRY_ATTEMPTS) return; buildGpxAndUpload(entry.activityId, entry.metadata, accessToken, function (success) { console.log('GoLog: sync ' + entry.activityId + ' -> ' + (success ? 'OK' : 'FAILED')); }); });
      });
    }
  };
})();
module.exports = StravaSync;
