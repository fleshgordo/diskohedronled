/*
StreamBuffer - latest frame from an external source:
  - WebSocket binary message (tools/sphere/video2lamp.py --stream)
  - Art-Net (Resolume, TouchDesigner, lighting desks), see ArtNet.cpp

A frame is NUM_TILES * 4 bytes (R, G, B, W per tile, tile 1 first). While frames keep
arriving the lamp shows them instead of the current mode; after the source's timeout
it falls back to the mode.
*/
#pragma once

#include <Arduino.h>
#include <string.h>
#include "config.h"

#define STREAM_FRAME_BYTES (NUM_TILES * 4)
#define STREAM_TIMEOUT_MS 1000

namespace stream {

inline portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
inline uint8_t frame[STREAM_FRAME_BYTES];
inline volatile uint32_t lastFrameAt = 0;
inline volatile uint32_t timeoutMs = STREAM_TIMEOUT_MS;

// called from the WebSocket / UDP task
inline void push(const uint8_t *data, uint32_t timeout = STREAM_TIMEOUT_MS) {
	portENTER_CRITICAL(&mux);
	memcpy(frame, data, STREAM_FRAME_BYTES);
	lastFrameAt = millis() | 1;  // never 0
	timeoutMs = timeout;
	portEXIT_CRITICAL(&mux);
}

// called from loop(); false when no stream is active
inline bool get(uint8_t *out, uint32_t now) {
	if (!lastFrameAt || now - lastFrameAt > timeoutMs) return false;
	portENTER_CRITICAL(&mux);
	memcpy(out, frame, STREAM_FRAME_BYTES);
	portEXIT_CRITICAL(&mux);
	return true;
}

}  // namespace stream
