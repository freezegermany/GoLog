var GpxExport = {
  buildGpxString: function (points, metadata) {
    var activityTypeTag = metadata.activityType === 'cycling' ? 'cycling' : 'running';
    var trkpts = points.map(function (p) { var timeIso = new Date(p.timestamp).toISOString(); return '      <trkpt lat="' + p.lat + '" lon="' + p.lon + '">
' + '        <ele>' + (p.altitude || 0) + '</ele>
' + '        <time>' + timeIso + '</time>
' + '      </trkpt>'; }).join('
');
    return '<?xml version="1.0" encoding="UTF-8"?>
' + '<gpx version="1.1" creator="GoLog">
' + '  <trk>
' + '    <name>GoLog ' + activityTypeTag + '</name>
' + '    <type>' + activityTypeTag + '</type>
' + '    <trkseg>
' + trkpts + '
' + '    </trkseg>
' + '  </trk>
' + '</gpx>';
  },
  exportIfEnabled: function (points, metadata, gpxExportEnabled) { if (!gpxExportEnabled) return null; var gpx = GpxExport.buildGpxString(points, metadata); var filename = 'golog_' + metadata.activityType + '_' + Date.now() + '.gpx'; return filename; }
};
module.exports = GpxExport;
