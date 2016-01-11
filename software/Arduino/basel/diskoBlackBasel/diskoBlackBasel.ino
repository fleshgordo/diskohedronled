#include <math.h>
//#include <TimerOne.h>
#include <TimerThree.h>


//BATMAN!!!
volatile const int lampe [] = {
                  9, 10, 8, 11, 7, 12, 6, 13,
                  22,21,24,20,26,19,28,18,
                  32,30,36,34,40,38,44,42,
                  33,31,37,35,41,39,45,43
};

/*
//KYLIE!!!
volatile const int lampe [] = {  9, 10, 8, 11, 7, 12, 6, 13,
                  24, 26, 21, 20, 28, 19, 30, 18,
                  32, 38, 36, 40, 42, 39, 41, 43,
                  45, 47, 33, 35, 37, 44, 46, 34 
                };
*/

volatile int dim [] =  {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

volatile int wait [] =  {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

volatile const int numBulbs=32;
volatile int bright=67;
volatile int toBright=67;

volatile int decr=-15;
volatile int toDecr=-15;

volatile float speedi=0.013;
volatile float toSpeed=speedi;

int heartbeat=0;

volatile boolean fire[32];
volatile int z=0;




long bfire = B00000000;
float sine[515];

const int freqStep = 78;    // Set the delay for the frequency of power (65 for 60Hz, 78 for 50Hz) per step (using 128 steps)

boolean switcher=false;
boolean strobe=false;

int strobeCount=0;
int strobeFreq=2;

/** logic variables */
float count=2*PI;

int fadeSpeed=50;


void setup() {
  //Serial.begin(115200);
  Serial.begin(115200);
  for (int i = 0;i<numBulbs; i++) {
    fire[i]=false;
    dim[i]=85;
    pinMode(lampe[i], OUTPUT);
    digitalWrite(lampe[i],LOW);
  }
  //attachInterrupt(1, zero_cross_detect, RISING);   // Attach an Interupt to Pin 2 (interupt 0) for Zero Cross Detection
  attachInterrupt(1, zero_cross_detect, RISING);   // Attach an Interupt to Pin 2 (interupt 0) for Zero Cross Detection
  //Timer1.initialize(freqStep);
  //Timer1.setPeriod(freqStep);
  //Timer1.attachInterrupt(dim_check);
  Timer3.initialize(freqStep);
  Timer3.setPeriod(freqStep);
  Timer3.attachInterrupt(dim_check);
}

/** function to be fired at the zero crossing */
void zero_cross_detect() {
   //calcSine();
   for(int i=0;i<numBulbs;i++){
     fire[i]=true;
   }
   z = 0;
}

/** Function will fire the triac at the proper time */
void dim_check() {
   for(int k=0;k<numBulbs;k++) {
     if(z>=dim[k]){
       if(fire[k]){
         digitalWrite(lampe[k], HIGH);    // Fire the Triac mid-phase
         fire[k]=false;
         wait[k]=0;
       }else{
         wait[k]++;
         if(wait[k]==2){
           //dim[k]=500;
           digitalWrite(lampe[k], LOW);
         }
       }
     }
   }
   z++;
}


void loop(){
  checkInput();
}


/** check for lightbulb-brightness byteArray from serial/USB port */
void checkInput(){
  if (Serial.available()>=32) {
      for(int i=0;i<32;i++){
        dim[i]=Serial.read();
      }
  }
}
