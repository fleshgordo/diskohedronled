#pragma once

// shown in the web page (Firmware section) and in /api/schema; raise it for every release
#define FIRMWARE_VERSION "1.1.0"

// ---------------------------------------------------------------- LED chain
// ESP32 -> JP1 on the FIRST board of the chain
#if CONFIG_IDF_TARGET_ESP32C3
// ESP32-C3: avoid 2/8/9 (boot strapping), 12-17 (flash), 18/19 (USB)
#define PIN_CLK 6     // JP1 pin 3  EXTCLK
#define PIN_DATA 7    // JP1 pin 4  EXTDATA
#define PIN_LATCH 10  // JP1 pin 5  EXTOE (drives LE of all STP04)
#else
#define PIN_CLK 18    // JP1 pin 3  EXTCLK
#define PIN_DATA 23   // JP1 pin 4  EXTDATA
#define PIN_LATCH 4   // JP1 pin 5  EXTOE (drives LE of all STP04)
#endif
                      // GND -> JP1 pin 2
#define SPI_CLOCK_HZ 4000000  // same as the old Arduino; ~130 Hz refresh

#define NUM_TILES 30

// ---------------------------------------------------------------- WiFi
// The lamp always opens its own hotspot (with captive portal).
#define AP_SSID "diskoledron"
#define AP_PASS "diskodisko"  // min. 8 characters
#define HOSTNAME "diskoledron"  // http://diskoledron.local when on a shared network

// Optional: also join your home WiFi. Normally set in the web UI (WiFi section), which
// overrides these; they are only the default before anything was saved there.
// Joins in the background; the hotspot may switch to the home WiFi's channel when it does.
#define STA_SSID ""
#define STA_PASS ""

#define OTA_PASSWORD "diskodisko"

// ---------------------------------------------------------------- Art-Net input
// Resolume / TouchDesigner / lighting desks. 30 tiles x RGBW = 120 channels.
#define ARTNET_UNIVERSE 0        // 15-bit port address (net << 8 | subnet << 4 | universe)
#define ARTNET_START_CHANNEL 1   // 1-based DMX channel of tile 1 red
#define ARTNET_TIMEOUT_MS 5000   // back to the selected mode this long after the last packet

// ---------------------------------------------------------------- misc
#define EFFECT_TICK_MS 10      // effect update rate
#define SAVE_DELAY_MS 3000     // persist state this long after the last change
