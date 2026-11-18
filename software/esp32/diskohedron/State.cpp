#include "State.h"
#include "LedChain.h"

extern LedChain leds;  // flash writes pause its interrupt: see holdForFlash()
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <algorithm>

const ModeDef MODES[MODE_COUNT] = {
	{MODE_OFF, "Off"},
	{MODE_FLICKER, "Flicker"},
	{MODE_NOISE, "Colour noise"},
	{MODE_LIFE, "Game of Life"},
	{MODE_SWEEP, "Sweep"},
	{MODE_COLOR, "Colour"},
	{MODE_RIPPLE, "Ripples"},
	{MODE_CHECK, "Check"},
	{MODE_MANUAL, "Manual"},
};

const ParamDef PARAMS[P_COUNT] = {
	{"brightness", MODE_GLOBAL, "Brightness", 0, 100, 100, "%", ""},
	{"power", MODE_GLOBAL, "Lamp on", 0, 1, 1, "", ""},  // off = dark, the mode is kept
	{"flicker.level", MODE_FLICKER, "Intensity", 0, 255, 40, "", "#f4f1e8"},
	{"flicker.rate", MODE_FLICKER, "Interval", 10, 500, 40, "ms", ""},
	{"noise.r", MODE_NOISE, "Red", 0, 255, 255, "", "#ff4b4b"},
	{"noise.g", MODE_NOISE, "Green", 0, 255, 255, "", "#4bd36b"},
	{"noise.b", MODE_NOISE, "Blue", 0, 255, 255, "", "#4b7bff"},
	{"noise.w", MODE_NOISE, "White", 0, 255, 255, "", "#f4f1e8"},
	{"noise.rate", MODE_NOISE, "Interval", 10, 500, 40, "ms", ""},
	{"life.fade", MODE_LIFE, "Generation time", 100, 10000, 3000, "ms", ""},
	{"life.reseed", MODE_LIFE, "Reseed every", 5, 600, 60, "s", ""},
	{"sweep.level", MODE_SWEEP, "Intensity", 0, 255, 255, "", ""},
	{"sweep.speed", MODE_SWEEP, "Speed", -100, 100, 30, "", ""},
	{"color.r", MODE_COLOR, "Red", 0, 255, 0, "", "#ff4b4b"},
	{"color.g", MODE_COLOR, "Green", 0, 255, 0, "", "#4bd36b"},
	{"color.b", MODE_COLOR, "Blue", 0, 255, 0, "", "#4b7bff"},
	{"color.w", MODE_COLOR, "White", 0, 255, 255, "", "#f4f1e8"},
	{"ripple.rate", MODE_RIPPLE, "Drops per minute", 1, 60, 8, "", ""},
	{"ripple.speed", MODE_RIPPLE, "Speed", 2, 90, 12, "°/s", ""},
	{"ripple.width", MODE_RIPPLE, "Ring width", 10, 90, 35, "°", ""},
	{"ripple.sat", MODE_RIPPLE, "Colourfulness", 0, 100, 80, "%", ""},
	// 0..1 parameters show up as checkboxes in the UI
	{"check.time", MODE_CHECK, "Time per board", 100, 3000, 500, "ms", ""},
	{"check.r", MODE_CHECK, "Red", 0, 1, 1, "", "#ff4b4b"},
	{"check.g", MODE_CHECK, "Green", 0, 1, 1, "", "#4bd36b"},
	{"check.b", MODE_CHECK, "Blue", 0, 1, 1, "", "#4b7bff"},
	{"check.w", MODE_CHECK, "White", 0, 1, 1, "", "#f4f1e8"},
};

static int paramIndex(const char *key) {
	for (int i = 0; i < P_COUNT; i++)
		if (strcmp(PARAMS[i].key, key) == 0) return i;
	return -1;
}

State::State() {
	for (int i = 0; i < P_COUNT; i++) m_values[i] = PARAMS[i].def;
	for (auto &t : m_gain)
		for (auto &g : t) g = 1000;
}

void State::begin() {
	if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
	loadState();
	loadCalib();
	loadPresets();
	if (m_presetsVersion < PRESETS_VERSION) {  // first boot, or firmware with new built-in presets
		addBuiltinPresets(m_presetsVersion);
		m_presetsVersion = PRESETS_VERSION;
		savePresets();
	}
}

void State::loop() {
	if (m_saveAt && (int32_t)(millis() - m_saveAt) >= 0) {
		m_saveAt = 0;
		saveState();
	}
	if (m_showTile >= 0 && (int32_t)(millis() - m_showUntil) >= 0) {  // forgotten measurement
		m_showTile = -1;
		m_calibChanged = true;
	}
}

void State::setMode(int m) {
	if (m < 0 || m >= MODE_COUNT) return;
	m_mode = (Mode)m;
}

bool State::setParam(const char *key, int value) {
	int i = paramIndex(key);
	if (i < 0) return false;
	m_values[i] = constrain(value, PARAMS[i].min, PARAMS[i].max);
	return true;
}

// ------------------------------------------------------------------ messages
bool State::handle(const char *json, size_t len, bool &presetsChanged) {
	presetsChanged = false;
	JsonDocument doc;
	if (deserializeJson(doc, json, len)) return false;
	const char *type = doc["type"] | "";

	if (!strcmp(type, "mode")) {
		setMode(doc["mode"] | -1);
	} else if (!strcmp(type, "set")) {
		if (!setParam(doc["key"] | "", doc["value"] | 0)) return false;
	} else if (!strcmp(type, "manual.set")) {
		// {"tiles": [0, 4, ...], "rgbw": [r, g, b, w]}: the same colour for all listed boards
		JsonArray rgbw = doc["rgbw"];
		for (int t : doc["tiles"].as<JsonArray>()) {
			if (t < 0 || t >= NUM_TILES) continue;
			for (int c = 0; c < 4; c++) m_manual[t * 4 + c] = constrain(rgbw[c] | 0, 0, 255);
		}
	} else if (!strncmp(type, "calib.", 6)) {
		handleCalib(type + 6, doc);
		m_calibChanged = true;
		return false;
	} else if (!strcmp(type, "preset.load")) {
		loadPreset(doc["name"] | "");
	} else if (!strcmp(type, "preset.save")) {
		savePreset(doc["name"] | "");
		presetsChanged = true;
		return false;
	} else if (!strcmp(type, "preset.delete")) {
		presetsChanged = deletePreset(doc["name"] | "");
		return false;
	} else {
		return false;
	}
	markDirty();
	return true;
}

// ------------------------------------------------------------------ presets
void State::loadPreset(const char *name) {
	for (auto &p : m_presets) {
		if (p.name != name) continue;
		m_mode = p.mode;
		for (auto &kv : p.values) m_values[kv.first] = kv.second;
		if (p.tiles.length()) setManualHex(p.tiles.c_str());
		return;
	}
}

void State::savePreset(const char *name) {
	String n = String(name);
	n.trim();
	if (!n.length() || n.length() > 32) return;
	Preset p{n, m_mode, {}};
	for (int i = 0; i < P_COUNT; i++)
		if (PARAMS[i].mode == m_mode) p.values.push_back({(uint8_t)i, m_values[i]});
	if (m_mode == MODE_MANUAL) p.tiles = manualHex();
	deletePreset(name);  // overwrite same name
	m_presets.push_back(p);
	savePresets();
}

bool State::deletePreset(const char *name) {
	for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
		if (it->name == name) {
			m_presets.erase(it);
			savePresets();
			return true;
		}
	}
	return false;
}

// converted from the old www/js/presets.js (old full scale 165 -> 255)
// `since` = PRESETS_VERSION that introduced the preset. Lamps that already have presets get
// only the newer ones on a firmware update, so presets you deleted don't come back.
void State::addBuiltinPresets(int after) {
	struct Builtin {
		int since;
		Preset p;
	};
	const Builtin builtins[] = {
		{1, {"Flicker full", MODE_FLICKER, {{P_FLICKER_LEVEL, 255}, {P_FLICKER_RATE, 40}}}},
		{1, {"Flicker low", MODE_FLICKER, {{P_FLICKER_LEVEL, 8}, {P_FLICKER_RATE, 60}}}},
		{1, {"Krazy red", MODE_NOISE, {{P_NOISE_R, 255}, {P_NOISE_G, 0}, {P_NOISE_B, 0}, {P_NOISE_W, 0}, {P_NOISE_RATE, 40}}}},
		{1, {"Krazy green", MODE_NOISE, {{P_NOISE_R, 0}, {P_NOISE_G, 255}, {P_NOISE_B, 0}, {P_NOISE_W, 0}, {P_NOISE_RATE, 40}}}},
		{1, {"Krazy blue", MODE_NOISE, {{P_NOISE_R, 0}, {P_NOISE_G, 0}, {P_NOISE_B, 255}, {P_NOISE_W, 0}, {P_NOISE_RATE, 40}}}},
		{1, {"Redred", MODE_NOISE, {{P_NOISE_R, 255}, {P_NOISE_G, 255}, {P_NOISE_B, 247}, {P_NOISE_W, 249}, {P_NOISE_RATE, 40}}}},
		{1, {"Life slow", MODE_LIFE, {{P_LIFE_FADE, 5000}, {P_LIFE_RESEED, 120}}}},
		{1, {"Life fast", MODE_LIFE, {{P_LIFE_FADE, 600}, {P_LIFE_RESEED, 30}}}},
		{1, {"Fast sweep", MODE_SWEEP, {{P_SWEEP_LEVEL, 255}, {P_SWEEP_SPEED, 80}}}},
		{1, {"Red", MODE_COLOR, {{P_COLOR_R, 255}, {P_COLOR_G, 0}, {P_COLOR_B, 0}, {P_COLOR_W, 0}}}},
		{1, {"Warm white", MODE_COLOR, {{P_COLOR_R, 60}, {P_COLOR_G, 20}, {P_COLOR_B, 0}, {P_COLOR_W, 255}}}},
		{2, {"Cold white", MODE_COLOR, {{P_COLOR_R, 0}, {P_COLOR_G, 25}, {P_COLOR_B, 90}, {P_COLOR_W, 255}}}},
		{1, {"Slow rain", MODE_RIPPLE, {{P_RIPPLE_RATE, 8}, {P_RIPPLE_SPEED, 12}, {P_RIPPLE_WIDTH, 35}, {P_RIPPLE_SAT, 80}}}},
		{1, {"White drops", MODE_RIPPLE, {{P_RIPPLE_RATE, 4}, {P_RIPPLE_SPEED, 8}, {P_RIPPLE_WIDTH, 45}, {P_RIPPLE_SAT, 0}}}},
		{1, {"Storm", MODE_RIPPLE, {{P_RIPPLE_RATE, 40}, {P_RIPPLE_SPEED, 45}, {P_RIPPLE_WIDTH, 20}, {P_RIPPLE_SAT, 100}}}},
		{3, {"checklamp", MODE_CHECK, {{P_CHECK_TIME, 500}, {P_CHECK_R, 1}, {P_CHECK_G, 1}, {P_CHECK_B, 1}, {P_CHECK_W, 1}}}},
	};
	for (auto &b : builtins) {
		if (b.since <= after) continue;
		bool exists = false;
		for (auto &p : m_presets) exists |= p.name == b.p.name;
		if (!exists) m_presets.push_back(b.p);
	}
}

// ------------------------------------------------------------------ JSON out
String State::schemaJson() const {
	JsonDocument doc;
	doc["type"] = "schema";
	// Firmware section of the page
	doc["version"] = FIRMWARE_VERSION;
	doc["built"] = __DATE__ " " __TIME__;
	doc["chip"] = ESP.getChipModel();
	doc["appUsed"] = (int)(100ull * ESP.getSketchSize() / (ESP.getSketchSize() + ESP.getFreeSketchSpace()));
	JsonArray modes = doc["modes"].to<JsonArray>();
	for (auto &m : MODES) {
		JsonObject o = modes.add<JsonObject>();
		o["id"] = m.id;
		o["name"] = m.name;
	}
	JsonArray params = doc["params"].to<JsonArray>();
	for (auto &p : PARAMS) {
		JsonObject o = params.add<JsonObject>();
		o["key"] = p.key;
		o["mode"] = p.mode == MODE_GLOBAL ? -1 : (int)p.mode;
		o["label"] = p.label;
		o["min"] = p.min;
		o["max"] = p.max;
		o["unit"] = p.unit;
		o["color"] = p.color;
	}
	String out;
	serializeJson(doc, out);
	return out;
}

String State::stateJson() const {
	JsonDocument doc;
	doc["type"] = "state";
	doc["on"] = m_values[P_POWER] != 0;
	doc["mode"] = (int)m_mode;
	doc["modeName"] = MODES[m_mode].name;
	JsonObject values = doc["values"].to<JsonObject>();
	for (int i = 0; i < P_COUNT; i++) values[PARAMS[i].key] = m_values[i];
	doc["manual"] = manualHex();
	String out;
	serializeJson(doc, out);
	return out;
}

String State::presetsJson() const {
	JsonDocument doc;
	doc["type"] = "presets";
	JsonArray list = doc["list"].to<JsonArray>();
	for (auto &p : m_presets) {
		JsonObject o = list.add<JsonObject>();
		o["name"] = p.name;
		o["mode"] = (int)p.mode;
	}
	String out;
	serializeJson(doc, out);
	return out;
}

// ------------------------------------------------------------------ files
void State::saveState() {
	JsonDocument doc;
	doc["mode"] = (int)m_mode;
	for (int i = 0; i < P_COUNT; i++) doc["values"][PARAMS[i].key] = m_values[i];
	doc["manual"] = manualHex();
	leds.holdForFlash(80);
	File f = LittleFS.open("/state.json", "w");
	if (!f) return;
	serializeJson(doc, f);
	f.close();
}

void State::loadState() {
	File f = LittleFS.open("/state.json", "r");
	if (!f) return;
	JsonDocument doc;
	if (!deserializeJson(doc, f)) {
		setMode(doc["mode"] | (int)m_mode);
		JsonObject values = doc["values"];
		for (JsonPair kv : values) setParam(kv.key().c_str(), kv.value() | 0);
		setManualHex(doc["manual"] | "");
	}
	f.close();
}

void State::savePresets() {
	JsonDocument doc;
	doc["version"] = m_presetsVersion;
	JsonArray list = doc["presets"].to<JsonArray>();
	for (auto &p : m_presets) {
		JsonObject o = list.add<JsonObject>();
		o["name"] = p.name;
		o["mode"] = (int)p.mode;
		JsonObject vals = o["values"].to<JsonObject>();
		for (auto &kv : p.values) vals[PARAMS[kv.first].key] = kv.second;
		if (p.tiles.length()) o["tiles"] = p.tiles;
	}
	leds.holdForFlash(180);
	File f = LittleFS.open("/presets.json", "w");
	if (!f) return;
	serializeJson(doc, f);
	f.close();
}

void State::loadPresets() {
	File f = LittleFS.open("/presets.json", "r");
	if (!f) return;
	JsonDocument doc;
	if (!deserializeJson(doc, f)) {
		m_presets.clear();
		// older firmware stored a bare array (= version 1)
		JsonArray list = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["presets"].as<JsonArray>();
		m_presetsVersion = doc["version"] | 1;
		for (JsonObject o : list) {
			int mode = o["mode"] | -1;
			if (mode < 0 || mode >= MODE_COUNT) continue;
			Preset p{String(o["name"] | "?"), (Mode)mode, {}};
			JsonObject values = o["values"];
			for (JsonPair kv : values) {
				int i = paramIndex(kv.key().c_str());
				if (i >= 0) p.values.push_back({(uint8_t)i, (int16_t)constrain((int)(kv.value() | 0), PARAMS[i].min, PARAMS[i].max)});
			}
			p.tiles = o["tiles"] | "";
			m_presets.push_back(p);
		}
	}
	f.close();
}

// ------------------------------------------------------------------ manual colours
// as one hex string, RRGGBBWW per board, board 1 first (compact in JSON and files)
String State::manualHex() const {
	static const char HEX_DIGITS[] = "0123456789abcdef";
	String out;
	out.reserve(sizeof(m_manual) * 2);
	for (uint8_t b : m_manual) {
		out += HEX_DIGITS[b >> 4];
		out += HEX_DIGITS[b & 15];
	}
	return out;
}

void State::setManualHex(const char *hex) {
	if (strlen(hex) != sizeof(m_manual) * 2) return;
	for (size_t i = 0; i < sizeof(m_manual); i++) {
		char byte[3] = {hex[i * 2], hex[i * 2 + 1], 0};
		m_manual[i] = strtoul(byte, nullptr, 16);
	}
}

// ------------------------------------------------------------------ calibration
static const uint32_t CALIB_SHOW_MS = 300000;  // measuring light switches itself off after 5 min

void State::handleCalib(const char *cmd, JsonDocument &doc) {
	int tile = doc["tile"] | -1, ch = doc["ch"] | -1;
	bool valid = tile >= 0 && tile < NUM_TILES && ch >= 0 && ch < 4;
	if (!strcmp(cmd, "show") && valid) {
		m_showTile = tile;
		m_showCh = ch;
		m_showUntil = millis() + CALIB_SHOW_MS;
	} else if (!strcmp(cmd, "stop")) {
		m_showTile = -1;
	} else if (!strcmp(cmd, "lux") && valid) {
		m_lux[tile][ch] = max(0.0f, doc["lux"] | 0.0f);
		computeGains();
		saveCalib();
	} else if (!strcmp(cmd, "enable")) {
		m_evenBoards = doc["boards"] | m_evenBoards;
		m_balanceColours = doc["colours"] | m_balanceColours;
		computeGains();
		saveCalib();
	} else if (!strcmp(cmd, "reset") && ch >= 0 && ch < 4) {
		for (auto &t : m_lux) t[ch] = 0;
		computeGains();
		saveCalib();
	}
}

bool State::calibShow(int &tile, int &ch) const {
	if (m_showTile < 0) return false;
	tile = m_showTile;
	ch = m_showCh;
	return true;
}

// Two corrections, both only ever dim:
// - even out boards: per colour, every LED is dimmed to the weakest one, ignoring outliers
//   below 70 % of the median (a nearly dead LED would otherwise dim the whole lamp). Those
//   and unmeasured LEDs stay at full power.
// - balance colours: red, green and blue (median over the boards) are dimmed until their
//   brightness ratio is that of sRGB white (Y = 0.2126 R + 0.7152 G + 0.0722 B), so equal
//   R/G/B values give a neutral white. White is left alone: it is the main light.
void State::computeGains() {
	static const float WHITE_RATIO[3] = {0.2126f, 0.7152f, 0.0722f};
	float median[4] = {};
	for (int ch = 0; ch < 4; ch++) {
		float v[NUM_TILES];
		int n = 0;
		for (int t = 0; t < NUM_TILES; t++)
			if (m_lux[t][ch] > 0) v[n++] = m_lux[t][ch];
		std::sort(v, v + n);
		median[ch] = n ? v[n / 2] : 0;
		float floor = 0.7f * median[ch];
		float ref = 0;
		for (int i = 0; i < n && !ref; i++)
			if (v[i] >= floor) ref = v[i];
		for (int t = 0; t < NUM_TILES; t++) {
			float lux = m_lux[t][ch];
			bool use = m_evenBoards && lux > 0 && lux >= floor;
			m_gain[t][ch] = use ? (uint16_t)lroundf(1000 * ref / lux) : 1000;
		}
	}

	for (auto &b : m_balance) b = 1000;
	if (median[0] > 0 && median[1] > 0 && median[2] > 0) {
		float k = 1e30f;  // the colour that is weakest relative to its share sets the level
		for (int ch = 0; ch < 3; ch++) k = min(k, median[ch] / WHITE_RATIO[ch]);
		for (int ch = 0; ch < 3; ch++) m_balance[ch] = lroundf(1000 * k * WHITE_RATIO[ch] / median[ch]);
	}
	if (m_balanceColours)
		for (auto &t : m_gain)
			for (int ch = 0; ch < 4; ch++) t[ch] = (uint32_t)t[ch] * m_balance[ch] / 1000;
}

String State::calibJson() const {
	JsonDocument doc;
	doc["type"] = "calib";
	doc["boards"] = m_evenBoards;
	doc["colours"] = m_balanceColours;
	JsonArray balance = doc["balance"].to<JsonArray>();
	for (uint16_t b : m_balance) balance.add(b);
	if (m_showTile >= 0) {
		doc["show"]["tile"] = m_showTile;
		doc["show"]["ch"] = m_showCh;
	}
	JsonArray lux = doc["lux"].to<JsonArray>(), gain = doc["gain"].to<JsonArray>();
	for (int t = 0; t < NUM_TILES; t++) {
		JsonArray l = lux.add<JsonArray>(), g = gain.add<JsonArray>();
		for (int ch = 0; ch < 4; ch++) {
			l.add(m_lux[t][ch]);
			g.add(m_gain[t][ch]);
		}
	}
	String out;
	serializeJson(doc, out);
	return out;
}

void State::saveCalib() {
	JsonDocument doc;
	doc["boards"] = m_evenBoards;
	doc["colours"] = m_balanceColours;
	JsonArray lux = doc["lux"].to<JsonArray>();
	for (auto &t : m_lux) {
		JsonArray l = lux.add<JsonArray>();
		for (float v : t) l.add(v);
	}
	leds.holdForFlash(100);
	File f = LittleFS.open("/calib.json", "w");
	if (!f) return;
	serializeJson(doc, f);
	f.close();
}

void State::loadCalib() {
	File f = LittleFS.open("/calib.json", "r");
	if (!f) return;
	JsonDocument doc;
	if (!deserializeJson(doc, f)) {
		m_evenBoards = doc["boards"] | true;
		m_balanceColours = doc["colours"] | true;
		JsonArray lux = doc["lux"];
		for (int t = 0; t < NUM_TILES && t < (int)lux.size(); t++)
			for (int ch = 0; ch < 4; ch++) m_lux[t][ch] = lux[t][ch] | 0.0f;
		computeGains();
	}
	f.close();
}
