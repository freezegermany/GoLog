var tileUrl = require('./tileUrl.js');

function load(provider, zoom, x, y, success, error) {
  console.log('tileCache.load aufgerufen: Provider=' + provider + ', Zoom=' + zoom + ', X=' + x + ', Y=' + y);

  var url = tileUrl.getTileURL(provider, zoom, x, y);
  if (!url) {
    console.log('Ungültige URL für Provider: ' + provider);
    error('Ungültige URL');
    return;
  }

  console.log('Lade Kachel von URL: ' + url);

  // Verwende das globale Image-Objekt (funktioniert in PebbleKit-JS auf echten Geräten)
  var img = new Image();
  img.onload = function() {
    console.log('Kachel erfolgreich geladen von URL: ' + url);
    success(img);
  };
  img.onerror = function() {
    console.log('Fehler beim Laden der Kachel von URL: ' + url);
    error('Netzwerkfehler oder ungültige URL');
  };
  img.src = url;
}

module.exports = {
  load: load
};