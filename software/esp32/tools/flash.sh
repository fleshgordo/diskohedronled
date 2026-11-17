#!/usr/bin/env bash
# Build the lamp firmware and upload it, by USB or over WiFi (OTA).
#
#   tools/flash.sh              USB if a board is plugged in, otherwise WiFi
#   tools/flash.sh usb [port]   USB (port found automatically if omitted)
#   tools/flash.sh ota [host]   WiFi, default diskoledron.local
#   tools/flash.sh build        only build
#
#   CHIP=esp32 tools/flash.sh   for the classic ESP32 instead of the C3
set -euo pipefail

CHIP="${CHIP:-esp32c3}"
FQBN="esp32:esp32:${CHIP}:PartitionScheme=min_spiffs"
OTA_PASS="diskodisko"  # OTA_PASSWORD in config.h

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH="$ROOT/diskohedron"
BUILD="$ROOT/build/$CHIP"  # own folder: doesn't clash with the Arduino IDE's cache

mode="${1:-auto}"
target="${2:-}"

usb_port() {
	ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1 || true
}

if [[ "$mode" == auto ]]; then
	target="$(usb_port)"
	if [[ -n "$target" ]]; then mode=usb; else mode=ota; fi
fi

echo "== web/ -> web_assets.h"
python3 "$ROOT/tools/embed_web.py"

echo "== build ($FQBN)"
out="$(arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD" "$SKETCH")"
echo "$out" | tail -2
# keep room for updates over WiFi: the program may use at most the 1.9 MB app slot
pct="$(echo "$out" | grep -oE '\(([0-9]+)%\)' | head -1 | tr -dc 0-9)"
if [[ -n "$pct" && "$pct" -ge 85 ]]; then
	echo "!! firmware uses ${pct}% of the app partition - close to the limit, consider disabling features" >&2
fi

case "$mode" in
build)
	echo "binary: $BUILD/diskohedron.ino.bin"
	;;
usb)
	port="${target:-$(usb_port)}"
	[[ -n "$port" ]] || { echo "no USB board found" >&2; exit 1; }
	echo "== upload via USB ($port)"
	echo "   (close the Serial Monitor if the port is busy)"
	arduino-cli upload --fqbn "$FQBN" --input-dir "$BUILD" -p "$port" "$SKETCH"
	;;
ota)
	host="${target:-diskoledron.local}"
	espota="$(ls -d "$HOME"/Library/Arduino15/packages/esp32/hardware/esp32/*/tools/espota.py | sort -V | tail -1)"
	echo "== upload via WiFi ($host)"
	echo "   (the Mac must be on the same network as the lamp - your home WiFi if the lamp joined it)"
	python3 "$espota" -r -i "$host" -p 3232 --auth="$OTA_PASS" -f "$BUILD/diskohedron.ino.bin" 2>&1 |
		tr '\r' '\n' | grep -vE '^Uploading: .* [0-9]{1,2}% $'
	;;
*)
	sed -n '2,9p' "$0"
	exit 1
	;;
esac
echo "== done"
