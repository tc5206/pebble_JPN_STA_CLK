// index.js
var Clay = require('@rebble/clay');
var clay = null;

function buildConfig(platform) {
  var isColor = ['basalt', 'chalk', 'emery', 'gabbro'].indexOf(platform) !== -1;
  var isMono  = ['aplite', 'diorite', 'flint'].indexOf(platform) !== -1;
  var isRound = ['chalk', 'gabbro'].indexOf(platform) !== -1;

  var config = [
    { "type": "heading", "defaultValue": "Watchface Settings" }
  ];

  if (isColor) {
    config.push({
      "type": "section",
      "items": [
        { "type": "heading", "defaultValue": "Colors" },
        {
          "type": "color",
          "messageKey": "BackgroundColor",
          "defaultValue": "0xFFFFFF",
          "label": "Gauge Color (Time/Time2) / Face Color (Round/Round2)",
          "sunlight": true
        }
      ]
    });
  }

  if (isMono) {
    config.push({
      "type": "section",
      "items": [
        { "type": "heading", "defaultValue": "Monochrome Settings" },
        { "type": "text", "defaultValue": "Settings are limited for this device." }
      ]
    });
  }

  if (isRound) {
    config.push({
      "type": "section",
      "id": "digitalSection",
      "items": [
        { "type": "heading", "defaultValue": "Digital Display" },
        { "type": "toggle", "messageKey": "ShowDigitalTime",  "label": "Show Digital Time",    "defaultValue": true },
        { "type": "toggle", "messageKey": "ShowDigitalDate",  "label": "Show Date",             "defaultValue": true },
        { "type": "toggle", "messageKey": "ShowDigitalDay",   "label": "Show Day of Week",      "defaultValue": true }
      ]
    });
  }

  config.push({ "type": "submit", "defaultValue": "Save Settings" });
  return config;
}

Pebble.addEventListener('showConfiguration', function() {
  var watch = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
  var platform = (watch && watch.platform) ? watch.platform.toLowerCase() : 'basalt';
  var isColorPlatform = ['basalt', 'chalk', 'emery', 'gabbro'].indexOf(platform) !== -1;

  clay = new Clay(buildConfig(platform), null, { autoHandleEvents: false });

  if (isColorPlatform) {
    try {
      if (!clay.meta) { clay.meta = {}; }
      if (!clay.meta.activeWatchInfo) { clay.meta.activeWatchInfo = watch || {}; }
      if (!clay.meta.activeWatchInfo.capabilities) {
        clay.meta.activeWatchInfo.capabilities = [];
      }
      if (clay.meta.activeWatchInfo.capabilities.indexOf('COLOR') === -1) {
        clay.meta.activeWatchInfo.capabilities.push('COLOR');
      }
    } catch(e) {
      console.log('[clay.meta error] ' + e);
    }
  }

  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  var dict = clay.getSettings(e.response);
  Pebble.sendAppMessage(dict, function() {
    console.log('Config sent');
  }, function() {
    console.log('Config send failed');
  });
});