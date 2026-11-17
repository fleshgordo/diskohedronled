/************************************************************************************************************************************
 * ShiftPWM blocking RGB fades example, (c) Elco Jacobs, updated August 2012.
 *
 * ShiftPWM blocking RGB fades example. This example uses simple delay loops to create fades.
 * If you want to change the fading mode based on inputs (sensors, buttons, serial), use the non-blocking example as a starting point.
 * Please go to www.elcojacobs.com/shiftpwm for documentation, fuction reference and schematics.
 * If you want to use ShiftPWM with LED strips or high power LED's, visit the shop for boards.
 ************************************************************************************************************************************/

// Clock and data pins are pins from the hardware SPI, you cannot choose them yourself if you use the hardware SPI.
// Data pin is MOSI (Uno and earlier: 11, Leonardo: ICSP 4, Mega: 51, Teensy 2.0: 2, Teensy 2.0++: 22)
// Clock pin is SCK (Uno and earlier: 13, Leonardo: ICSP 3, Mega: 52, Teensy 2.0: 1, Teensy 2.0++: 21)

// You can choose the latch pin yourself.
const int ShiftPWM_latchPin = 8;

// If your LED's turn on if the pin is low, set this to true, otherwise set it to false.
const bool ShiftPWM_invertOutputs = false;

// You can enable the option below to shift the PWM phase of each shift register by 8 compared to the previous.
// This will slightly increase the interrupt load, but will prevent all PWM signals from becoming high at the same time.
// This will be a bit easier on your power supply, because the current peaks are distributed.
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>  // include ShiftPWM.h after setting the pins!

unsigned char maxBrightness = 255;
unsigned char pwmFrequency = 75;

int numRGBleds = 23;
int bitsPerBoard = 4;
int registerSize = 8;

int numRegisters = (numRGBleds+1)*bitsPerBoard/registerSize;

void setup() {
  Serial.begin(9600);

  // // Sets the number of 8-bit registers that are used.
  // ShiftPWM.SetAmountOfRegisters(numRegisters);

  // // SetPinGrouping allows flexibility in LED setup.
  // // If your LED's are connected like this: RRRRGGGGBBBBRRRRGGGGBBBB, use SetPinGrouping(4).
  // ShiftPWM.SetPinGrouping(1);  //This is the default, but I added here to demonstrate how to use the funtion

  // ShiftPWM.Start(pwmFrequency, maxBrightness);
    ShiftPWM.SetPinGrouping(1);
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  ShiftPWM.Start(pwmFrequency,maxBrightness);
  ShiftPWM.PrintInterruptLoad();
  ShiftPWM.SetAllRGBW(200,200,200,200);
}



void loop() {
  for (int led = 0; led < numRGBleds; led++) {               // loop over all LED's
    for (int intensity = 0; intensity < 255; intensity++) {  // loop over all LED's
      ShiftPWM.SetRGBW(led, 0, 0, 0, intensity);
      delay(1);
    }
    for (int intensity = 255; intensity > 0; intensity--) {  // loop over all LED's
      ShiftPWM.SetRGBW(led, 0, 0, 0, intensity);
      delay(1);
    }
  }
}
