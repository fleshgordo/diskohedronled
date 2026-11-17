/*
ArtNet - receive the lamp's pixels from Resolume, TouchDesigner or any Art-Net source.

  - UDP port 6454, universe ARTNET_UNIVERSE (config.h)
  - channels ARTNET_START_CHANNEL .. +119: tile 1 R, G, B, W, tile 2 R, G, B, W, ...
  - answers ArtPoll, so the lamp shows up as "diskoledron" in node lists
  - while data arrives it overrides the selected mode (global brightness still applies)

Setup for Resolume / TouchDesigner: tools/artnet/README.md
*/
#pragma once

namespace artnet {
void begin();
}
