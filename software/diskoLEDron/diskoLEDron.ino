
const int ShiftPWM_latchPin=8;
const bool ShiftPWM_invertOutputs = false; 
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>   // include ShiftPWM.h after setting the pins!

unsigned char maxBrightness = 23;
unsigned char pwmFrequency = 75;


int numRGBleds = 5;

int bitsPerBoard = 4;
int registerSize = 8;

int numRegisters = (numRGBleds+1)*bitsPerBoard/registerSize;


void setup(){
  Serial.begin(9600);
  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  ShiftPWM.Start(pwmFrequency,maxBrightness);
}


void loop()
{    
  ShiftPWM.SetAll(0);

  alternateAllRGBW(255,20); //fade red then green then blue then white (after each other) for all leds
  //fadeAllRGBW(255,20); //fade red and green and blue and white (at same time) for all leds
}




void alternateAllRGBW(int maxB, float delayFade){
  int r=0;
  int g=0;
  int b=0;
  int w=0;
  int d=delayFade*(255.0/(float)maxB);

  for(; r<maxB; r++){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; r>0; r--){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; g<maxB; g++){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; g>0; g--){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; b<maxB; b++){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; b>0; b--){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; w<maxB; w++){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
  for(; w>0; w--){
    ShiftPWM.SetAllRGBW(r,g,b,w);
    delay(d);
  }
}


void fadeAllRGBW(int maxB, float delayFade){
  int br=0;
  int d=delayFade*(255.0/(float)maxB);
  
  for(; br<maxB; br++){
    ShiftPWM.SetAllRGBW(br,br,br,br);
    delay(d);
  }
  for(; br>0; br--){
    ShiftPWM.SetAllRGBW(br,br,br,br);
    delay(d);
  }
}



