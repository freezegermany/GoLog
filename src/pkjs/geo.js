function long2tileFloat(lon, zoom) {
  return (lon + 180) / 360 * Math.pow(2, zoom);
}

function lat2tileFloat(lat, zoom) {
  var rad = lat * Math.PI / 180;
  return (1 - Math.log(Math.tan(rad) + 1 / Math.cos(rad)) / Math.PI) / 2 * Math.pow(2, zoom);
}

function mod(n, m) {
  return ((n % m) + m) % m;
}

function getDistanceFromLatLonInMeters(lat1, lon1, lat2, lon2) {
  if (lat1 === undefined || lon1 === undefined) return Infinity;
  var R = 6371000;
  var dLat = (lat2 - lat1) * Math.PI / 180;
  var dLon = (lon2 - lon1) * Math.PI / 180;
  var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
    Math.sin(dLon / 2) * Math.sin(dLon / 2);
  var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

module.exports = {
  long2tileFloat: long2tileFloat,
  lat2tileFloat: lat2tileFloat,
  mod: mod,
  getDistanceFromLatLonInMeters: getDistanceFromLatLonInMeters
};
