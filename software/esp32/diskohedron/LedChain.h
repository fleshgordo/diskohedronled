/*
LedChain - drives the daisy-chained STP04CM05 boards (4 channels per board).

Replaces the AVR-only ShiftPWM library. Uses Binary Code Modulation streamed by
the SPI peripheral via DMA, so the CPU stays almost idle:

  - one PWM frame = 8 SPI transactions, one per bit plane (bit 0..7)
  - SPI CS drives LE: low while shifting, high = outputs take the new bits
  - a bit plane stays visible until the NEXT transaction ends, so that next
    transaction is padded with leading dummy bytes to last 2^k units. The dummy
    bytes fall off the end of the chain, only the last bits clocked in count.

Bit order matches the old ShiftPWM setup: the last output is shifted first,
output 0 (board 1, OUT0) last. Per board the outputs are R, B, G, W.
*/
#pragma once

#include <Arduino.h>
#include "driver/spi_master.h"

#if CONFIG_IDF_TARGET_ESP32C3
#define LEDCHAIN_SPI_HOST SPI2_HOST  // the C3's only free SPI
#else
#define LEDCHAIN_SPI_HOST SPI3_HOST
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

private:
	static void task(void *arg);
	void buildPlanes(uint8_t *planes);

	int m_clkPin, m_dataPin, m_latchPin;
	int m_numTiles;
	int m_numOutputs;
	int m_numBytes;
	spi_host_device_t m_host;
	spi_device_handle_t m_dev = nullptr;

	uint8_t *m_values;   // written by the effects
	uint8_t *m_pending;  // handed over to the SPI task on show()
	volatile bool m_dirty = true;
	portMUX_TYPE m_mux = portMUX_INITIALIZER_UNLOCKED;

	// two sets of the 8 bit planes: one is being sent while the next frame goes into the other
	uint8_t *m_tx[2][8] = {};
	size_t m_txLen[8] = {};
};
