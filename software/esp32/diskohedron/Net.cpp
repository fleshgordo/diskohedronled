#include "Net.h"
#include "config.h"
#include "web_assets.h"
#include "StreamBuffer.h"
#include "ArtNet.h"
#include "LedChain.h"

extern LedChain leds;

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <Update.h>
#include <vector>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static DNSServer dns;
static const IPAddress AP_IP(192, 168, 4, 1);

// messages from the WebSocket task to loop()
struct Inbox {
	uint32_t client;  // 0 = message, otherwise "send everything to this new client"
	uint32_t seq;     // HTTP API: request number, so the request can wait until it is handled
	char text[256];
};
static QueueHandle_t inbox;

static void serveAsset(AsyncWebServerRequest *req, const WebAsset &a) {
	AsyncWebServerResponse *res = req->beginResponse(200, a.mime, a.data, a.len);
	res->addHeader("Content-Encoding", "gzip");
	res->addHeader("Cache-Control", "no-cache");
	req->send(res);
}

// ------------------------------------------------------------------ home WiFi (station)
// Set from the web UI and kept in /wifi.json. STA_SSID / STA_PASS in config.h are only the
// default for the first boot. If the network isn't reachable, the lamp stops trying for a
// minute: while it searches, it hops channels, which disturbs the hotspot.
static String staSsid = STA_SSID, staPass = STA_PASS;
static uint32_t staTriedAt = 0;  // start of the current attempt, 0 = connected or off
static uint32_t staGaveUpAt = 0; // 0 = not given up
static const uint32_t STA_TRY_MS = 20000, STA_RETRY_MS = 60000;

static void loadWifi() {
	File f = LittleFS.open("/wifi.json", "r");
	if (!f) return;
	JsonDocument doc;
	if (!deserializeJson(doc, f)) {
		staSsid = doc["ssid"] | "";
		staPass = doc["pass"] | "";
	}
	f.close();
}

static void saveWifi() {
	JsonDocument doc;
	doc["ssid"] = staSsid;
	doc["pass"] = staPass;
	leds.holdForFlash(100);
	File f = LittleFS.open("/wifi.json", "w");
	if (!f) return;
	serializeJson(doc, f);
	f.close();
}

static void staStart() {
	staGaveUpAt = 0;
	staTriedAt = 0;
	WiFi.disconnect();
	if (staSsid.isEmpty()) {
		WiFi.mode(WIFI_AP);
		return;
	}
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(staSsid.c_str(), staPass.c_str());
	staTriedAt = millis() | 1;
}

static void staLoop() {
	if (staSsid.isEmpty()) return;
	uint32_t now = millis();
	if (WiFi.status() == WL_CONNECTED) {
		if (staTriedAt) Serial.printf("joined %s as %s\n", staSsid.c_str(), WiFi.localIP().toString().c_str());
		staTriedAt = 0;
		return;
	}
	if (staGaveUpAt) {
		if (now - staGaveUpAt >= STA_RETRY_MS) staStart();
	} else if (!staTriedAt) {
		staTriedAt = now | 1;  // connection lost: the WiFi driver reconnects by itself
	} else if (now - staTriedAt >= STA_TRY_MS) {
		Serial.printf("%s not reachable, retry in %u s\n", staSsid.c_str(), (unsigned)(STA_RETRY_MS / 1000));
		WiFi.disconnect();
		staGaveUpAt = now | 1;
	}
}

static const char *staStatus() {
	if (staSsid.isEmpty()) return "off";
	if (WiFi.status() == WL_CONNECTED) return "connected";
	return staGaveUpAt ? "failed" : "connecting";
}

static String wifiJson() {
	JsonDocument doc;
	doc["type"] = "wifi";
	doc["ssid"] = staSsid;  // never the password
	doc["status"] = staStatus();
	if (WiFi.status() == WL_CONNECTED) doc["ip"] = WiFi.localIP().toString();
	doc["hostname"] = HOSTNAME;
	int n = WiFi.scanComplete();
	doc["scanning"] = n == WIFI_SCAN_RUNNING;
	JsonArray nets = doc["networks"].to<JsonArray>();
	for (int i = 0; i < n; i++) {  // strongest first, without duplicates
		String name = WiFi.SSID(i);
		bool seen = name.isEmpty();
		for (JsonVariant v : nets) seen |= name == v.as<const char *>();
		if (!seen) nets.add(name);
	}
	String out;
	serializeJson(doc, out);
	return out;
}

// wifi.* messages from the UI; false = not a WiFi message
static bool handleWifi(const char *text) {
	JsonDocument doc;
	if (deserializeJson(doc, text)) return false;
	const char *type = doc["type"] | "";
	if (!strcmp(type, "wifi.scan")) {
		WiFi.scanDelete();
		WiFi.scanNetworks(true);
	} else if (!strcmp(type, "wifi.set")) {
		staSsid = doc["ssid"] | "";
		staSsid.trim();
		staPass = staSsid.isEmpty() ? "" : (doc["pass"] | "");
		saveWifi();
		staStart();
	} else {
		return false;
	}
	return true;
}

// ------------------------------------------------------------------ HTTP JSON API (see ../API.md)
// Requests arrive in the async TCP task. They are turned into the same messages the web page
// sends, queued for loop(), and answered once loop() has handled them. Answers come from
// copies of the JSON made in loop(), so the State is never read from this task.
static SemaphoreHandle_t cacheLock;
static String cacheState, cachePresets, cacheSchema;
static uint32_t apiSeq = 0;               // last request number handed out (TCP task only)
static volatile uint32_t apiDone = 0;     // last request number handled by loop()
static const uint32_t API_WAIT_MS = 500;

static String cached(const String &s) {
	xSemaphoreTake(cacheLock, portMAX_DELAY);
	String copy = s;
	xSemaphoreGive(cacheLock);
	return copy;
}

static void sendJson(AsyncWebServerRequest *req, int code, const String &json) {
	AsyncWebServerResponse *res = req->beginResponse(code, "application/json", json);
	res->addHeader("Access-Control-Allow-Origin", "*");
	req->send(res);
}

static int modeId(JsonVariantConst v) {
	if (v.is<int>()) return v.as<int>() >= 0 && v.as<int>() < MODE_COUNT ? v.as<int>() : -1;
	const char *name = v | "";
	for (auto &m : MODES)
		if (!strcasecmp(m.name, name)) return m.id;
	return -1;
}

// one API request -> web page messages; returns an error text or nullptr
static const char *apiMessages(JsonVariantConst req, int power, std::vector<String> &out) {
	auto add = [&](JsonDocument &d) {
		String s;
		serializeJson(d, s);
		out.push_back(s);
	};
	auto set = [&](const char *key, int value) {
		JsonDocument d;
		d["type"] = "set";
		d["key"] = key;
		d["value"] = value;
		add(d);
	};
	bool turnOn = false;  // choosing what to show also switches the lamp on

	if (!req["preset"].isNull()) {
		JsonDocument d;
		d["type"] = "preset.load";
		d["name"] = req["preset"];
		add(d);
		turnOn = true;
	}
	if (!req["mode"].isNull()) {
		int m = modeId(req["mode"]);
		if (m < 0) return "unknown mode";
		JsonDocument d;
		d["type"] = "mode";
		d["mode"] = m;
		add(d);
		turnOn = true;
	}
	if (!req["color"].isNull()) {
		JsonArrayConst c = req["color"];
		if (c.size() < 3 || c.size() > 4) return "color must be [r, g, b] or [r, g, b, w]";
		JsonDocument d;
		d["type"] = "mode";
		d["mode"] = (int)MODE_COLOR;
		add(d);
		static const char *KEYS[4] = {"color.r", "color.g", "color.b", "color.w"};
		for (int i = 0; i < 4; i++) set(KEYS[i], i < (int)c.size() ? c[i].as<int>() : 0);
		turnOn = true;
	}
	for (JsonPairConst kv : req["values"].as<JsonObjectConst>()) set(kv.key().c_str(), kv.value() | 0);
	if (!req["brightness"].isNull()) set("brightness", req["brightness"] | 0);

	JsonVariantConst on = req["on"];
	if (on.is<const char *>() && !strcmp(on, "toggle")) set("power", !power);
	else if (!on.isNull()) set("power", on.as<bool>() ? 1 : 0);
	else if (turnOn && !power) set("power", 1);
	return nullptr;
}

// query parameters (?on=1&brightness=40&color=255,0,0,0) -> the same JSON as a POST body
static void queryToJson(AsyncWebServerRequest *req, JsonDocument &doc) {
	for (size_t i = 0; i < req->params(); i++) {
		const AsyncWebParameter *p = req->getParam(i);
		if (p->isFile() || p->isPost() && req->contentType().startsWith("application/json")) continue;
		const String &k = p->name(), &v = p->value();
		bool number = v.length() && strspn(v.c_str(), "-0123456789") == v.length();
		if (k == "color") {
			JsonArray c = doc["color"].to<JsonArray>();
			for (int from = 0; from <= (int)v.length();) {
				int comma = v.indexOf(',', from);
				if (comma < 0) comma = v.length();
				c.add(v.substring(from, comma).toInt());
				from = comma + 1;
			}
		} else if (k == "on") {
			if (v == "toggle") doc["on"] = "toggle";
			else doc["on"] = v == "1" || v.equalsIgnoreCase("true") || v.equalsIgnoreCase("on");
		} else if (k == "mode" || k == "preset" || k == "brightness") {
			if (number && k != "preset") doc[k] = v.toInt();
			else doc[k] = v;
		} else if (number) {
			doc["values"][k] = v.toInt();  // a parameter, e.g. ripple.speed=20
		}
	}
}

static void apiRequest(AsyncWebServerRequest *req, JsonVariantConst body, int power) {
	std::vector<String> msgs;
	if (const char *err = apiMessages(body, power, msgs)) {
		sendJson(req, 400, String("{\"error\":\"") + err + "\"}");
		return;
	}
	uint32_t seq = 0;
	for (auto &m : msgs) {
		Inbox msg = {};
		msg.seq = seq = ++apiSeq;
		strlcpy(msg.text, m.c_str(), sizeof(msg.text));
		if (xQueueSend(inbox, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
			sendJson(req, 503, "{\"error\":\"busy\"}");
			return;
		}
	}
	for (uint32_t t = millis(); seq && (int32_t)(apiDone - seq) < 0 && millis() - t < API_WAIT_MS;) vTaskDelay(2);
	sendJson(req, 200, cached(cacheState));
}

// ------------------------------------------------------------------ firmware upload from the page
// POST /update with the .bin (from tools/flash.sh build, or Arduino "Export Compiled Binary")
// and the OTA password in the X-Password header. Written to the other app slot; the lamp
// only boots it after a complete, checked upload, so a failed upload changes nothing.
volatile bool g_firmwareUpdating = false;
static uint32_t restartAt = 0;
static String updateError;

static void onUpdateData(AsyncWebServerRequest *req, const String &, size_t index, uint8_t *data, size_t len,
                         bool final) {
	if (index == 0) {
		updateError = "";
		const AsyncWebHeader *pw = req->getHeader("X-Password");
		if (!pw || pw->value() != OTA_PASSWORD) updateError = "wrong password";
		// app image header: magic 0xE9 at byte 0, chip id at bytes 12-13
		else if (len < 16 || data[0] != 0xE9) updateError = "not an ESP32 firmware file";
		else if ((data[12] | data[13] << 8) != CONFIG_IDF_FIRMWARE_CHIP_ID)
			updateError = "firmware for a different chip (ESP32 vs ESP32-C3?)";
		else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) updateError = Update.errorString();
		if (!updateError.isEmpty()) return;
		g_firmwareUpdating = true;
		req->onDisconnect([] {  // browser closed mid-upload: forget it, LEDs back on
			if (!Update.isFinished()) {
				Update.abort();
				g_firmwareUpdating = false;
			}
		});
		Serial.println("firmware upload started");
	}
	if (!updateError.isEmpty()) return;
	if (Update.write(data, len) != len) updateError = Update.errorString();
	if (final && updateError.isEmpty() && !Update.end(true)) updateError = Update.errorString();
	if (!updateError.isEmpty()) {
		Update.abort();
		g_firmwareUpdating = false;
	}
}

static void onUpdateDone(AsyncWebServerRequest *req) {
	bool ok = updateError.isEmpty() && Update.isFinished();
	if (!ok && updateError.isEmpty()) updateError = "no complete file received";
	if (ok) {
		restartAt = millis() + 1000;  // let the answer reach the browser first
		Serial.println("firmware upload done, restarting");
	}
	g_firmwareUpdating = ok;
	AsyncWebServerResponse *res = req->beginResponse(ok ? 200 : 400, "application/json",
	                                                 ok ? "{\"ok\":true}" : "{\"error\":\"" + updateError + "\"}");
	res->addHeader("Connection", "close");
	req->send(res);
}

static bool isOwnHost(AsyncWebServerRequest *req) {
	String host = req->host();
	return host == AP_IP.toString() || host == WiFi.localIP().toString() || host == HOSTNAME ".local" ||
	       host == HOSTNAME;
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
                      size_t len) {
	Inbox msg = {};
	if (type == WS_EVT_CONNECT) {
		msg.client = client->id();
		xQueueSend(inbox, &msg, 0);
	} else if (type == WS_EVT_DATA) {
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		bool whole = info->final && info->index == 0 && info->len == len;
		// binary = one video frame from video2lamp.py --stream
		if (whole && info->opcode == WS_BINARY && len == STREAM_FRAME_BYTES) {
			stream::push(data);
			return;
		}
		// UI messages are tiny: only accept complete, single-frame text messages
		if (whole && info->opcode == WS_TEXT && len < sizeof(msg.text)) {
			memcpy(msg.text, data, len);
			xQueueSend(inbox, &msg, 0);
		}
	}
}

void Net::begin() {
	inbox = xQueueCreate(16, sizeof(Inbox));

	// --- WiFi: own hotspot always, venue network optional
	WiFi.persistent(false);
	WiFi.setHostname(HOSTNAME);
	loadWifi();
	WiFi.mode(staSsid.isEmpty() ? WIFI_AP : WIFI_AP_STA);
	WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);
	Serial.printf("hotspot %s at %s\n", AP_SSID, AP_IP.toString().c_str());
	// don't wait for the home network: it connects in the background (see staLoop())
	if (!staSsid.isEmpty()) staStart();

	// --- captive portal: every DNS name points to the lamp
	dns.setErrorReplyCode(DNSReplyCode::NoError);
	dns.start(53, "*", AP_IP);
	MDNS.begin(HOSTNAME);
	MDNS.addService("http", "tcp", 80);

	// --- HTTP JSON API
	cacheLock = xSemaphoreCreateMutex();
	cacheSchema = m_state.schemaJson();
	cachePresets = m_state.presetsJson();
	cacheState = m_state.stateJson();
	State *st = &m_state;
	auto *apiPost = new AsyncCallbackJsonWebHandler("/api/state", [st](AsyncWebServerRequest *r, JsonVariant &json) {
		apiRequest(r, json, st->v(P_POWER));
	});
	apiPost->setMethod(HTTP_POST | HTTP_PUT);
	server.addHandler(apiPost);
	server.on("/api/state", HTTP_GET | HTTP_POST, [st](AsyncWebServerRequest *r) {  // query parameters
		JsonDocument doc;
		queryToJson(r, doc);
		apiRequest(r, doc, st->v(P_POWER));
	});
	server.on("/api/schema", HTTP_GET, [](AsyncWebServerRequest *r) { sendJson(r, 200, cached(cacheSchema)); });
	server.on("/update", HTTP_POST, onUpdateDone, onUpdateData);
	// diagnostics: late LED frames ([uptime ms, dark µs] each), WiFi, memory
	server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest *r) {
		String s = "{\"uptimeMs\":" + String(millis()) + ",\"led\":" + leds.statsJson() +
		           ",\"rssi\":" + String(WiFi.RSSI()) + ",\"channel\":" + String(WiFi.channel()) +
		           ",\"freeHeap\":" + String(ESP.getFreeHeap()) + "}";
		sendJson(r, 200, s);
	});
	server.on("/api/presets", HTTP_GET, [](AsyncWebServerRequest *r) { sendJson(r, 200, cached(cachePresets)); });

	// --- web
	ws.onEvent(onWsEvent);
	server.addHandler(&ws);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) { serveAsset(r, ASSET_INDEX_HTML); });
	server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *r) { serveAsset(r, ASSET_APP_JS); });
	server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *r) { serveAsset(r, ASSET_STYLE_CSS); });
	server.onNotFound([](AsyncWebServerRequest *r) {
		// phones probe some URL on a foreign host to detect captive portals -> send them to the UI
		if (!isOwnHost(r)) r->redirect(String("http://") + AP_IP.toString() + "/");
		else r->redirect("/");
	});
	server.begin();

	// --- pixels from Resolume / TouchDesigner
	artnet::begin();

	// --- firmware updates over WiFi (arduino-cli upload -p diskoledron.local ...)
	ArduinoOTA.setHostname(HOSTNAME);
	ArduinoOTA.setPassword(OTA_PASSWORD);
	// LEDs go dark while an update is written
	ArduinoOTA.onStart([] { g_firmwareUpdating = true; });
	ArduinoOTA.onError([](ota_error_t) { g_firmwareUpdating = false; });
	ArduinoOTA.begin();
}

void Net::loop() {
	dns.processNextRequest();
	ArduinoOTA.handle();
	if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();

	staLoop();

	Inbox msg;
	bool stateChanged = false, presetsChanged = false, wifiChanged = false;
	uint32_t apiHandled = 0;
	while (xQueueReceive(inbox, &msg, 0) == pdTRUE) {
		if (msg.seq) apiHandled = msg.seq;
		if (msg.client) {
			ws.text(msg.client, m_state.schemaJson());
			ws.text(msg.client, m_state.presetsJson());
			ws.text(msg.client, m_state.stateJson());
			ws.text(msg.client, wifiJson());
			ws.text(msg.client, m_state.calibJson());
			continue;
		}
		if (handleWifi(msg.text)) {
			wifiChanged = true;
			continue;
		}
		bool p = false;
		stateChanged |= m_state.handle(msg.text, strlen(msg.text), p);
		presetsChanged |= p;
	}
	// one broadcast per loop keeps slider drags from flooding the clients
	if (presetsChanged || stateChanged) {
		String presets = presetsChanged ? m_state.presetsJson() : String();
		String state = stateChanged ? m_state.stateJson() : String();
		if (presetsChanged) ws.textAll(presets);
		if (stateChanged) ws.textAll(state);
		xSemaphoreTake(cacheLock, portMAX_DELAY);  // copies for the HTTP API
		if (presetsChanged) cachePresets = presets;
		if (stateChanged) cacheState = state;
		xSemaphoreGive(cacheLock);
	}
	if (apiHandled) apiDone = apiHandled;
	if (m_state.takeCalibChanged()) ws.textAll(m_state.calibJson());

	// WiFi status, IP or scan results changed -> tell the UI
	static String lastWifi;
	static uint32_t lastWifiCheck = 0;
	if (wifiChanged || millis() - lastWifiCheck > 500) {
		lastWifiCheck = millis();
		String w = wifiJson();
		if (w != lastWifi) {
			lastWifi = w;
			ws.textAll(w);
		}
	}

	static uint32_t lastCleanup = 0;
	if (millis() - lastCleanup > 1000) {
		lastCleanup = millis();
		ws.cleanupClients();
	}
}
