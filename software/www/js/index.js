var app = {
	socket: {},
	inits: {
		sliderMax165: {
			min: 0,
			max: 165,
			value: 100
		},
		sliderMax255: {
			min: 0,
			max: 255,
			value: 100
		},
		sliderMax1000: {
			min: 0,
			max: 1000,
			value: 500
		},
		dropDownStatus: false
	},

	/*
	 * GLOBAL INIT
	 */
	initialize: function() {
		this.bindEvents();
		this.initializePresets();
		this.bindClickHandlers();
	},

	/*
	 * Bind Event Listeners
	 */
	bindEvents: function() {
		document.addEventListener('deviceready', this.onDeviceReady, false);
		app.socket = io.connect('http://192.168.66.1:3003');
		//app.socket = io.connect('http://192.168.10.192:3003');
	},

	/*
	 * init the presets
	 */
	initializePresets: function() {

		app.buildPresetDropdown();

		$('#presetDropdown').on('change', function() {
			var mode = $(this).find(':selected').data('mode'),
				preset = $(this).val(),
				presetName = 'p' + mode;

			if (mode !== undefined) {
				console.log('set mode to: ' + mode + ' with preset: ' + preset + ' and presetName: ' + presetName);
				// send values to lamp
				app.socket.emit('lampMode', parseInt(mode));
				app.socket.emit(presetName, preset);

				$('.lampMode>option').attr('selected', false);
				$('.lampMode>option:eq(' + mode + ')').attr('selected', true);
				app.inits.dropDownStatus = true;
				$(".lampMode").change();

				// update slider
				app.setSliderPresets(mode, preset);
			}
		});
	},
	/*
	 * build preset dropdown and append values
	 */
	buildPresetDropdown: function() {
		$('#presetDropdown').append('<option value="">________</option>');
		for (var i in presets) {
			$('#presetDropdown').append('<option value="">****** Mode ' + presets[i].pName + ' *************</option>');
			$.each(presets[i].entries, function(key, value) {
				//console.log('preset name is: ' + key + ' preset value is: ' + value);
				$('#presetDropdown').append('<option value="' + value + '" data-mode="' + i + '">' + key + '</option>');
			});
		}
	},

	/*
	 * set sliders...
	 * @param {string} mode - selected mode 0, 1, 2, 3
	 * @param {array} presets - presets...
	 */
	setSliderPresets: function(mode, preset) {
		switch (mode) {
			case 1:
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
	 * read sliders values...
	 * @param {string} _mode - selected mode 0, 1, 2, 3
	 * @return {array} values in array
	 */
	savePresets: function(_mode) {
		var params = [];
		$('.slide' + _mode).each(function() {
			var value = parseInt($(this).val());
			isNaN(value) ?
				params.push(0) : params.push(value);
		});
		return params;
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

		// on saving a preset, open a modal window with text input box
		$('.savePreset').on('click', function() {
			var currMode = $(this).data('mode'),
				newPresets = presets;

			$('#modalsavePreset').show();
			$('input[name=savePresetName]').val('');

			// submit handler
			$('form#fSavePreset').submit(function(e) {
				e.preventDefault();
				var pName = $('input[name=savePresetName]').val();
				if (pName !== "") {
					$('#modalsavePreset').fadeOut();
					var value = "";
					switch (currMode) {
						case 1:
							value = $('#s-1-bright').val();
							break;
						case 2:
						case 3:
							value = app.savePresets(currMode);
							break;

					}
					// add new preset value to current preset object
					newPresets[currMode].entries[pName] = value;
					app.socket.emit('savePresets', newPresets);
					presets = newPresets;
					// re-create dropdown preset menu
					$('#presetDropdown').html('');
					app.buildPresetDropdown();
				}
			});
		});

		// dropdown 
		$('.lampMode').on('change', function() {
			var lampMode = parseInt($(this).val());
			console.log('#### changing lamp mode to: ' + lampMode);
			app.inits.dropDownStatus === true ?
				app.inits.dropDownStatus = false : app.socket.emit('lampMode', lampMode);
			$('.modes').hide();
			$('#paramMode' + lampMode).show();
		});

		/* SLIDER MADNESS INITS AND SLIDE CALLBACKS */
		$('#s-g-bright, #s-1-bright, #s-2-brightRed, #s-2-brightGreen, #s-2-brightBlue, #s-2-brightWhite, #s-4-bright').slider(app.inits.slider165);
		$('#s-3-seed,#s-3-generation').slider(app.inits.slider255);
		$('#s-4-speed').slider(app.inits.sliderMax1000);;

		$("#s-g-bright").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			app.socket.emit('pBrightness', value);
		});

		$("#s-1-bright").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			app.socket.emit('p1', value);
		});

		$("#s-2-brightRed, #s-2-brightBlue, #s-2-brightGreen, #s-2-brightWhite").on("slidestop", function(event, ui) {
			var params = app.savePresets('2');
			app.socket.emit('p2', params);
		});

		$("#s-3-seed, #s-3-generation").on("slidestop", function(event, ui) {
			var params = app.savePresets('3');
			app.socket.emit('p3', params);
		});
		$("#s-4-speed").on("slidestop", function(event, ui) {
			var value = parseInt($(this).val());
			app.socket.emit('p4', value);
		});

	},

	/*
	 * deviceready Event Handler
	 */
	onDeviceReady: function() {
		//app.socket = io.connect('http://127.0.0.1:3000');
		app.socket = io.connect('http://192.168.66.1:3003');
	}
};

app.initialize();