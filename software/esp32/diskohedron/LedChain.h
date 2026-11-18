/*
LedChain - drives the daisy-chained STP04CM05 boards (4 channels per board).

Replaces the AVR-only ShiftPWM library. Binary Code Modulation, sent by the SPI
peripheral via DMA, timed entirely by the hardware:

  - the SPI runs in dual mode: D0 (MOSI) = DATA, D1 (MISO pin) = LE. So the latch
    pulses are part of the data stream and come exactly on time, whatever the CPU does.
  - one PWM frame = ONE DMA transaction. Bit plane k is shown for 2^k x (chain length)
    clocks: its pattern is clocked in, LE pulses on the last bit, then the same pattern
    is repeated (2^k - 1) more times while it is visible. The repeats keep the shift
    register equal to the shown plane.
  - Each frame ends with one blank pass (all off). The gap between two transactions, the only
    part that depends on software, is therefore dark: a late frame dims instead of flashing.

Bit order matches the old ShiftPWM setup: the last output is shifted first,
output 0 (board 1, OUT0) last. Per board the outputs are R, B, G, W.
*/
#pragma once

#include <Arduino.h>
#include "driver/spi_master.h"
#include "esp_private/spi_master_internal.h"  // SCT: several frames per transfer

#if CONFIG_IDF_TARGET_ESP32C3
#define LEDCHAIN_SPI_HOST SPI2_HOST  // the C3's only free SPI
#else
#define LEDCHAIN_SPI_HOST SPI3_HOST
#endif

// In dual mode each byte carries 4 clocks, 2 bits per clock: bit 7 goes to D1 (MISO), bit 6 to
// D0 (MOSI), and so on. Set to 1 if the boards show garbage, i.e. DATA and LE come out swapped.
#ifndef LEDCHAIN_SWAP_LINES
#define LEDCHAIN_SWAP_LINES 0
#endif

class LedChain {
public:
	LedChain(int clkPin, int dataPin, int latchPin, int numTiles, spi_host_device_t host = LEDCHAIN_SPI_HOST);

	bool begin(int spiClockHz);

	// values 0..255 = duty cycle; takes effect on show()
	void set(int tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
	void clear();
	void show();

	int numTiles() const { return m_numTiles; }

	// Call before writing to flash. While the flash is written the CPU can't run the SPI
	// interrupt, so the next frame would come late and the lamp flash dark. This queues
	// `ms` worth of the current picture as one hardware transfer and returns once it is playing.
	void holdForFlash(uint32_t ms);

	// diagnostics: late frames (the lamp is dark for the gap). JSON for /api/debug.
	String statsJson();

private:
	static void task(void *arg);
	static void IRAM_ATTR onDone(spi_transaction_t *t);
	void buildFrame(uint8_t *out);

	int m_clkPin, m_dataPin, m_latchPin;
	int m_numTiles;
	int m_numOutputs;  // bits per bit plane = clocks per chain pass
	int m_blockBytes;  // one pass in dual mode: 4 clocks per byte
	size_t m_frameBytes;
	spi_host_device_t m_host;
	spi_device_handle_t m_dev = nullptr;

	uint8_t *m_values;   // written by the effects
	uint8_t *m_pending;  // handed over to the SPI task on show()
	volatile bool m_dirty = true;
	portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

	// two frames: one is being sent while the next one is built in the other
	uint8_t *m_tx[2] = {};

	// one queued transfer = 1 frame normally, up to MAX_SEGS repeats of it for holdForFlash()
	static const int MAX_SEGS = 24;  // 184 ms
	struct Slot {
		LedChain *self;
		int segs;
		spi_multi_transaction_t seg[MAX_SEGS];
	};
	Slot *m_slots = nullptr;
	bool m_sct = false;  // false: SCT unavailable, plain transfers (no hold)
	volatile int m_holdSegs = 0;
	SemaphoreHandle_t m_holdStarted = nullptr;

	// filled by onDone() in the SPI interrupt
	uint32_t m_frameUs = 0;               // duration of one frame on the wire
	volatile int64_t m_lastDoneUs = 0;
	volatile uint32_t m_frames = 0, m_late = 0, m_maxGapUs = 0, m_holds = 0;
	static const int LOG = 16;
	volatile uint32_t m_logAtMs[LOG] = {}, m_logGapUs[LOG] = {};
	volatile uint32_t m_logN = 0;
};
