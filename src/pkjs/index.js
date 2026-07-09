/* GoLog - PebbleKit JS Companion (korrigiert für GPS) */
var KEY_GPS_SPEED_KMH = 1;
var KEY_GPS_DISTANCE_M = 2;
var KEY_GPS_PACE_S = 3;
var KEY_HR_BPM = 4;
var KEY_HR_SOURCE = 5;
var KEY_CMD_START = 10;
var KEY_CMD_STOP = 11;
var KEY_ACTIVITY_TYPE = 12;
var KEY_GPS_LAT_E6 = 13;
var KEY_GPS_LON_E6 = 14;
var KEY_MAP_INDEX = 20;
var KEY_MAP_LENGTH = 21;
var KEY_MAP_CHUNK = 22;
var KEY_MAP_CHUNK_SIZE = 23;
var KEY_MAP_COMPLETE = 24;
var KEY_MAP_WIDTH = 25;
var KEY_MAP_HEIGHT = 26;
var KEY_MAP_ROW_BYTES = 27;
var KEY_MAP_IS_COLOR = 28;
var KEY_GPS_BEARING_DEG = 29;
var KEY_MAP_ZOOM = 30;
var KEY_CMD_LAP = 31;
var KEY_MAP_MODE = 32;

var recording = false,
    activityType = 0,
    gpsWatchId = null,
    lastPos = null,
    totalDistM = 0,
    trackPoints = [],
    lastMapLat = null,
    lastMapLon = null,
    lastMapFetchTs = 0,
    currentMapZoom = 16,
    currentMapMode = 0,
    smoothedSpeedMps = 0,
    lastTileKey = null,
    lastHrBpm = 0,
    lastHrSource = 0,
    lastPauseState = 'resume';

var MAP_REFRESH_MIN_METERS = 20;
var GPS_MIN_ACCURACY_METERS = 80; // Standardwert, kann für Tests erhöht werden

var geo = require('./geo');
var tileCache = require('./tileCache');
var tileUrl = require('./tileUrl.js');
var tileRenderer = require('./tileRenderer');

// GPS-Position erfolgreich empfangen
function onPosition(pos) {
  console.log('GPS-Position empfangen: Lat=' + pos.coords.latitude + ', Lon=' + pos.coords.longitude + ', Genauigkeit=' + pos.coords.accuracy);

  if (pos.coords.accuracy <= GPS_MIN_ACCURACY_METERS) {
    lastPos = {
      latitude: pos.coords.latitude,
      longitude: pos.coords.longitude,
      accuracy: pos.coords.accuracy,
      speed: pos.coords.speed || 0
    };

    // Sende GPS-Daten an die Watch
    var msg = {};
    msg[KEY_GPS_LAT_E6] = Math.round(lastPos.latitude * 1e6);
    msg[KEY_GPS_LON_E6] = Math.round(lastPos.longitude * 1e6);
    msg[KEY_MAP_ZOOM] = currentMapZoom;

    Pebble.sendAppMessage(msg, function() {
      console.log('GPS-Daten an Watch gesendet');
    }, function(e) {
      console.log('Fehler beim Senden der GPS-Daten: ' + JSON.stringify(e));
    });

    // Lade die Karte für die neuen Koordinaten
    fetchTiles(lastPos.latitude, lastPos.longitude, currentMapZoom);
  } else {
    console.log('GPS-Genauigkeit zu niedrig: ' + pos.coords.accuracy);
    tileRenderer.createTileRenderer().renderError({ renderState: {}, config: {} }, 'Waiting for GPS', '📡');
  }
}

// GPS-Fehler
function onPositionError(err) {
  console.log('GPS-Fehler ' + err.code + ': ' + err.message);
  tileRenderer.createTileRenderer().renderError({ renderState: {}, config: {} }, 'GPS Error: ' + err.message, '❌');
}

// Starte GPS-Aufzeichnung
function startRecording() {
  if (recording) return;
  recording = true;
  totalDistM = 0;
  lastPos = null;
  lastTileKey = null;
  lastMapFetchTs = 0;
  smoothedSpeedMps = 0;

  // Starte GPS-Überwachung
  if (gpsWatchId !== null) {
    navigator.geolocation.clearWatch(gpsWatchId);
  }
  gpsWatchId = navigator.geolocation.watchPosition(
    onPosition,
    onPositionError,
    {
      enableHighAccuracy: true,
      maximumAge: 1000,
      timeout: 10000
    }
  );
  console.log('GPS-Überwachung gestartet (watchPosition)');
}

// Stoppe GPS-Aufzeichnung
function stopRecording() {
  recording = false;
  if (gpsWatchId !== null) {
    navigator.geolocation.clearWatch(gpsWatchId);
    gpsWatchId = null;
  }
  console.log('GPS-Überwachung gestoppt');
}

// Lade Kacheln für die Karte
function fetchTiles(lat, lon, zoom) {
  console.log('fetchTiles aufgerufen mit: Lat=' + lat + ', Lon=' + lon + ', Zoom=' + zoom);

  if (!lat || !lon) {
    console.log('Ungültige Koordinaten in fetchTiles!');
    return;
  }

  lastMapLat = lat;
  lastMapLon = lon;

  var tileX = geo.long2tileFloat(lon, zoom);
  var tileY = geo.lat2tileFloat(lat, zoom);

  console.log('Kachelkoordinaten berechnet: X=' + tileX + ', Y=' + tileY);

  tileCache.load('osm', zoom, Math.floor(tileX), Math.floor(tileY), function(img) {
    console.log('Kachel erfolgreich geladen: X=' + Math.floor(tileX) + ', Y=' + Math.floor(tileY));
    processTile(img, Math.floor(tileX), Math.floor(tileY));
  }, function(error) {
    console.log('Fehler in tileCache.load: ' + error);
    tileRenderer.createTileRenderer().renderError({ renderState: {}, config: {} }, 'Error loading map', '❌');
  });
}

// Verarbeite geladene Kachel
function processTile(img, x, y) {
  console.log('Kachel verarbeitet: X=' + x + ', Y=' + y);
  var renderer = tileRenderer.createTileRenderer({ tileProvider: 'osm' });
  renderer.render(
    {
      renderState: { width: 144, height: 168, isColor: true },
      config: { enforceMonochrome: false }
    },
    {
      zoom: currentMapZoom,
      jobs: [{ srcX: x, srcY: y, drawX: 0, drawY: 0 }]
    }
  );
}

// AppMessage-Empfang
Pebble.addEventListener('ready', function() {
  console.log('Companion App bereit');
});

Pebble.addEventListener('appmessage', function(e) {
  var p = e.payload || {};
  if (p[KEY_CMD_START]) {
    activityType = p[KEY_ACTIVITY_TYPE] || 0;
    startRecording();
  }
  if (p[KEY_CMD_STOP]) {
    stopRecording();
  }
  if (typeof p[KEY_MAP_ZOOM] === 'number') {
    currentMapZoom = p[KEY_MAP_ZOOM];
    lastTileKey = null;
    if (lastPos) {
      fetchTiles(lastPos.latitude, lastPos.longitude, currentMapZoom);
    }
  }
  if (typeof p[KEY_MAP_MODE] === 'number') {
    currentMapMode = p[KEY_MAP_MODE];
    lastTileKey = null;
    if (lastPos) {
      fetchTiles(lastPos.latitude, lastPos.longitude, currentMapZoom);
    }
  }
  if (p[KEY_CMD_LAP]) {
    console.log('Lap marker received');
  }
});