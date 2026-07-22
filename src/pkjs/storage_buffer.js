var TRACKPOINTS_KEY_PREFIX = 'golog_track_';
var PENDING_SYNC_KEY = 'golog_pending_syncs';
var StorageBuffer = (function () {
  function loadPendingSyncs() { var raw = localStorage.getItem(PENDING_SYNC_KEY); return raw ? JSON.parse(raw) : []; }
  function savePendingSyncs(list) { localStorage.setItem(PENDING_SYNC_KEY, JSON.stringify(list)); }
  return {
    appendTrackpoint: function (activityId, point) { var key = TRACKPOINTS_KEY_PREFIX + activityId; var raw = localStorage.getItem(key); var points = raw ? JSON.parse(raw) : []; points.push(point); localStorage.setItem(key, JSON.stringify(points)); },
    getTrackpoints: function (activityId) { var raw = localStorage.getItem(TRACKPOINTS_KEY_PREFIX + activityId); return raw ? JSON.parse(raw) : []; },
    clearTrackpoints: function (activityId) { localStorage.removeItem(TRACKPOINTS_KEY_PREFIX + activityId); },
    enqueuePendingSync: function (activityId, metadata) { var list = loadPendingSyncs(); list.push({ activityId: activityId, metadata: metadata, attempts: 0 }); savePendingSyncs(list); },
    removePendingSync: function (activityId) { var list = loadPendingSyncs().filter(function (entry) { return entry.activityId !== activityId; }); savePendingSyncs(list); },
    getPendingSyncs: function () { return loadPendingSyncs(); },
    incrementAttempt: function (activityId) { var list = loadPendingSyncs(); list.forEach(function (entry) { if (entry.activityId === activityId) entry.attempts += 1; }); savePendingSyncs(list); }
  };
})();
module.exports = StorageBuffer;
