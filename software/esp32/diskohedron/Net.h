/*
Net - hotspot with captive portal, optional venue WiFi, web UI, WebSocket, OTA.

WebSocket messages arrive in the async TCP task; they are queued and handled in
loop() so the State is only ever touched from one task.
*/
#pragma once

#include "State.h"

extern volatile bool g_firmwareUpdating;  // LEDs stay dark while the new firmware is written

class Net {
public:
	explicit Net(State &state) : m_state(state) {}

	void begin();
	void loop();

private:
	State &m_state;
};
