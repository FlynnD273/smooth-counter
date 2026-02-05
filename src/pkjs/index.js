const Clay = require('@rebble/clay');
const Keys = require('message_keys');

var settings = {
	value: 1,
	every_value: 10,
	offset_value: 0,
}

function saveSettings() {
	localStorage.setItem("settings", JSON.stringify(settings));
}
function loadSettings() {
	try {
		settings = JSON.parse(localStorage.getItem("settings")) || settings;
	}
	catch (e) { }
}
loadSettings();
saveSettings();

const clayConfig = require("./config");
const clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener("appmessage",
	function(e) {
		const dict = e.payload;
		if ("value" in dict) {
			settings.value = dict.value;
			saveSettings();
		}
	}
);

Pebble.addEventListener('showConfiguration', function(_e) {
	Pebble.sendAppMessage({}, () => {
		const settingsString = localStorage.getItem("clay-settings");
		if (settingsString) {
			const claySettings = JSON.parse(settingsString);
			claySettings.value = settings.value;
			localStorage.setItem("clay-settings", JSON.stringify(claySettings));
		}
		Pebble.openURL(clay.generateUrl());
	}, function(e) {
		console.warn("Failed to send config data!", e);
	});
});

function getKey(dict, setting, prop) {
	if (Keys[prop] in dict)
		setting[prop] = dict[Keys[prop]];
}

Pebble.addEventListener("webviewclosed", function(e) {
	if (e && !e.response) { return; }

	var dict = clay.getSettings(e.response);
	for (const key of Object.keys(dict)) {
		dict[key] = parseInt(dict[key]) || 0;
	}
	getKey(dict, settings, "value");
	getKey(dict, settings, "every_value");
	getKey(dict, settings, "offset_value");

	saveSettings();

	Pebble.sendAppMessage(dict, null, function(e) {
		console.warn("Failed to send config data!", e);
	});
});
