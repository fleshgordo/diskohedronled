#include "ArtNet.h"
#include "config.h"
#include "StreamBuffer.h"

#include <AsyncUDP.h>
#include <WiFi.h>

namespace artnet {

static AsyncUDP udp;
static const uint16_t PORT = 6454;
static const uint16_t OP_POLL = 0x2000;
static const uint16_t OP_POLL_REPLY = 0x2100;
static const uint16_t OP_DMX = 0x5000;
static const char ID[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};

// the lamp's own address on the network the request came from (hotspot or venue WiFi)
static IPAddress ownIpFor(const IPAddress &peer) {
	IPAddress ap = WiFi.softAPIP();
	if (peer[0] == ap[0] && peer[1] == ap[1] && peer[2] == ap[2]) return ap;
	return WiFi.localIP();
}

// ArtPollReply, layout from the Art-Net 4 spec (239 bytes)
static void sendPollReply(const IPAddress &peer) {
	uint8_t r[239] = {};
	IPAddress ip = ownIpFor(peer);
	memcpy(r, ID, 8);
	r[8] = OP_POLL_REPLY & 0xff;
	r[9] = OP_POLL_REPLY >> 8;
	for (int i = 0; i < 4; i++) r[10 + i] = ip[i];
	r[14] = PORT & 0xff;  // little endian
	r[15] = PORT >> 8;
	r[17] = 1;                                   // firmware version
	r[18] = (ARTNET_UNIVERSE >> 8) & 0x7f;       // NetSwitch
	r[19] = (ARTNET_UNIVERSE >> 4) & 0x0f;       // SubSwitch
	r[21] = 0xff;                                // OEM: unknown
	r[23] = 0xc0;                                // Status1: indicators normal
	strncpy((char *)r + 26, HOSTNAME, 17);       // ShortName
	strncpy((char *)r + 44, "Diskohedron LED - 30 RGBW faces", 63);  // LongName
	strncpy((char *)r + 108, "#0001 [0001] ok", 63);                  // NodeReport
	r[173] = 1;                                  // NumPorts
	r[174] = 0x80;                               // port 1: outputs DMX from Art-Net
	r[182] = 0x80;                               // GoodOutput: data is being output
	r[190] = ARTNET_UNIVERSE & 0x0f;             // SwOut
	WiFi.macAddress(r + 201);
	for (int i = 0; i < 4; i++) r[207 + i] = ip[i];  // BindIp
	r[211] = 1;                                  // BindIndex
	r[212] = 0x08;                               // Status2: 15-bit port address
	udp.writeTo(r, sizeof(r), peer, PORT);
}

static void onPacket(AsyncUDPPacket &p) {
	const uint8_t *d = p.data();
	size_t len = p.length();
	if (len < 10 || memcmp(d, ID, 8) != 0) return;
	uint16_t op = d[8] | (d[9] << 8);

	if (op == OP_POLL) {
		sendPollReply(p.remoteIP());
		return;
	}
	if (op != OP_DMX || len < 18) return;

	uint16_t universe = d[14] | ((d[15] & 0x7f) << 8);  // SubUni | Net
	if (universe != ARTNET_UNIVERSE) return;
	size_t dmxLen = (d[16] << 8) | d[17];
	if (dmxLen > len - 18) dmxLen = len - 18;

	// senders may send fewer than 512 channels: missing ones count as 0
	uint8_t frame[STREAM_FRAME_BYTES] = {};
	size_t first = ARTNET_START_CHANNEL - 1;
	for (size_t i = 0; i < STREAM_FRAME_BYTES && first + i < dmxLen; i++) frame[i] = d[18 + first + i];
	stream::push(frame, ARTNET_TIMEOUT_MS);
}

void begin() {
	if (udp.listen(PORT)) udp.onPacket(onPacket);
	else Serial.println("Art-Net: cannot listen on UDP 6454");
}

}  // namespace artnet
