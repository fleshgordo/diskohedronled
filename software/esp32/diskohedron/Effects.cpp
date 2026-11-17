#include "Effects.h"
#include "StreamBuffer.h"
#include "Net.h"
#include "TileGeometry.h"

// --------------------------------------------------------------- geometry
// the 4 neighbours of every face (1-based, from initNeighbours() of the old firmware;
// tile 17 listed 5 instead of 4 there - fixed, see tools/sphere/geometry.py)
static const uint8_t NEIGHBOURS[30][4] = {
	{2, 3, 9, 10}, {3, 1, 14, 15}, {4, 5, 1, 2}, {5, 3, 17, 18}, {6, 7, 3, 4},
	{7, 5, 20, 21}, {8, 9, 5, 6}, {9, 7, 23, 24}, {10, 1, 7, 8}, {11, 12, 1, 9},
	{12, 10, 24, 25}, {13, 14, 10, 11}, {14, 12, 27, 28}, {15, 2, 12, 13}, {16, 17, 2, 14},
	{17, 15, 28, 29}, {18, 4, 15, 16}, {19, 20, 4, 17}, {20, 18, 29, 30}, {21, 6, 18, 19},
	{22, 23, 6, 20}, {23, 21, 30, 26}, {24, 8, 21, 22}, {25, 11, 8, 23}, {26, 27, 11, 24},
	{27, 25, 22, 30}, {28, 13, 25, 26}, {29, 16, 13, 27}, {30, 19, 16, 28}, {26, 22, 19, 29},
};

// horizontal rings from top to bottom, 6 entries each (1-based, ledrows36 of the old firmware)
static const uint8_t ROWS[7][6] = {
	{1, 2, 3, 1, 2, 3},
	{4, 5, 9, 10, 14, 15},
	{7, 12, 17, 7, 12, 17},
	{6, 8, 11, 13, 16, 18},
	{20, 24, 28, 20, 24, 28},
	{19, 21, 23, 25, 27, 29},
	{22, 26, 30, 22, 26, 30},
};

// tile bits for the game of life
static const uint8_t LED_BIT[4] = {0x80, 0x40, 0x20, 0x10};  // R G B W
static const uint8_t DIR_BIT[4] = {0x08, 0x04, 0x02, 0x01};

void Effects::begin() {
	randomSeed(esp_random());
	m_leds.clear();
	m_leds.show();
}

void Effects::out(int tile, int r, int g, int b, int w) {
	int bri = m_state.v(P_BRIGHTNESS);
	// brightness in %, calibration gain in per mille
	auto scale = [&](int v, int ch) { return (int)((uint32_t)v * bri * m_state.gain(tile, ch) / 100000); };
	m_leds.set(tile, scale(r, 0), scale(g, 1), scale(b, 2), scale(w, 3));
}

void Effects::update(uint32_t now) {
	// measuring with the luxmeter: one LED at full power, no brightness or calibration
	int calTile, calCh;
	if (m_state.calibShow(calTile, calCh)) {
		m_leds.clear();
		uint8_t rgbw[4] = {};
		rgbw[calCh] = 255;
		m_leds.set(calTile, rgbw[0], rgbw[1], rgbw[2], rgbw[3]);
		m_leds.show();
		return;
	}

	// switched off: dark, also for Art-Net and video. Also while new firmware is written: flash
	// writes stall the LED task, and a dark frame is the only one that can't flash.
	if (!m_state.v(P_POWER) || g_firmwareUpdating) {
		m_leds.clear();
		m_leds.show();
		return;
	}

	// a video streamed over WiFi overrides the mode while frames keep coming
	uint8_t frame[STREAM_FRAME_BYTES];
	if (stream::get(frame, now)) {
		for (int t = 0; t < 30; t++) out(t, frame[t * 4], frame[t * 4 + 1], frame[t * 4 + 2], frame[t * 4 + 3]);
		m_leds.show();
		return;
	}

	Mode mode = m_state.mode();
	if (mode != m_lastMode) {
		m_lastMode = mode;
		m_lastStep = 0;
		if (mode == MODE_LIFE) lifeSeed();
		m_checkPos = 0;
		m_checkLast = now;
	}
	switch (mode) {
		case MODE_FLICKER: flicker(now); break;
		case MODE_NOISE: noise(now); break;
		case MODE_LIFE: life(now); break;
		case MODE_SWEEP: sweep(now); break;
		case MODE_COLOR: color(); break;
		case MODE_RIPPLE: ripple(now); break;
		case MODE_CHECK: check(now); break;
		case MODE_MANUAL:
			for (int t = 0; t < 30; t++) {
				const uint8_t *c = m_state.manual(t);
				out(t, c[0], c[1], c[2], c[3]);
			}
			break;
		default:
			for (int t = 0; t < 30; t++) out(t, 0, 0, 0, 0);
			break;
	}
	m_leds.show();
}

// --------------------------------------------------------------- flicker / noise / colour
void Effects::flicker(uint32_t now) {
	if (m_lastStep && now - m_lastStep < (uint32_t)m_state.v(P_FLICKER_RATE)) return;
	m_lastStep = now;
	int level = m_state.v(P_FLICKER_LEVEL);
	for (int t = 0; t < 30; t++) out(t, 0, 0, 0, random(level + 1));
}

void Effects::noise(uint32_t now) {
	if (m_lastStep && now - m_lastStep < (uint32_t)m_state.v(P_NOISE_RATE)) return;
	m_lastStep = now;
	for (int t = 0; t < 30; t++)
		out(t, random(m_state.v(P_NOISE_R) + 1), random(m_state.v(P_NOISE_G) + 1),
		    random(m_state.v(P_NOISE_B) + 1), random(m_state.v(P_NOISE_W) + 1));
}

void Effects::color() {
	for (int t = 0; t < 30; t++)
		out(t, m_state.v(P_COLOR_R), m_state.v(P_COLOR_G), m_state.v(P_COLOR_B), m_state.v(P_COLOR_W));
}

// --------------------------------------------------------------- game of life
// Four independent games (R, G, B, W) on the faces of the triacontahedron.
// A face's DIR bit lets a living colour count as neighbour for the other colours.
void Effects::lifeSeed() {
	for (int t = 0; t < 30; t++) m_tiles[t] = random(256);
	m_lastSeed = millis();
	lifeCompute();
}

void Effects::lifeCompute() {
	memcpy(m_next, m_tiles, sizeof(m_tiles));
	for (int t = 0; t < 30; t++) {
		uint8_t tile = m_tiles[t];
		uint8_t n[4] = {0, 0, 0, 0};
		for (int ne = 0; ne < 4; ne++) {
			uint8_t neighbour = m_tiles[NEIGHBOURS[t][ne] - 1];
			for (int c = 0; c < 4; c++)
				if (neighbour & LED_BIT[c]) n[c]++;
		}
		for (int c = 0; c < 4; c++) {
			if ((tile & DIR_BIT[c]) && (tile & LED_BIT[c]))
				for (int o = 0; o < 4; o++)
					if (o != c) n[o]++;
		}
		for (int c = 0; c < 4; c++) {
			if (tile & LED_BIT[c]) {
				if (n[c] < m_alive[c] - 1 || n[c] >= m_alive[c] + 1) m_next[t] ^= LED_BIT[c];
			} else {
				if (n[c] == m_alive[c]) m_next[t] ^= LED_BIT[c];
			}
		}
	}
	m_transition = 0;
}

uint8_t Effects::lifeLevel(bool was, bool will) const {
	if (was && will) return 255;
	if (was) return 255 * (1 - m_transition);
	if (will) return 255 * m_transition;
	return 0;
}

void Effects::life(uint32_t now) {
	uint32_t dt = m_lastStep ? now - m_lastStep : 0;
	m_lastStep = now;

	if (now - m_lastSeed >= (uint32_t)m_state.v(P_LIFE_RESEED) * 1000) {
		// rotate the survival rules between the colours, then start over
		uint8_t first = m_alive[0];
		m_alive[0] = m_alive[1];
		m_alive[1] = m_alive[2];
		m_alive[2] = m_alive[3];
		m_alive[3] = first;
		lifeSeed();
	}

	m_transition += (float)dt / m_state.v(P_LIFE_FADE);
	if (m_transition >= 1) {
		memcpy(m_tiles, m_next, sizeof(m_tiles));
		bool anyAlive = false;
		for (int t = 0; t < 30; t++) anyAlive |= (m_tiles[t] & 0xF0) != 0;
		if (anyAlive) lifeCompute();
		else lifeSeed();  // everything died: don't sit in the dark until the next reseed
	}

	for (int t = 0; t < 30; t++) {
		uint8_t lv[4];
		for (int c = 0; c < 4; c++) lv[c] = lifeLevel(m_tiles[t] & LED_BIT[c], m_next[t] & LED_BIT[c]);
		out(t, lv[0], lv[1], lv[2], lv[3]);
	}
}

// --------------------------------------------------------------- sweep (old "boot" mode)
// A soft band of light travels over the rings; every pass switches R -> G -> B -> W.
void Effects::sweep(uint32_t now) {
	uint32_t dt = m_lastStep ? now - m_lastStep : 0;
	m_lastStep = now;
	float rowsPerSecond = m_state.v(P_SWEEP_SPEED) / 10.0f;
	m_sweepY += rowsPerSecond * dt / 1000.0f;
	if (m_sweepY > 9) {
		m_sweepY = -2;
		m_sweepColor = (m_sweepColor + 1) % 4;
	} else if (m_sweepY < -2) {
		m_sweepY = 9;
		m_sweepColor = (m_sweepColor + 1) % 4;
	}

	int level = m_state.v(P_SWEEP_LEVEL);
	for (int row = 0; row < 7; row++) {
		float d = fabsf(m_sweepY - (row + 0.5f)) * 0.27f;
		int br = d <= 1 ? level * (1 - d) * (1 - d) : 0;
		int c[4] = {0, 0, 0, 0};
		c[m_sweepColor] = br;
		for (int i = 0; i < 6; i++) out(ROWS[row][i] - 1, c[0], c[1], c[2], c[3]);
	}
}

// --------------------------------------------------------------- ripples
// Random drops land on the sphere. Each sends a soft ring outwards along the surface;
// the ring shrinks again on the far side and meets at the opposite point.
static void hsvToRgbw(float h, float s, float out[4]) {
	float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
	float rgb[3] = {constrain(r, 0.f, 1.f), constrain(g, 0.f, 1.f), constrain(b, 0.f, 1.f)};
	for (float &c : rgb) c = 1 - s + s * c;  // desaturate towards white
	float w = min(rgb[0], min(rgb[1], rgb[2]));  // common part goes to the white LED
	out[0] = rgb[0] - w;
	out[1] = rgb[1] - w;
	out[2] = rgb[2] - w;
	out[3] = w;
}

void Effects::ripple(uint32_t now) {
	const float speed = radians((float)m_state.v(P_RIPPLE_SPEED));  // rad/s along the surface
	const float halfWidth = radians((float)m_state.v(P_RIPPLE_WIDTH)) / 2;
	const float meanGap = 60000.0f / m_state.v(P_RIPPLE_RATE);

	// new drop: random time gaps (exponential), random place and colour
	if (!m_nextDrop || (int32_t)(now - m_nextDrop) >= 0) {
		int slot = 0;
		for (int i = 0; i < MAX_DROPS; i++) {
			if (!m_drops[i].active) { slot = i; break; }
			if (m_drops[i].start < m_drops[slot].start) slot = i;  // all busy: replace the oldest
		}
		Drop &d = m_drops[slot];
		float z = random(-10000, 10001) / 10000.0f;
		float phi = random(0, 62832) / 10000.0f;
		float rxy = sqrtf(1 - z * z);
		d.dir[0] = rxy * cosf(phi);
		d.dir[1] = rxy * sinf(phi);
		d.dir[2] = z;
		d.start = now;
		hsvToRgbw(random(0, 1000) / 1000.0f, m_state.v(P_RIPPLE_SAT) / 100.0f, d.rgbw);
		d.active = true;
		float u = random(1, 10001) / 10000.0f;
		m_nextDrop = now + max(300.0f, -logf(u) * meanGap);
	}

	float acc[30][4] = {};
	for (Drop &d : m_drops) {
		if (!d.active) continue;
		float radius = speed * (now - d.start) / 1000.0f;  // ring position, 0..PI = start..opposite point
		if (radius > PI + 2 * halfWidth) {
			d.active = false;
			continue;
		}
		float amp = 1 - 0.6f * fminf(radius / (float)PI, 1.0f);    // weaker as it travels
		if (radius > PI) amp *= 1 - (radius - PI) / (2 * halfWidth);  // fade out after meeting
		for (int t = 0; t < 30; t++) {
			float dot = d.dir[0] * TILE_DIR[t][0] + d.dir[1] * TILE_DIR[t][1] + d.dir[2] * TILE_DIR[t][2];
			float dist = acosf(constrain(dot, -1.0f, 1.0f));  // angle between drop and face
			float x = (dist - radius) / halfWidth;
			if (x < -3 || x > 3) continue;
			float level = amp * expf(-x * x);
			for (int c = 0; c < 4; c++) acc[t][c] += level * d.rgbw[c];
		}
	}
	for (int t = 0; t < 30; t++) {
		int v[4];
		for (int c = 0; c < 4; c++) v[c] = min(255, (int)(acc[t][c] * 255));
		out(t, v[0], v[1], v[2], v[3]);
	}
}

// --------------------------------------------------------------- check
// Board 1, 2, ... 30 light up one after the other, fading in and out: a full round in red,
// then green, blue, white (only the selected colours).
void Effects::check(uint32_t now) {
	// advance by elapsed time, so moving the speed slider doesn't jump
	m_checkPos += (now - m_checkLast) / (float)m_state.v(P_CHECK_TIME);
	m_checkLast = now;

	int colours[4], n = 0;
	for (int c = 0; c < 4; c++)
		if (m_state.v((Param)(P_CHECK_R + c))) colours[n++] = c;

	for (int t = 0; t < 30; t++) out(t, 0, 0, 0, 0);
	if (!n) return;
	if (m_checkPos >= 30 * n) m_checkPos = fmodf(m_checkPos, 30 * n);

	int step = (int)m_checkPos;
	int level = 255 * sinf(PI * (m_checkPos - step));  // 0 -> 255 -> 0 within one board
	int rgbw[4] = {};
	rgbw[colours[step / 30]] = level;
	out(step % 30, rgbw[0], rgbw[1], rgbw[2], rgbw[3]);
}
