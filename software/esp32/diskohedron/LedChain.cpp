#include "LedChain.h"
#include <string.h>

LedChain::LedChain(int clkPin, int dataPin, int latchPin, int numTiles, spi_host_device_t host)
	: m_clkPin(clkPin), m_dataPin(dataPin), m_latchPin(latchPin), m_numTiles(numTiles), m_host(host) {
	m_numOutputs = numTiles * 4;
	m_numBytes = (m_numOutputs + 7) / 8;
	m_values = (uint8_t *)calloc(m_numOutputs, 1);
	m_pending = (uint8_t *)calloc(m_numOutputs, 1);
}

void LedChain::set(int tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
	if (tile < 0 || tile >= m_numTiles) return;
	uint8_t *v = m_values + tile * 4;
	v[0] = r;
	v[1] = b;
	v[2] = g;
	v[3] = w;
}

void LedChain::clear() {
	memset(m_values, 0, m_numOutputs);
}

void LedChain::show() {
	portENTER_CRITICAL(&m_mux);
	memcpy(m_pending, m_values, m_numOutputs);
	m_dirty = true;
	portEXIT_CRITICAL(&m_mux);
}

bool LedChain::begin(int spiClockHz) {
	// transaction k shows plane k-1 while it shifts, so its length = weight of plane k-1
	for (int k = 0; k < 8; k++) {
		int prevPlane = (k + 7) % 8;
		m_txLen[k] = (size_t)m_numBytes << prevPlane;
		for (int s = 0; s < 2; s++) {
			m_tx[s][k] = (uint8_t *)heap_caps_calloc(1, m_txLen[k], MALLOC_CAP_DMA);
			if (!m_tx[s][k]) return false;
		}
	}

	spi_bus_config_t bus = {};
	bus.mosi_io_num = m_dataPin;
	bus.miso_io_num = -1;
	bus.sclk_io_num = m_clkPin;
	bus.quadwp_io_num = -1;
	bus.quadhd_io_num = -1;
	bus.max_transfer_sz = m_txLen[0] + 64;
	esp_err_t err = spi_bus_initialize(m_host, &bus, SPI_DMA_CH_AUTO);
	if (err != ESP_OK) {
		Serial.printf("LedChain: spi_bus_initialize: %s\n", esp_err_to_name(err));
		return false;
	}

	spi_device_interface_config_t dev = {};
	dev.mode = 3;  // CPOL=1, CPHA=1 like the old ShiftPWM
	dev.clock_speed_hz = spiClockHz;
	dev.spics_io_num = m_latchPin;  // CS = LE
	dev.queue_size = 16;  // two frames: the next one is always queued behind the current one
	dev.cs_ena_pretrans = 1;
	dev.cs_ena_posttrans = 1;
	err = spi_bus_add_device(m_host, &dev, &m_dev);
	if (err != ESP_OK) {
		Serial.printf("LedChain: spi_bus_add_device: %s\n", esp_err_to_name(err));
		return false;
	}
	Serial.printf("LedChain: CLK %d, DATA %d, LE %d, %d Hz\n", m_clkPin, m_dataPin, m_latchPin, spiClockHz);

	// core 0 next to WiFi; the task sleeps while DMA runs
	xTaskCreatePinnedToCore(task, "ledchain", 4096, this, configMAX_PRIORITIES - 1, nullptr, 0);
	return true;
}

void LedChain::buildPlanes(uint8_t *planes) {
	// planes: 8 x m_numBytes, MSB first, last output shifted first
	uint8_t duty[m_numOutputs];
	portENTER_CRITICAL(&m_mux);
	memcpy(duty, m_pending, m_numOutputs);
	m_dirty = false;
	portEXIT_CRITICAL(&m_mux);

	memset(planes, 0, 8 * m_numBytes);
	// pad at the front so output 0 is the very last bit of the stream
	const int pad = m_numBytes * 8 - m_numOutputs;
	for (int i = 0; i < m_numOutputs; i++) {
		int s = pad + i;  // stream position of output (N-1-i)
		uint8_t d = duty[m_numOutputs - 1 - i];
		for (int b = 0; b < 8; b++)
			if (d & (1 << b)) planes[b * m_numBytes + (s >> 3)] |= 0x80 >> (s & 7);
	}
}

// The SPI must never run dry: while it is idle the boards keep showing the last bit plane
// (the brightest one), which flashes visibly if this task is held up by WiFi for a few ms.
// So frame n+1 is queued before waiting for frame n to finish.
void LedChain::task(void *arg) {
	LedChain *self = (LedChain *)arg;
	const int R = self->m_numBytes;
	uint8_t *planes = (uint8_t *)malloc(8 * R);
	spi_transaction_t t[2][8] = {};
	spi_transaction_t *done;
	int set = 0;  // buffer set of the newest queued frame

	for (uint32_t frame = 0;; frame++) {
		if (self->m_dirty) {
			// the other set was last used two or more frames ago, which have finished
			set ^= 1;
			self->buildPlanes(planes);
			for (int k = 0; k < 8; k++)
				memcpy(self->m_tx[set][k] + self->m_txLen[k] - R, planes + k * R, R);
		}
		spi_transaction_t *ft = t[frame & 1];
		for (int k = 0; k < 8; k++) {
			ft[k].length = self->m_txLen[k] * 8;
			ft[k].tx_buffer = self->m_tx[set][k];
			spi_device_queue_trans(self->m_dev, &ft[k], portMAX_DELAY);
		}
		if (frame == 0) continue;
		for (int k = 0; k < 8; k++)  // wait for the previous frame
			spi_device_get_trans_result(self->m_dev, &done, portMAX_DELAY);
	}
}
