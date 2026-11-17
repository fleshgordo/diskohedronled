/*
Effects - the lamp modes, ported from LampFirmware / diskoledronBasel.ino.

Everything is timed with millis() now. On the Arduino the animations advanced
once per loop() and their speed depended on how busy the ShiftPWM interrupt
kept the CPU.
*/
#pragma once

#include "LedChain.h"
#include "State.h"

class Effects {
public:
	Effects(LedChain &leds, const State &state) : m_leds(leds), m_state(state) {}

	void begin();
	void update(uint32_t now);  // call every EFFECT_TICK_MS

private:
	void out(int tile, int r, int g, int b, int w);

	void flicker(uint32_t now);
	void noise(uint32_t now);
	void life(uint32_t now);
	void sweep(uint32_t now);
	void color();
	void ripple(uint32_t now);
	void check(uint32_t now);

	// game of life
	void lifeSeed();
	void lifeCompute();
	uint8_t lifeLevel(bool was, bool will) const;

	LedChain &m_leds;
	const State &m_state;
	Mode m_lastMode = MODE_COUNT;
	uint32_t m_lastStep = 0;

	uint8_t m_tiles[30];
	uint8_t m_next[30];
	uint8_t m_alive[4] = {2, 3, 3, 3};
	float m_transition = 0;  // 0..1 crossfade from m_tiles to m_next
	uint32_t m_lastSeed = 0;

	float m_sweepY = -2;
	uint8_t m_sweepColor = 0;

	// ripples: drops land on the sphere and send a ring around it
	struct Drop {
		float dir[3];    // where it landed (unit vector)
		uint32_t start;  // millis
		float rgbw[4];   // colour 0..1
		bool active;
	};
	static constexpr int MAX_DROPS = 12;
	Drop m_drops[MAX_DROPS] = {};
	uint32_t m_nextDrop = 0;

	// check: one board at a time, 1..30, once per selected colour
	float m_checkPos = 0;  // in boards, the fraction is the fade
	uint32_t m_checkLast = 0;
};
