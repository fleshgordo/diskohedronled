#include "LedChain.h"
#include <string.h>
#include "esp_timer.h"

#if LEDCHAIN_SWAP_LINES
static const int DATA_SHIFT = 1, LE_SHIFT = 0;
#else
static const int DATA_SHIFT = 0, LE_SHIFT = 1;  // per clock: D1 = higher bit = LE, D0 = DATA
#endif

LedChain::LedChain(int clkPin, int dataPin, int latchPin, int numTiles, spi_host_device_t host)
	: m_clkPin(clkPin), m_dataPin(dataPin), m_latchPin(latchPin), m_numTiles(numTiles), m_host(host) {
	m_numOutputs = numTiles * 4;   // a multiple of 4, so one pass is a whole number of bytes
	m_blockBytes = m_numOutputs / 4;
	m_frameBytes = (size_t)m_blockBytes * 256;  // planes 0..7 last 1+2+...+128 passes, + 1 blank pass
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
	for (int s = 0; s < 2; s++) {
		m_tx[s] = (uint8_t *)heap_caps_calloc(1, m_frameBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
		if (!m_tx[s]) {
			Serial.println("LedChain: out of DMA memory");
			return false;
		}
	}

	spi_bus_config_t bus = {};
	bus.mosi_io_num = m_dataPin;   // D0 = DATA
	bus.miso_io_num = m_latchPin;  // D1 = LE (an output in dual mode)
	bus.sclk_io_num = m_clkPin;
	bus.quadwp_io_num = -1;
	bus.quadhd_io_num = -1;
	bus.max_transfer_sz = m_frameBytes * MAX_SEGS;  // also sizes the DMA descriptor pool for SCT
	bus.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_DUAL;
	esp_err_t err = spi_bus_initialize(m_host, &bus, SPI_DMA_CH_AUTO);
	if (err != ESP_OK) {
		Serial.printf("LedChain: spi_bus_initialize: %s\n", esp_err_to_name(err));
		return false;
	}

	spi_device_interface_config_t dev = {};
	dev.mode = 3;  // CPOL=1, CPHA=1 like the old ShiftPWM
	dev.clock_speed_hz = spiClockHz;
	dev.spics_io_num = -1;  // LE comes from the data stream
	dev.flags = SPI_DEVICE_HALFDUPLEX;  // needed for dual mode
	dev.queue_size = 2;
	dev.post_cb = onDone;
	m_frameUs = (uint32_t)((uint64_t)m_frameBytes * 4 * 1000000 / spiClockHz);  // 4 clocks per byte
	err = spi_bus_add_device(m_host, &dev, &m_dev);
	if (err != ESP_OK) {
		Serial.printf("LedChain: spi_bus_add_device: %s\n", esp_err_to_name(err));
		return false;
	}
	m_slots = (Slot *)heap_caps_calloc(2, sizeof(Slot), MALLOC_CAP_INTERNAL);
	m_holdStarted = xSemaphoreCreateBinary();
	err = m_slots ? spi_bus_multi_trans_mode_enable(m_dev, true) : ESP_ERR_NO_MEM;
	m_sct = err == ESP_OK;
	if (!m_sct) Serial.printf("LedChain: SCT mode: %s (flash writes may blink)\n", esp_err_to_name(err));
	Serial.printf("LedChain: CLK %d, DATA %d, LE %d, %d Hz, %u bytes/frame\n", m_clkPin, m_dataPin,
	              m_latchPin, spiClockHz, (unsigned)m_frameBytes);

	// core 0 next to WiFi; the task sleeps while DMA runs
	xTaskCreatePinnedToCore(task, "ledchain", 4096, this, configMAX_PRIORITIES - 1, nullptr, 0);
	return true;
}

void LedChain::buildFrame(uint8_t *out) {
	const int N = m_numOutputs, B = m_blockBytes;
	uint8_t duty[N];
	portENTER_CRITICAL(&m_mux);
	memcpy(duty, m_pending, N);
	m_dirty = false;
	portEXIT_CRITICAL(&m_mux);

	uint8_t block[B];
	uint8_t *p = out;
	for (int plane = 0; plane < 8; plane++) {
		// one pass: clock s shifts in output N-1-s, so output 0 is the last bit
		memset(block, 0, B);
		for (int s = 0; s < N; s++)
			if (duty[N - 1 - s] & (1 << plane)) block[s >> 2] |= 1 << (6 - 2 * (s & 3) + DATA_SHIFT);
		// first pass ends with the LE pulse, the repeats keep the register on this plane
		memcpy(p, block, B);
		p[B - 1] |= 1 << LE_SHIFT;  // last clock of the pass
		p += B;
		for (int r = 1; r < (1 << plane); r++, p += B) memcpy(p, block, B);
	}
	// end with all outputs off: if the next frame comes late (CPU busy, flash write), the lamp
	// stays dark for that moment instead of holding plane 7, which flashes bright
	memset(p, 0, B);
	p[B - 1] |= 1 << LE_SHIFT;
}

// SPI interrupt, after each frame. Frames normally finish exactly m_frameUs apart; more means
// the SPI sat idle (lamp dark) because the interrupt or the task came late.
void IRAM_ATTR LedChain::onDone(spi_transaction_t *t) {
	Slot *slot = (Slot *)t->user;
	LedChain *self = slot->self;
	int64_t now = esp_timer_get_time();
	if (self->m_lastDoneUs && slot->segs == 1) {
		int64_t gap = now - self->m_lastDoneUs - self->m_frameUs;
		if (gap > 500) {
			self->m_late++;
			if (gap > self->m_maxGapUs) self->m_maxGapUs = gap;
			uint32_t i = self->m_logN++ % LOG;
			self->m_logAtMs[i] = now / 1000;
			self->m_logGapUs[i] = gap;
		}
	}
	self->m_lastDoneUs = now;
	self->m_frames++;
}

String LedChain::statsJson() {
	String s = "{\"frameUs\":" + String(m_frameUs) + ",\"frames\":" + String(m_frames) +
	           ",\"late\":" + String(m_late) + ",\"holds\":" + String(m_holds) + ",\"sct\":" + String(m_sct) + ",\"maxGapUs\":" + String(m_maxGapUs) + ",\"recent\":[";
	uint32_t n = m_logN, first = n > LOG ? n - LOG : 0;
	for (uint32_t k = first; k < n; k++) {
		if (k > first) s += ",";
		s += "[" + String(m_logAtMs[k % LOG]) + "," + String(m_logGapUs[k % LOG]) + "]";
	}
	return s + "]}";
}

void LedChain::holdForFlash(uint32_t ms) {
	if (!m_sct) return;
	int segs = ms / (m_frameUs / 1000 + 1) + 1;
	m_holdSegs = segs > MAX_SEGS ? MAX_SEGS : segs;
	xSemaphoreTake(m_holdStarted, pdMS_TO_TICKS(60));  // at most ~2 frames
}

void LedChain::task(void *arg) {
	LedChain *self = (LedChain *)arg;
	spi_transaction_t t[2] = {};
	spi_transaction_t *done;
	spi_multi_transaction_t *doneMulti;
	int set = 0;  // buffer of the newest queued frame
	bool holdQueued = false;

	// The SPI must never run dry: frame n+1 is queued before waiting for frame n.
	for (uint32_t frame = 0;; frame++) {
		if (self->m_dirty) {
			// the other buffer belonged to the transfer that has just finished
			set ^= 1;
			self->buildFrame(self->m_tx[set]);
		}
		if (!self->m_sct) {
			spi_transaction_t &ft = t[frame & 1];
			ft.flags = SPI_TRANS_MODE_DIO;
			ft.length = self->m_frameBytes * 8;
			ft.tx_buffer = self->m_tx[set];
			ft.user = &self->m_slots[frame & 1];
			self->m_slots[frame & 1].self = self;
			self->m_slots[frame & 1].segs = 1;
			spi_device_queue_trans(self->m_dev, &ft, portMAX_DELAY);
			if (frame) spi_device_get_trans_result(self->m_dev, &done, portMAX_DELAY);
			continue;
		}
		Slot &sl = self->m_slots[frame & 1];
		int segs = self->m_holdSegs;
		if (segs < 1 || holdQueued) segs = 1;
		sl.self = self;
		sl.segs = segs;
		for (int i = 0; i < segs; i++) {
			sl.seg[i] = {};
			sl.seg[i].base.flags = SPI_TRANS_MODE_DIO;
			sl.seg[i].base.length = self->m_frameBytes * 8;
			sl.seg[i].base.tx_buffer = self->m_tx[set];  // the same frame, repeated
			sl.seg[i].base.user = &sl;
		}
		spi_device_queue_multi_trans(self->m_dev, sl.seg, segs, portMAX_DELAY);
		if (segs > 1) holdQueued = true;
		if (!frame) continue;
		spi_device_get_multi_trans_result(self->m_dev, &doneMulti, portMAX_DELAY);
		// the transfer before ours has ended, so the hold is playing now
		if (holdQueued && segs > 1) {
			self->m_holdSegs = 0;
			self->m_holds++;
			xSemaphoreGive(self->m_holdStarted);
		} else if (holdQueued) {
			holdQueued = false;  // the hold itself has finished
		}
	}
}
