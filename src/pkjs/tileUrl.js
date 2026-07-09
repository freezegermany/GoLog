function getTileURL(provider, zoom, x, y) {
  if (provider === 'osm') {
    var url = 'https://tile.openstreetmap.org/' + zoom + '/' + x + '/' + y + '.png';
    console.log('Generierte Kachel-URL: ' + url);
    return url;
  }
  console.log('Unbekannter Provider: ' + provider);
  return null;
}

module.exports = {
  getTileURL: getTileURL
};