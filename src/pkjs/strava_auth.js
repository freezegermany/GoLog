var STRAVA_CLIENT_ID = 'YOUR_STRAVA_CLIENT_ID';
var STRAVA_CLIENT_SECRET = 'YOUR_STRAVA_CLIENT_SECRET';
var GOLOG_CONFIG_URL = 'https://freezegermany.github.io/golog-config/';
var STRAVA_REDIRECT_URI = GOLOG_CONFIG_URL;
var STRAVA_SCOPE = 'activity:write,activity:read_all';
var STRAVA_TOKEN_URL = 'https://www.strava.com/oauth/token';
var STRAVA_DEAUTHORIZE_URL = 'https://www.strava.com/oauth/deauthorize';
var STORAGE_KEY_ACCESS_TOKEN = 'golog_strava_access_token';
var STORAGE_KEY_REFRESH_TOKEN = 'golog_strava_refresh_token';
var STORAGE_KEY_EXPIRES_AT = 'golog_strava_expires_at';
var StravaAuth = (function () {
  function buildConfigPageUrl() { var params = ['client_id=' + encodeURIComponent(STRAVA_CLIENT_ID), 'redirect_uri=' + encodeURIComponent(STRAVA_REDIRECT_URI), 'scope=' + encodeURIComponent(STRAVA_SCOPE)]; return GOLOG_CONFIG_URL + '?' + params.join('&'); }
  function exchangeCodeForToken(code, onDone) { var xhr = new XMLHttpRequest(); xhr.open('POST', STRAVA_TOKEN_URL, true); xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'); xhr.onload = function () { if (xhr.status >= 200 && xhr.status < 300) { var response = JSON.parse(xhr.responseText); storeTokens(response); onDone(true, null); } else onDone(false, 'http_' + xhr.status); }; xhr.onerror = function () { onDone(false, 'network_error'); }; var body = 'client_id=' + encodeURIComponent(STRAVA_CLIENT_ID) + '&client_secret=' + encodeURIComponent(STRAVA_CLIENT_SECRET) + '&code=' + encodeURIComponent(code) + '&grant_type=authorization_code'; xhr.send(body); }
  function refreshAccessToken(onDone) { var refreshToken = localStorage.getItem(STORAGE_KEY_REFRESH_TOKEN); if (!refreshToken) { onDone(null); return; } var xhr = new XMLHttpRequest(); xhr.open('POST', STRAVA_TOKEN_URL, true); xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'); xhr.onload = function () { if (xhr.status >= 200 && xhr.status < 300) { var response = JSON.parse(xhr.responseText); storeTokens(response); onDone(localStorage.getItem(STORAGE_KEY_ACCESS_TOKEN)); } else onDone(null); }; xhr.onerror = function () { onDone(null); }; var body = 'client_id=' + encodeURIComponent(STRAVA_CLIENT_ID) + '&client_secret=' + encodeURIComponent(STRAVA_CLIENT_SECRET) + '&refresh_token=' + encodeURIComponent(refreshToken) + '&grant_type=refresh_token'; xhr.send(body); }
  function storeTokens(response) { localStorage.setItem(STORAGE_KEY_ACCESS_TOKEN, response.access_token); localStorage.setItem(STORAGE_KEY_REFRESH_TOKEN, response.refresh_token); localStorage.setItem(STORAGE_KEY_EXPIRES_AT, response.expires_at); }
  function clearTokens() { localStorage.removeItem(STORAGE_KEY_ACCESS_TOKEN); localStorage.removeItem(STORAGE_KEY_REFRESH_TOKEN); localStorage.removeItem(STORAGE_KEY_EXPIRES_AT); }
  return {
    openConfigPage: function () { Pebble.openURL(buildConfigPageUrl()); },
    handleConfigClosed: function (responseString, onDone) { if (!responseString) { onDone(false, 'cancelled'); return; } var params = {}; decodeURIComponent(responseString).split('&').forEach(function (pair) { var kv = pair.split('='); params[kv[0]] = kv[1]; }); if (params.error) { onDone(false, params.error); return; } if (!params.code) { onDone(false, 'missing_code'); return; } exchangeCodeForToken(params.code, onDone); },
    getValidAccessToken: function (onDone) { var expiresAt = parseInt(localStorage.getItem(STORAGE_KEY_EXPIRES_AT) || '0', 10); var nowSeconds = Math.floor(Date.now() / 1000); var hasRefreshToken = !!localStorage.getItem(STORAGE_KEY_REFRESH_TOKEN); if (!hasRefreshToken) { onDone(null); return; } if (expiresAt - nowSeconds > 3600) { onDone(localStorage.getItem(STORAGE_KEY_ACCESS_TOKEN)); return; } refreshAccessToken(onDone); },
    isAuthorized: function () { return !!localStorage.getItem(STORAGE_KEY_REFRESH_TOKEN); },
    deauthorize: function (onDone) { var accessToken = localStorage.getItem(STORAGE_KEY_ACCESS_TOKEN); if (!accessToken) { clearTokens(); onDone(true); return; } var xhr = new XMLHttpRequest(); xhr.open('POST', STRAVA_DEAUTHORIZE_URL, true); xhr.setRequestHeader('Authorization', 'Bearer ' + accessToken); xhr.onload = function () { clearTokens(); onDone(xhr.status >= 200 && xhr.status < 300); }; xhr.onerror = function () { clearTokens(); onDone(false); }; xhr.send(); }
  };
})();
module.exports = StravaAuth;
