Diskohedronled
==============

Diskohedron (led version) 2014

A triacontahedron with 30 laser-etched rhombic faces, each containing a 4W RGBW(+White) LED. The first prototype consists of 30 3d-printed rhombic boxes which form a Triacontahedron. 

  * 30 x 4W RGBW
  * WIFI interface (android/iphone controllable, served by nodejs on a raspberry pi)
  * Modular
  
Thanks to Bread'n'Butter in Lausanne and HEAD Option interaction in Geneva for making us use their Makerbot printers.

## ESP32 version

A rewrite without Raspberry Pi and Arduino lives in [software/esp32](software/esp32/README.md):
one ESP32 drives the boards and serves the web UI from its own hotspot.
The rest of this file describes the original Arduino + Raspberry Pi setup.

## Hardware

4 wires are connected to the Arduino.

  - GND -> GND
  - CLK -> Pin 13
  - DATA -> Pin 11
  - LE -> Pin 8 

Raspi Lite OS

## Power supply

Shottkey Diode (STPS20L25CT) or MBR2545 for polarity
Power supply specs: 24V DC (5A), 120W minimum

## Install log

Disabling some services

```
sudo systemctl disable bluetooth.service
sudo systemctl disable avahi-daemon.service
sudo systemctl disable triggerhappy.service
```

Disable swap 

```
sudo dphys-swapfile swapoff
sudo systemctl  disable  dphys-swapfile
```

installed log2ram to have /var/log on tmpfs with options 

Installing

```
sudo apt install git
sudo apt install hostapd dnsmasq
sudo apt install node npm 
npm install --save socket.io
npm install --save serialport
npm install --save express
```

in boot/config.txt on rpi
```
# Turn off LEDs
dtparam=pwr_led_trigger=none
dtparam=pwr_led_activelow=off
dtparam=act_led_trigger=none
dtparam=act_led_activelow=off
dtparam=eth_led0=14
dtparam=eth_led1=14
```

### Get software
```
git clone git@github.com:fleshgordo/diskohedronled.git
```

### Setup accesspoint

Two Wlan adapters (built-in wlan0, Ra2800 via USB wlan1)

```
nmcli dev wifi connect "CyberLazanje" ifname wlan0 password "YourNetworkPassword"
nmcli device wifi hotspot ifname wlan0 ssid diskoledron password diskodisko
nmcli con modify Hotspot connection.autoconnect yes
```

Wifi credentials:
SSID: diskoledron
PWD:  diskodisko

## Firmware

Install ShiftPWM library to Arduino. 

Compile in Arduino for Duemilanove board. Locate the hex file (set in Arduino preferences avrdude verbose)

```
./upload.sh /Users/gordo/Library/Caches/arduino/sketches/AF2493AC7716A44163A3BF0821558275/LampFirmware.ino.with_bootloader.hex
```

