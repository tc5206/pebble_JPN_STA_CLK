// index.js
var Clay = require('@rebble/clay');
var clay = null;

function buildConfig(platform) {
  var isColor = ['basalt', 'chalk', 'emery', 'gabbro'].indexOf(platform) !== -1;
  var isMono  = ['aplite', 'diorite', 'flint'].indexOf(platform) !== -1;
  var isRound = ['chalk', 'gabbro'].indexOf(platform) !== -1;

  var config = [
    { "type": "heading", "defaultValue": "Watchface Settings" },
    {
      "type": "section",
      "items": [
        { "type": "heading", "defaultValue": "Animation" },
        {
          "type": "slider",
          "messageKey": "AnimDuration",
          "defaultValue": 2,
          "label": "Overshoot Duration",
          "description": "0: Off, 1: 40ms ... 5: 200ms",
          "min": 0,
          "max": 5,
          "step": 1
        }
      ]
    }
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
          "label": "Background Color",
          "sunlight": true
        }
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
  var isColor = ['basalt', 'chalk', 'emery', 'gabbro'].indexOf(platform) !== -1;

  clay = new Clay(buildConfig(platform), null, { autoHandleEvents: false });

  if (isColor) {
    try {
      if (!clay.meta) { clay.meta = {}; }
      if (!clay.meta.activeWatchInfo) { clay.meta.activeWatchInfo = watch || {}; }
      if (!clay.meta.activeWatchInfo.capabilities) {
        clay.meta.activeWatchInfo.capabilities = [];
      }
      if (clay.meta.activeWatchInfo.capabilities.indexOf('COLOR') === -1) {
        clay.meta.activeWatchInfo.capabilities.push('COLOR');
      }
    } catch (e) {
      console.log('Capabilities injection error: ' + e);
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