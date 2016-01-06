/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
var app = {
	socket: {},
	presets: {
		1: {
			'test1': 10,
			'test2': 155
		},
		2: {
			'KRAZYRED': [165, 0, 0, 0],
			'KRAZYGREEN': [0, 165, 0, 0],
			'KRAZYBLUE': [0, 0, 165, 0]
		},
		3: {
			'preset': [165, 200]
		}
	},
	// Application Constructor
	initialize: function() {
		this.bindEvents();
		this.initializePresets();
		this.bindClickHandlers();
	},

	// Bind Event Listeners
	bindEvents: function() {
		document.addEventListener('deviceready', this.onDeviceReady, false);
		app.socket = io.connect('http://192.168.66.1:3003');
		//app.socket = io.connect('http://192.168.10.192:3003');
	},

	/*
	 * init the presets
	 */
	initializePresets: function() {
		for (var i in app.presets) {
			$.each(app.presets[i], function(key, value) {
				console.log('preset name is: ' + key + ' preset value is: ' + value);
				$('#presetDropdown').append('<option value="' + value + '" data-modus="' + i + '">' + key + '</option>');
			});
		}

		$('#presetDropdown').on('change', function() {
			var mode = $(this).find(':selected').data('modus'),
				preset = $(this).val(),
				presetName = 'p' + mode;

			console.log('set mode to: ' + mode + ' with preset: ' + preset + ' and presetName: ' + presetName);

			// send values to lamp
			app.socket.emit('lampMode', parseInt(mode));
			app.socket.emit(presetName, preset);


			// update dropdown and show/hide parameters section
			$(".lampMode").val(mode);
			$('.modes').hide();
			//$('.lampMode>option:eq(' + mode + ')').attr('selected', true);
			$('#paramMode' + mode).show();

			// update slider
			app.setSliderPresets(mode, preset);

		});
	},
	/*
	 * set sliders...
	 * @param {string} mode - selected mode 0, 1, 2, 3
	 * @param {array} presets - presets...
	 */
	setSliderPresets: function(mode, preset) {
		console.log(mode);
		console.log(typeof(mode));
		switch (mode) {
			case 1:
				console.log('set slider 1 to: ' + preset);
				$('#s-1-bright').val(preset).slider("refresh");
				break;
			case 2:
				var array = preset.split(',');
				$('#s-2-brightRed').val(array[0]).slider("refresh");
				$('#s-2-brightGreen').val(array[1]).slider("refresh");
				$('#s-2-brightBlue').val(array[2]).slider("refresh");
				$('#s-2-brightWhite').val(array[3]).slider("refresh");
				break;
			case 3:
				var array = preset.split(',');
				$('#s-3-seed').val(array[0]).slider("refresh");
				$('#s-3-generation').val(array[1]).slider("refresh");
				break;
		}
	},

	/*
	 * Bind global click handlers...
	 */
	bindClickHandlers: function() {
		console.log('bind click handlers');

		// flipswitch for general on/off 
		// $('#select-based-flipswitch').on('change', function() {
		// 	var value = parseInt($(this).val());
		// 	value === 0 ? app.socket.emit('lampStatus', 'off') : app.socket.emit('lampStatus', 'on');
		// });

		// dropdown 
		$('.lampMode').on('change', function() {
			var lampMode = parseInt($(this).val());
			console.log('#### changing lamp mode to: ' + lampMode);
			app.socket.emit('lampMode', lampMode);
			$('.modes').hide();
			$('#paramMode' + lampMode).show();
		});

		$('#s-g-bright, #s-2-brightRed, #s-2-brightGreen, #s-2-brightBlue, #s-2-brightWhite, #s-4-bright').slider({
			min: 0,
			max: 165,
			value: 100
		});

		$('#s-1-bright').slider({
			min: 0,
			max: 165,
			value: 100
		});
		$('#s-3-seed,#s-3-generation').slider({
			min: 0,
			max: 255,
			value: 100
		});

		$('#s-4-speed').slider({
			min: 0,
			max: 1000,
			value: 500
		});

		$("#s-2-brightRed, #s-2-brightGreen, #s-2-brightBlue, #s-g-bright, #s-2-brightWhite, #s-3-generation, #s-4-bright, #s-3-seed, #s-4-speed").slider("enable");
		$("#s-1-bright").slider("enable");

		$("#s-g-bright").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			console.log($(this));
			console.log('#### slider brightness: ' + value);
			app.socket.emit('pBrightness', value);
		});

		$("#s-1-bright").on("slidestop", function(event, ui) {
			console.log($(this).val());
			var value = parseInt($(this).val());
			app.socket.emit('p1', value);
		});

		$("#s-2-brightRed, #s-2-brightBlue, #s-2-brightGreen, #s-2-brightWhite").on("slidestop", function(event, ui) {
			var params = [];
			$('.slide2').each(function() {
				var value = parseInt($(this).val());
				isNaN(value) ?
					params.push(0) : params.push(value);
			});
			app.socket.emit('p2', params);
		});

		$("#s-3-seed, #s-3-generation").on("slidestop", function(event, ui) {
			var params = [];
			$('.slide3').each(function() {
				var value = parseInt($(this).val());
				isNaN(value) ?
					params.push(0) : params.push(value);
			});
			app.socket.emit('p3', params);
		});
		$("#s-4-speed").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			app.socket.emit('p4Speed', value);
		});
		$("#s-4-bright").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			app.socket.emit('p4Brightness', value);
		});
	},

	/*
	 * deviceready Event Handler
	 */
	onDeviceReady: function() {
		//app.socket = io.connect('http://127.0.0.1:3000');


		app.socket = io.connect('http://192.168.66.1:3003');
		// app.socket.on('connect', function() {
		// 	app.socket.on('text', function(text) {
		// 		//alert(text);
		// 	});

		// });
	}
};

app.initialize();