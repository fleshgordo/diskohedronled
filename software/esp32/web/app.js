// Diskohedron web UI - talks to the lamp over a WebSocket.
// The lamp sends a "schema" (modes + parameters); all controls are built from it.
'use strict';

const $ = (id) => document.getElementById(id);

const ui = {
	schema: null,
	state: null,
	presets: [],
	editing: false,
	dragging: new Set(),  // keys of sliders the user is touching right now
	sliders: new Map(),   // key -> {input, value}
	shownMode: null,
	wifi: null,
	restarting: false,    // firmware installed: reload the page when the lamp is back
	calib: null,          // measurements from the lamp
	calibCh: 0,           // colour being measured
	calibTile: null,      // board lit for measuring
	selected: new Set(),  // manual mode: board indices 0..29
	manualRgbw: [0, 0, 0, 0],
};

const MODE_MANUAL = 8;

// ------------------------------------------------------------------ socket
let ws = null;
let retry = 500;

function connect() {
	ws = new WebSocket(`ws://${location.host}/ws`);
	ws.onopen = () => {
		if (ui.restarting) location.reload();  // new firmware may bring a new page
		retry = 500;
		setStatus(true);
	};
	ws.onclose = () => {
		setStatus(false);
		setTimeout(connect, retry);
		retry = Math.min(retry * 2, 5000);
	};
	ws.onmessage = (e) => {
		const msg = JSON.parse(e.data);
		if (msg.type === 'schema') {
			ui.schema = msg;
			ui.shownMode = null;
			buildGlobal();
			buildModes();
			renderFirmware(msg);
		} else if (msg.type === 'state') {
			ui.state = msg;
			render();
		} else if (msg.type === 'presets') {
			ui.presets = msg.list;
			renderPresets();
		} else if (msg.type === 'calib') {
			ui.calib = msg;
			if (!msg.show) ui.calibTile = null;  // switched off (timeout or another phone)
			renderCalib();
		} else if (msg.type === 'wifi') {
			ui.wifi = msg;
			renderWifi();
		}
	};
}

function send(obj) {
	if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

function setStatus(online) {
	const s = $('status');
	s.textContent = online ? 'connected' : 'reconnecting…';
	s.classList.toggle('offline', !online);
}

// ------------------------------------------------------------------ controls
// sliders send while dragging, at most every 50 ms, plus the final value.
// Without onValue the slider is a lamp parameter ("set" message, updated from the state).
function makeSlider(param, onValue) {
	const wrap = document.createElement('label');
	wrap.className = 'slider';
	const head = document.createElement('div');
	head.className = 'row';
	const name = document.createElement('span');
	name.textContent = param.label;
	const value = document.createElement('span');
	value.className = 'value';
	head.append(name, value);

	const input = document.createElement('input');
	input.type = 'range';
	input.min = param.min;
	input.max = param.max;
	if (param.color) input.style.setProperty('--tint', param.color);

	let timer = null;
	let pending = false;
	let lastSent = null;
	const show = () => (value.textContent = input.value + (param.unit ? ' ' + param.unit : ''));
	const flush = () => {
		timer = null;
		if (pending) {
			pending = false;
			const v = Number(input.value);
			if (v === lastSent) return;
			lastSent = v;
			if (onValue) onValue(v);
			else send({ type: 'set', key: param.key, value: v });
			timer = setTimeout(flush, 50);
		}
	};
	input.addEventListener('pointerdown', () => ui.dragging.add(param.key));
	input.addEventListener('input', () => {
		show();
		pending = true;
		if (!timer) flush();
	});
	input.addEventListener('change', () => {
		ui.dragging.delete(param.key);
		pending = true;
		if (!timer) flush();
	});

	wrap.append(head, input);
	const set = (v) => {
		input.value = v;
		show();
	};
	if (onValue) Object.assign(wrap, { set, busy: () => ui.dragging.has(param.key) });
	else ui.sliders.set(param.key, { input, set });
	return wrap;
}

// 0..1 parameters: a checkbox, sent on every click
function makeToggle(param) {
	const wrap = document.createElement('label');
	wrap.className = 'toggle';
	const input = document.createElement('input');
	input.type = 'checkbox';
	if (param.color) input.style.setProperty('--tint', param.color);
	input.addEventListener('change', () => send({ type: 'set', key: param.key, value: input.checked ? 1 : 0 }));
	const name = document.createElement('span');
	name.textContent = param.label;
	wrap.append(input, name);
	ui.sliders.set(param.key, { input, set: (v) => (input.checked = !!v) });
	return wrap;
}

const isToggle = (p) => p.min === 0 && p.max === 1;

// sliders, with consecutive checkboxes grouped in one row
function makeControls(params) {
	const out = [];
	let row = null;
	for (const p of params) {
		if (!isToggle(p)) {
			out.push(makeSlider(p));
			row = null;
			continue;
		}
		if (!row) {
			row = document.createElement('div');
			row.className = 'toggles';
			out.push(row);
		}
		row.append(makeToggle(p));
	}
	return out;
}

function buildGlobal() {
	const box = $('global');
	box.replaceChildren(...makeControls(ui.schema.params.filter((p) => p.mode === -1)));
}

function buildModes() {
	const box = $('modes');
	box.replaceChildren(
		...ui.schema.modes.map((m) => {
			const b = document.createElement('button');
			b.type = 'button';
			b.textContent = m.name;
			b.dataset.mode = m.id;
			b.addEventListener('click', () => send({ type: 'mode', mode: m.id }));
			return b;
		})
	);
}

function buildParams(mode) {
	const params = ui.schema.params.filter((p) => p.mode === mode);
	for (const p of ui.schema.params) if (p.mode === ui.shownMode) ui.sliders.delete(p.key);
	$('params').replaceChildren(...makeControls(params));
	$('paramsCard').hidden = params.length === 0;
	$('paramsTitle').textContent = ui.schema.modes.find((m) => m.id === mode).name;
	ui.shownMode = mode;
}

// ------------------------------------------------------------------ rendering
function render() {
	if (!ui.schema || !ui.state) return;
	const mode = ui.state.mode;
	for (const b of $('modes').children) b.classList.toggle('active', Number(b.dataset.mode) === mode);
	if (mode !== ui.shownMode) buildParams(mode);

	for (const [key, s] of ui.sliders) {
		if (ui.dragging.has(key)) continue;
		const v = ui.state.values[key];
		if (v !== undefined) s.set(v);
	}
	renderPresets();
	renderManual();
}

// ------------------------------------------------------------------ manual mode
// Tap boards to select them; the R/G/B/W sliders then set all selected boards.
const MANUAL_CHANNELS = [
	{ key: 'manual.r', label: 'Red', color: '#ff4b4b' },
	{ key: 'manual.g', label: 'Green', color: '#4bd36b' },
	{ key: 'manual.b', label: 'Blue', color: '#4b7bff' },
	{ key: 'manual.w', label: 'White', color: '#f4f1e8' },
];

function manualColour(t) {
	const hex = ui.state.manual || '';
	return [0, 1, 2, 3].map((c) => parseInt(hex.substr((t * 4 + c) * 2, 2), 16) || 0);
}

// rough on-screen look of an RGBW value: white adds to all three, dim stays visible
function cssColour([r, g, b, w]) {
	const f = (x) => Math.round(40 + Math.min(255, x + w) * (215 / 255));
	return `rgb(${f(r)}, ${f(g)}, ${f(b)})`;
}

function buildManual() {
	const tiles = $('tiles');
	for (let t = 0; t < 30; t++) {
		const b = document.createElement('button');
		b.type = 'button';
		b.textContent = t + 1;
		b.addEventListener('click', () => {
			if (ui.selected.has(t)) ui.selected.delete(t);
			else ui.selected.add(t);
			pickSelectedColour();
			renderManual();
		});
		tiles.append(b);
	}
	const sendColour = () =>
		send({ type: 'manual.set', tiles: [...ui.selected], rgbw: ui.manualRgbw });
	$('manualSliders').replaceChildren(
		...MANUAL_CHANNELS.map((ch, c) =>
			makeSlider({ ...ch, min: 0, max: 255 }, (v) => {
				ui.manualRgbw[c] = v;
				if (ui.selected.size) sendColour();
			})
		)
	);
	const select = (list) => {
		ui.selected = new Set(list);
		pickSelectedColour();
		renderManual();
	};
	$('manualAll').addEventListener('click', () => select([...Array(30).keys()]));
	$('manualNone').addEventListener('click', () => select([]));
	$('manualOff').addEventListener('click', () => {
		if (!ui.selected.size) return;
		ui.manualRgbw = [0, 0, 0, 0];
		sendColour();
		renderManual();
	});
}

// sliders show the colour of the first selected board
function pickSelectedColour() {
	if (ui.selected.size) ui.manualRgbw = manualColour(Math.min(...ui.selected));
}

function renderManual() {
	const show = ui.state.mode === MODE_MANUAL;
	$('manualCard').hidden = !show;
	if (!show) return;
	const tiles = $('tiles').children;
	for (let t = 0; t < 30; t++) {
		const rgbw = manualColour(t);
		tiles[t].style.background = cssColour(rgbw);
		tiles[t].classList.toggle('dark', Math.max(...rgbw) < 100);
		tiles[t].classList.toggle('selected', ui.selected.has(t));
	}
	const n = ui.selected.size;
	$('manualSel').textContent = n ? `${n} selected` : 'tap boards to select';
	const sliders = $('manualSliders').children;
	for (let c = 0; c < 4; c++) {
		sliders[c].classList.toggle('disabled', !n);
		if (!sliders[c].busy()) sliders[c].set(ui.manualRgbw[c]);
	}
}

// ------------------------------------------------------------------ calibration
const CHANNELS = [
	{ name: 'Red', color: '#ff4b4b' },
	{ name: 'Green', color: '#4bd36b' },
	{ name: 'Blue', color: '#4b7bff' },
	{ name: 'White', color: '#f4f1e8' },
];

function buildCalib() {
	$('calibChans').replaceChildren(
		...CHANNELS.map((ch, c) => {
			const b = document.createElement('button');
			b.type = 'button';
			b.textContent = ch.name;
			b.style.setProperty('--tint', ch.color);
			b.addEventListener('click', () => {
				ui.calibCh = c;
				if (ui.calibTile !== null) calibShow(ui.calibTile);
				renderCalib();
			});
			return b;
		})
	);
	for (let t = 0; t < 30; t++) {
		const b = document.createElement('button');
		b.type = 'button';
		const num = document.createElement('b');
		num.textContent = t + 1;
		b.append(num, document.createElement('small'));
		b.addEventListener('click', () => calibShow(t));
		$('calibTiles').append(b);
	}
	$('calibForm').addEventListener('submit', (e) => {
		e.preventDefault();
		const lux = parseFloat($('calibLux').value.replace(',', '.'));
		if (ui.calibTile === null || !(lux >= 0)) return;
		send({ type: 'calib.lux', tile: ui.calibTile, ch: ui.calibCh, lux });
		$('calibLux').value = '';
		// all four colours of a board, then the next board: the meter stays put for four readings
		if (ui.calibCh < 3) {
			ui.calibCh++;
			calibShow(ui.calibTile);
		} else {
			ui.calibCh = 0;
			calibShow((ui.calibTile + 1) % 30);
		}
	});
	$('calibStop').addEventListener('click', calibStop);
	$('calibReset').addEventListener('click', () => {
		if (confirm(`Clear all ${CHANNELS[ui.calibCh].name.toLowerCase()} measurements?`))
			send({ type: 'calib.reset', ch: ui.calibCh });
	});
	const enable = () =>
		send({ type: 'calib.enable', boards: $('calibBoards').checked, colours: $('calibColours').checked });
	$('calibBoards').addEventListener('change', enable);
	$('calibColours').addEventListener('change', enable);
	$('calib').addEventListener('toggle', () => {
		if (!$('calib').open) calibStop();
	});
}

function calibShow(t) {
	ui.calibTile = t;
	send({ type: 'calib.show', tile: t, ch: ui.calibCh });
	renderCalib();
	$('calibLux').focus();
}

function calibStop() {
	ui.calibTile = null;
	send({ type: 'calib.stop' });
	renderCalib();
}

function renderCalib() {
	const cal = ui.calib;
	if (!cal) return;
	const c = ui.calibCh;
	const lux = cal.lux.map((l) => l[c]);
	const measured = lux.filter((v) => v > 0).sort((a, b) => a - b);
	const median = measured.length ? measured[measured.length >> 1] : 0;

	[...$('calibChans').children].forEach((b, i) => b.classList.toggle('active', i === c));
	[...$('calibTiles').children].forEach((b, t) => {
		b.lastChild.textContent = lux[t] > 0 ? fmtLux(lux[t]) : '–';
		b.classList.toggle('done', lux[t] > 0);
		b.classList.toggle('outlier', lux[t] > 0 && lux[t] < 0.7 * median);
		b.classList.toggle('selected', t === ui.calibTile);
	});

	const input = $('calibLux');
	input.disabled = ui.calibTile === null;
	input.placeholder =
		ui.calibTile === null ? 'tap a board first' : `lux for board ${ui.calibTile + 1}, ${CHANNELS[c].name.toLowerCase()}`;

	const lines = [];
	if (measured.length) {
		const used = measured.filter((v) => v >= 0.7 * median);
		lines.push(`${CHANNELS[c].name}: ${measured.length} of 30 measured, weakest ${fmtLux(used[0])} lx.`);
		const out = measured.length - used.length;
		if (out) lines.push(`${out} board${out > 1 ? 's are' : ' is'} far below the others (crossed out) and left at full power - check that LED.`);
	} else {
		lines.push(`${CHANNELS[c].name}: nothing measured yet.`);
	}
	if (cal.balance.slice(0, 3).some((b) => b !== 1000))
		lines.push(`Colour balance: red ${cal.balance[0] / 10} %, green ${cal.balance[1] / 10} %, blue ${cal.balance[2] / 10} %.`);
	else lines.push('Colour balance needs red, green and blue measured (one board is enough).');
	$('calibInfo').textContent = lines.join(' ');

	$('calibBoards').checked = cal.boards;
	$('calibColours').checked = cal.colours;
	const any = cal.lux.some((l) => l.some((v) => v > 0));
	$('calibState').textContent = ui.calibTile !== null ? 'measuring' : !any ? 'not measured' : cal.boards || cal.colours ? 'on' : 'off';
}

const fmtLux = (v) => (v >= 100 ? Math.round(v) : +v.toPrecision(3)).toString();

// ------------------------------------------------------------------ firmware upload
function renderFirmware(s) {
	$('fwVersion').textContent = s.version ? `v${s.version}` : '';
	$('fwInfo').textContent = s.version
		? `Running version ${s.version}, built ${s.built}, on ${s.chip}. Uses ${s.appUsed} % of the space for firmware.`
		: '';
}

function buildFirmware() {
	const status = (text) => ($('fwStatus').textContent = text);
	$('fwForm').addEventListener('submit', (e) => {
		e.preventDefault();
		const file = $('fwFile').files[0];
		if (!file) return status('Choose the .bin file first.');
		if (/merged|bootloader|partitions/i.test(file.name))
			return status('That is not the app file - use diskohedron.ino.bin (not .merged / .bootloader / .partitions).');
		const xhr = new XMLHttpRequest();
		const bar = $('fwProgress');
		const button = $('fwForm').querySelector('button');
		xhr.open('POST', '/update');
		xhr.setRequestHeader('X-Password', $('fwPass').value);
		xhr.upload.onprogress = (ev) => {
			if (!ev.lengthComputable) return;
			bar.value = ev.loaded / ev.total;
			status(`Uploading… ${Math.round((100 * ev.loaded) / ev.total)} %`);
		};
		xhr.onload = () => {
			button.disabled = false;
			let reply = {};
			try {
				reply = JSON.parse(xhr.responseText);
			} catch {}
			if (xhr.status === 200 && reply.ok) {
				ui.restarting = true;
				status('Installed. The lamp restarts - this page reloads in a few seconds.');
			} else {
				bar.hidden = true;
				status(`Not installed: ${reply.error || 'error ' + xhr.status}. The lamp keeps its current firmware.`);
			}
		};
		xhr.onerror = () => {
			button.disabled = false;
			bar.hidden = true;
			status('Connection lost during the upload. The lamp keeps its current firmware - try again.');
		};
		const form = new FormData();
		form.append('firmware', file, file.name);
		bar.hidden = false;
		bar.value = 0;
		button.disabled = true;
		status('Uploading…');
		xhr.send(form);
	});
}

function renderPresets() {
	const box = $('presets');
	if (!ui.schema) return;
	const modeName = (id) => ui.schema.modes.find((m) => m.id === id)?.name ?? '';
	box.replaceChildren(
		...ui.presets.map((p) => {
			const b = document.createElement('button');
			b.type = 'button';
			b.className = 'chip';
			if (ui.state && p.mode === ui.state.mode) b.classList.add('same-mode');
			const name = document.createElement('span');
			name.textContent = p.name;
			const tag = document.createElement('small');
			tag.textContent = modeName(p.mode);
			b.append(name, tag);
			if (ui.editing) {
				b.classList.add('delete');
				b.addEventListener('click', () => {
					if (confirm(`Delete preset "${p.name}"?`)) send({ type: 'preset.delete', name: p.name });
				});
			} else {
				b.addEventListener('click', () => send({ type: 'preset.load', name: p.name }));
			}
			return b;
		})
	);
	if (!ui.presets.length) box.textContent = 'No presets yet.';
}

// ------------------------------------------------------------------ presets
$('savePreset').addEventListener('submit', (e) => {
	e.preventDefault();
	const name = $('presetName').value.trim();
	if (!name) return;
	send({ type: 'preset.save', name });
	$('presetName').value = '';
	$('presetName').blur();
});

$('editPresets').addEventListener('click', () => {
	ui.editing = !ui.editing;
	$('editPresets').textContent = ui.editing ? 'done' : 'edit';
	renderPresets();
});

// ------------------------------------------------------------------ home WiFi
const WIFI_STATE = { off: 'off', connecting: 'connecting…', connected: 'connected', failed: 'not reachable' };

function renderWifi() {
	const w = ui.wifi;
	$('wifiState').textContent = WIFI_STATE[w.status] ?? w.status;
	const info = $('wifiInfo');
	info.replaceChildren();
	if (w.status === 'connected') {
		info.append(`Connected to ${w.ssid}. Open `);
		for (const [i, url] of [`http://${w.hostname}.local`, `http://${w.ip}`].entries()) {
			if (i) info.append(' or ');
			const a = document.createElement('a');
			a.href = a.textContent = url;
			info.append(a);
		}
	} else if (w.status === 'connecting') {
		info.textContent = `Connecting to ${w.ssid}… Your phone may drop off the lamp's hotspot for a moment.`;
	} else if (w.status === 'failed') {
		info.textContent = `${w.ssid} not reachable - wrong password, or out of range. Trying again every minute.`;
	}
	info.hidden = !info.textContent;
	$('wifiForget').hidden = w.status === 'off';

	const ssid = $('wifiSsid');
	if (document.activeElement !== ssid && !ssid.value) ssid.value = w.ssid;
	$('wifiNets').replaceChildren(
		...w.networks.map((n) => {
			const o = document.createElement('option');
			o.value = n;
			return o;
		})
	);
	$('wifiScan').textContent = w.scanning ? 'scanning…' : w.networks.length ? `${w.networks.length} networks found - scan again` : 'scan for networks';
}

$('wifiForm').addEventListener('submit', (e) => {
	e.preventDefault();
	const ssid = $('wifiSsid').value.trim();
	if (!ssid) return;
	send({ type: 'wifi.set', ssid, pass: $('wifiPass').value });
	$('wifiPass').value = '';
	document.activeElement.blur();
});

$('wifiScan').addEventListener('click', () => send({ type: 'wifi.scan' }));

$('wifiForget').addEventListener('click', () => {
	if (!confirm(`Forget ${ui.wifi.ssid}? The lamp then only has its own hotspot.`)) return;
	send({ type: 'wifi.set', ssid: '', pass: '' });
	$('wifiSsid').value = '';
});

buildManual();
buildCalib();
buildFirmware();
connect();
