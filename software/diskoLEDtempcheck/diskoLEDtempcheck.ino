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
int switchOffTemperature=35;

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
      Serial.println(t);
      // everything kewl, do nothing
    }
  }
}

void loop()
{    
  ShiftPWM.SetAll(0);
  temperature_check();

}
