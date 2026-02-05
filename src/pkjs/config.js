module.exports = [
	{
		"type": "heading",
		"defaultValue": "App Configuration"
	},
	{
		"type": "section",
		"items": [
			{
				"type": "heading",
				"defaultValue": "Game Settings"
			},
			{
				"type": "input",
				"messageKey": "value",
				"label": "The counter value",
				"defaultValue": 0,
			},
			{
				"type": "input",
				"messageKey": "every_value",
				"label": "Highlight every N numbers (0 to disable highlighting)",
				"defaultValue": 10,
			},
			{
				"type": "input",
				"messageKey": "offset_value",
				"label": "Offset the highlight",
				"defaultValue": 0,
			},
		]
	},
	{
		"type": "submit",
		"defaultValue": "Save Settings"
	}
];
