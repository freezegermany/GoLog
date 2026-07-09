function render(params, viewport) {
  console.log('render aufgerufen mit Viewport: ' + JSON.stringify(viewport));

  var renderState = params.renderState;
  var config = params.config;
  var width = renderState.width;
  var height = renderState.height;
  var outputFormat = getOutputFormat(width, renderState.isColor, config.enforceMonochrome);
  var ctx = ensureCanvas(width, height);

  if (!ctx) {
    console.log('Canvas nicht verfügbar');
    return;
  }

  thisRenderToken = ++renderToken;
  var loaded = 0;

  // Prüfe, ob lastPos in index.js gesetzt ist (global)
  if (!window.lastPos && !lastPos) {
    console.log('Keine GPS-Daten für Rendering verfügbar (lastPos ist null)');
    renderError(params, 'Waiting for GPS', '📡');
    return;
  }

  console.log('GPS-Daten für Rendering verfügbar: Lat=' + (window.lastPos ? window.lastPos.latitude : lastPos.latitude) +
              ', Lon=' + (window.lastPos ? window.lastPos.longitude : lastPos.longitude));

  // Lade die Kacheln für den Viewport
  viewport.jobs.forEach(function(job) {
    tileCache.load(config.tileProvider, viewport.zoom, job.srcX, job.srcY, function(img) {
      if (thisRenderToken !== renderToken) return;
      ctx.drawImage(img, job.drawX, job.drawY, 256, 256);
      loaded += 1;
      finalizeOneTile();
    }, function(error) {
      console.log('Fehler beim Laden der Kachel in render: ' + error);
      finalizeOneTile();
    });
  });
}