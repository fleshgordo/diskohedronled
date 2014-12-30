#include <DHT.h>
/***********************************************/
/* LED DRIVER                                  */
/***********************************************/
const int ShiftPWM_latchPin=8;
const bool ShiftPWM_invertOutputs = false; 
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>   // include ShiftPWM.h after setting the pins!

unsigned char maxBrightness = 255;
unsigned char pwmFrequency = 75;


int numRGBleds = 30;

int bitsPerBoard = 4;
int registerSize = 8;

int numRegisters = (numRGBleds+1)*bitsPerBoard/registerSize;

/***********************************************/
  /* TEMPERATURE SENSOR                        */
/***********************************************/

#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

int sTimer=0;
int switchOffTemperature=26;

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);

/***********************************************/
/* END                                         */
/***********************************************/

void setup(){
  Serial.begin(9600);
  dht.begin();
  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  ShiftPWM.Start(pwmFrequency,maxBrightness);
}

void temperature_check() {
float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("Failed to read from DHT");
  } else {
    if (t > switchOffTemperature) {
      Serial.print("overheating: ");
      Serial.println(t);
      ShiftPWM.SetAll(0);
    }
    else {
      // everything kewl, do nothing
    }
  }
}

void loop()
{    
  ShiftPWM.SetAll(0);
 
  alternateAllRGBW(255,20); //fade red then green then blue then white (after each other) for all leds
  //fadeAllWhite(255,20);
  fadeAllRGBW(255,20); //fade red and green and blue and white (at same time) for all leds
  
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

void fadeAllWhite(int maxB, float delayFade){
  int br=0;
  int d=delayFade*(255.0/(float)maxB);
  
  for(; br<maxB; br++){
    ShiftPWM.SetAllRGBW(0,0,0,br);
    delay(d);
  }
  for(; br>0; br--){
    ShiftPWM.SetAllRGBW(0,0,0,br);
    delay(d);
  }
}
