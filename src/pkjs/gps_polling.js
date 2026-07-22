var GpsPolling = (function () {
  var watchId = null;
  var onFixCallback = null;
  var isTracking = false;

  function handleSuccess(pos) {
    console.log('GoLog: GPS position received lat=' + pos.coords.latitude + ' lon=' + pos.coords.longitude);
    if (onFixCallback) {
      onFixCallback({
        lat: pos.coords.latitude,
        lon: pos.coords.longitude,
        altitude: pos.coords.altitude || 0,
        speedKmh: (pos.coords.speed || 0) * 3.6,
        timestamp: pos.timestamp
      });
    }
  }

  function handleError(err) {
    console.log('GoLog: GPS error code=' + err.code + ' message=' + err.message);
  }

  return {
    warmup: function () {
      if (watchId !== null) {
        console.log('GoLog: GPS warmup skipped, watch already active');
        return;
      }
      console.log('GoLog: GPS warmup starting watchPosition');
      watchId = navigator.geolocation.watchPosition(handleSuccess, handleError, {
        enableHighAccuracy: true,
        maximumAge: 10000,
        timeout: 10000
      });
    },

    startTracking: function (autopauseThresholdKmh, fixCallback) {
      onFixCallback = fixCallback;
      isTracking = true;
      console.log('GoLog: GPS tracking started, threshold=' + autopauseThresholdKmh);

      if (watchId === null) {
        console.log('GoLog: watch was not warmed up, starting now');
        watchId = navigator.geolocation.watchPosition(handleSuccess, handleError, {
          enableHighAccuracy: true,
          maximumAge: 0,
          timeout: 5000
        });
      }
    },

    stop: function () {
      isTracking = false;
      onFixCallback = null;
      if (watchId !== null) {
        console.log('GoLog: clearing GPS watch');
        navigator.geolocation.clearWatch(watchId);
        watchId = null;
      }
    },

    updateActivityState: function (state) {
      console.log('GoLog: activity state updated to ' + state);
    }
  };
})();

module.exports = GpsPolling;