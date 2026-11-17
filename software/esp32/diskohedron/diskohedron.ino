/*
Diskohedron LED - ESP32 firmware

Drives the 30 STP04CM05 boards of the triacontahedron and serves the web UI
from the lamp's own hotspot. See ../README.md for wiring and flashing.
*/
#include "config.h"
#include "LedChain.h"
#include "State.h"
#include "Effects.h"
#include "Net.h"

LedChain leds(PIN_CLK, PIN_DATA, PIN_LATCH, NUM_TILES);
State state;
Effects effects(leds, state);
Net net(state);

void setup() {
	Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
	delay(2000);  // debug build only: give the USB serial time to reconnect so the boot log isn't lost
#endif
	if (!leds.begin(SPI_CLOCK_HZ)) Serial.println("LED chain init failed");
	effects.begin();  // LEDs dark as early as possible
	state.begin();
	effects.update(millis());  // show the saved mode before WiFi comes up (~1 s)
	net.begin();
}

void loop() {
	net.loop();
	state.loop();

	static uint32_t lastTick = 0;
	uint32_t now = millis();
	if (now - lastTick >= EFFECT_TICK_MS) {
		lastTick = now;
		effects.update(now);
	}
	delay(1);
}
