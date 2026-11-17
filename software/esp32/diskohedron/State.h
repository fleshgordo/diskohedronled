/*
State - lamp mode, parameters and presets.

All parameters live in one table (PARAMS). The web UI receives this table as
"schema" and builds its sliders from it, so adding a parameter or a mode only
needs changes here and in Effects.cpp.
*/
#pragma once

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include "config.h"

enum Mode : uint8_t {
	MODE_OFF = 0,
	MODE_FLICKER = 1,
	MODE_NOISE = 2,
	MODE_LIFE = 3,
	MODE_SWEEP = 4,
	MODE_COLOR = 5,
	MODE_RIPPLE = 6,
	MODE_CHECK = 7,
	MODE_MANUAL = 8,
	MODE_COUNT,
	MODE_GLOBAL = 255,  // parameter belongs to all modes
};

// order must match PARAMS[] in State.cpp
enum Param : uint8_t {
	P_BRIGHTNESS,
	P_POWER,
	P_FLICKER_LEVEL,
	P_FLICKER_RATE,
	P_NOISE_R,
	P_NOISE_G,
	P_NOISE_B,
	P_NOISE_W,
	P_NOISE_RATE,
	P_LIFE_FADE,
	P_LIFE_RESEED,
	P_SWEEP_LEVEL,
	P_SWEEP_SPEED,
	P_COLOR_R,
	P_COLOR_G,
	P_COLOR_B,
	P_COLOR_W,
	P_RIPPLE_RATE,
	P_RIPPLE_SPEED,
	P_RIPPLE_WIDTH,
	P_RIPPLE_SAT,
	P_CHECK_TIME,
	P_CHECK_R,
	P_CHECK_G,
	P_CHECK_B,
	P_CHECK_W,
	P_COUNT,
};

struct ModeDef {
	Mode id;
	const char *name;
};

struct ParamDef {
	const char *key;
	Mode mode;
	const char *label;
	int16_t min, max, def;
	const char *unit;
	const char *color;  // slider tint in the UI, may be empty
};

struct Preset {
	String name;
	Mode mode;
	std::vector<std::pair<uint8_t, int16_t>> values;  // param index, value
	String tiles;                                     // manual mode: colours as hex, else empty
};

class State {
public:
	State();

	void begin();  // mount LittleFS, load state + presets
	void loop();   // delayed saving

	Mode mode() const { return m_mode; }
	int16_t v(Param p) const { return m_values[p]; }
	const uint8_t *manual(int tile) const { return m_manual + tile * 4; }  // R G B W

	// brightness calibration with a luxmeter (see computeGains()), per mille
	uint16_t gain(int tile, int ch) const { return m_gain[tile][ch]; }
	bool calibShow(int &tile, int &ch) const;  // true = show only this LED at full power (measuring)
	String calibJson() const;
	bool takeCalibChanged() {
		bool c = m_calibChanged;
		m_calibChanged = false;
		return c;
	}

	// handle one message from the web UI; returns true if the state changed
	bool handle(const char *json, size_t len, bool &presetsChanged);

	String schemaJson() const;
	String stateJson() const;
	String presetsJson() const;

private:
	void setMode(int m);
	bool setParam(const char *key, int value);
	void loadPreset(const char *name);
	void savePreset(const char *name);
	bool deletePreset(const char *name);
	void addBuiltinPresets(int after);  // built-in presets newer than version `after`
	void markDirty() { m_saveAt = millis() + SAVE_DELAY; }
	void saveState();
	void savePresets();
	void loadState();
	void loadPresets();
	String manualHex() const;
	void handleCalib(const char *cmd, JsonDocument &doc);
	void computeGains();
	void saveCalib();
	void loadCalib();
	void setManualHex(const char *hex);

	static constexpr uint32_t SAVE_DELAY = SAVE_DELAY_MS;
	Mode m_mode = MODE_SWEEP;
	int16_t m_values[P_COUNT];
	uint8_t m_manual[NUM_TILES * 4] = {};  // manual mode, set per board from the UI
	std::vector<Preset> m_presets;
	static constexpr int PRESETS_VERSION = 3;  // bump when adding a built-in preset
	int m_presetsVersion = 0;                  // of /presets.json, 0 = none yet
	uint32_t m_saveAt = 0;

	float m_lux[NUM_TILES][4] = {};  // measured, 0 = not yet
	uint16_t m_gain[NUM_TILES][4];
	bool m_evenBoards = true;     // every board as bright as the weakest
	bool m_balanceColours = true; // R:G:B in the ratio of a neutral white
	uint16_t m_balance[4] = {1000, 1000, 1000, 1000};
	int m_showTile = -1, m_showCh = 0;
	uint32_t m_showUntil = 0;
	bool m_calibChanged = false;
};

extern const ModeDef MODES[MODE_COUNT];
extern const ParamDef PARAMS[P_COUNT];
