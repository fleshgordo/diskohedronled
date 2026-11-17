// Diskohedron chain test - find the broken board (see DEBUG_PLAN.md).
// No libraries. Arduino: CLK 13, DATA 11, LATCH 8. Pro Micro: 15, 16, 10. ESP32-C3: 6, 7, 10. GND -> JP1 pin 2.
// Serial Monitor, 9600 baud, "Newline":
//   c        chase: one board after another (white), prints the board number
//   16       only board 16 on (white)
//   16r      only board 16, one colour: r g b w
//   a        all boards red, dimmed to ~20 % (software PWM)
//   o        all off
//   h        DATA static HIGH on the whole chain (LEDs don't change) - for the multimeter
//   l        DATA static LOW  on the whole chain
//   p        pin test: JP1 pin 3, 4, 5 go to 3.3 V one after another (2 s each) - check with a meter
//   ?        help
// Static HIGH/LOW: measure red probe TP39 (DATA+), black TP40 (DATA-) on every board.
// h -> about +0.35 V, l -> about -0.35 V. The first board where it doesn't follow is the broken spot.

#if defined(CONFIG_IDF_TARGET_ESP32C3)
const int CLK = 6, DATA = 7, LE = 10;  // ESP32-C3, same pins as software/esp32 (WIRING.md)
#define TICK() delayMicroseconds(1)  // keep the bit-banged clock well below 1 MHz
#elif defined(__AVR_ATmega32U4__)
const int CLK = 15, DATA = 16, LE = 10;  // Pro Micro / Leonardo: pins printed 15, 16, 10
#define TICK()
#else
const int CLK = 13, DATA = 11, LE = 8;  // Arduino Duemilanove / old firmware
#define TICK()
#endif
const int TILES = 30;
const int OUTS = TILES * 4;  // per board: OUT0 red, OUT1 blue, OUT2 green, OUT3 white

bool out[OUTS];
bool chasing = false;
bool dimRed = false;
bool pinTest = false;
int pinStep = 0;
unsigned long pinNext = 0;
int dimFrame = 0;
int chaseTile = 0;
unsigned long chaseNext = 0;
const unsigned long CHASE_MS = 1500;

// last output first: output 0 (board 1 red) is the last bit shifted in
void shiftAll(bool latch) {
	for (int i = OUTS - 1; i >= 0; i--) {
		digitalWrite(DATA, out[i] ? HIGH : LOW);
		TICK();
		digitalWrite(CLK, HIGH);
		TICK();
		digitalWrite(CLK, LOW);
	}
	if (latch) {
		digitalWrite(LE, HIGH);
		TICK();
		digitalWrite(LE, LOW);
	}
}

void clearAll() {
	for (int i = 0; i < OUTS; i++) out[i] = false;
}

int channel(char c) {
	switch (c) {
		case 'r': return 0;
		case 'b': return 1;
		case 'g': return 2;
		default: return 3;  // w
	}
}

void only(int tile, int ch) {
	clearAll();
	out[(tile - 1) * 4 + ch] = true;
	shiftAll(true);
}

void staticData(bool level) {
	// fill every shift register without latching: LEDs keep what they show,
	// but every board's DATA output now sits at this level
	for (int i = 0; i < OUTS; i++) {
		digitalWrite(DATA, level ? HIGH : LOW);
		TICK();
		digitalWrite(CLK, HIGH);
		TICK();
		digitalWrite(CLK, LOW);
	}
	digitalWrite(DATA, level ? HIGH : LOW);
	Serial.print(F("DATA static "));
	Serial.println(level ? F("HIGH: expect TP39-TP40 about +0.35 V on every board")
	                     : F("LOW: expect TP39-TP40 about -0.35 V on every board"));
}

void help() {
	Serial.println(F("--- diskohedron chain test ---"));
	Serial.println(F("c chase | 16 only board 16 | 16r/g/b/w one colour | a all red 20% | o off"));
	Serial.println(F("h / l DATA static HIGH / LOW for measuring | p pin test | ? help"));
}

void handle(String s) {
	s.trim();
	s.toLowerCase();
	if (s.length() == 0) return;
	chasing = false;
	if (dimRed) {  // don't leave the last PWM frame (all red, full) latched
		clearAll();
		shiftAll(true);
	}
	dimRed = false;
	if (pinTest) {
		digitalWrite(CLK, LOW);
		digitalWrite(DATA, LOW);
		digitalWrite(LE, LOW);
	}
	pinTest = false;
	char c = s.charAt(0);
	if (isDigit(c)) {
		int tile = s.toInt();
		char last = s.charAt(s.length() - 1);
		if (tile < 1 || tile > TILES) {
			Serial.println(F("board must be 1..30"));
			return;
		}
		only(tile, channel(isDigit(last) ? 'w' : last));
		Serial.print(F("only board "));
		Serial.println(tile);
	} else if (c == 'c') {
		chasing = true;
		chaseTile = 0;
		chaseNext = 0;
		Serial.println(F("chase - send anything to stop"));
	} else if (c == 'a') {
		dimRed = true;
		Serial.println(F("all red ~20 % - send anything to stop"));
	} else if (c == 'o') {
		clearAll();
		shiftAll(true);
		Serial.println(F("all off"));
	} else if (c == 'p') {
		pinTest = true;
		pinStep = 0;
		pinNext = 0;
		Serial.println(F("pin test: meter black on JP1 pin 2 (GND), red on pin 3 / 4 / 5 - send anything to stop"));
	} else if (c == 'h') {
		staticData(true);
	} else if (c == 'l') {
		staticData(false);
	} else {
		help();
	}
}

void setup() {
	pinMode(CLK, OUTPUT);
	pinMode(DATA, OUTPUT);
	pinMode(LE, OUTPUT);
	digitalWrite(CLK, LOW);
	digitalWrite(LE, LOW);
	Serial.begin(9600);
	clearAll();
	shiftAll(true);
	help();
}

void loop() {
	if (Serial.available()) handle(Serial.readStringUntil('\n'));
	if (dimRed) {
		// 1 frame on, 4 frames off (~1.5 ms each) = 20 %, ~130 Hz
		clearAll();
		if (dimFrame == 0)
			for (int t = 0; t < TILES; t++) out[t * 4] = true;
		shiftAll(true);
		dimFrame = (dimFrame + 1) % 5;
	}
	if (pinTest && millis() >= pinNext) {
		const int pins[3] = {CLK, DATA, LE};
		const char *names[3] = {"JP1 pin 3 CLK", "JP1 pin 4 DATA", "JP1 pin 5 LE"};
		for (int i = 0; i < 3; i++) digitalWrite(pins[i], i == pinStep ? HIGH : LOW);
		Serial.print(names[pinStep]);
		Serial.print(F(" HIGH (GPIO "));
		Serial.print(pins[pinStep]);
		Serial.println(F("), the other two LOW"));
		pinStep = (pinStep + 1) % 3;
		pinNext = millis() + 2000;
	}
	if (chasing && millis() >= chaseNext) {
		chaseTile = chaseTile % TILES + 1;
		only(chaseTile, 3);
		Serial.print(F("board "));
		Serial.println(chaseTile);
		chaseNext = millis() + CHASE_MS;
	}
}
