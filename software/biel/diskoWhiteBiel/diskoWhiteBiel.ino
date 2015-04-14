/***********************************************/
/* TEMPERATURE SENSOR                        */
/***********************************************/
#include <DHT.h>

#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

int sTimer=0;
int switchOffTemperature=70;
DHT dht(DHTPIN, DHTTYPE);
/***********************************************/
/* END                                         */
/***********************************************/


const int ShiftPWM_latchPin=8;
const bool ShiftPWM_invertOutputs = false; 
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>   // include ShiftPWM.h after setting the pins!

unsigned char maxBrightness = 165;
unsigned char pwmFrequency = 60;


int numRGBleds = 30;

int bitsPerBoard = 4;
int registerSize = 8;

int numRegisters = (numRGBleds+1)*bitsPerBoard/registerSize;


int heartbeat=0;


const int N1 = 0;
const int N2 = 1;
const int N3 = 2;
const int N4 = 3;
const int TR = 0;
const int TG = 1;
const int TB = 2;
const int TW = 3;


const byte LED_R = B10000000;
const byte LED_G = B01000000;
const byte LED_B = B00100000;
const byte LED_W = B00010000;
const byte DIR_1 = B00001000;
const byte DIR_2 = B00000100;
const byte DIR_3 = B00000010;
const byte DIR_4 = B00000001;

byte rr=0;
byte rg=0;
byte rb=0;
byte rw=0;


byte neighbours[30][4];

byte tiles[30];
byte tilesComp[30];


const byte PHASE_COMPUTE = 0;
const byte PHASE_TRANSITION = 1;
const byte PHASE_RANDOM = 2;
const byte PHASE_WAIT = 3;

byte phase = PHASE_RANDOM;
int generation=0;
int randomTime=0;

const int generationTime = 590;
const int seedTime = 22000;
float transition = 0.0;


void setup() {
  Serial.begin(9600);
  
  dht.begin();
  
  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  
  
  
  
  ShiftPWM.Start(pwmFrequency,maxBrightness);
  ShiftPWM.SetAll(0);
  
  initTiles();
  initNeighbours();
  
  randomSeed(1365363600);
  
  seed();
  draw();
    
}



void loop() {
  heartbeat++;
  
  compute();
  draw();
  
  //if(phase!=PHASE_WAIT){
    if(heartbeat%seedTime==0){
      //temperature_check();
      seed();
      phase=PHASE_RANDOM;
    }
  //}
}


int tileInitNr=0;

void initTiles(){
  for(int i=0; i<30; i++){
    tiles[i]=B00000000;
    //tiles[i]|=DIR_1;
  }
  //tiles[0]|=LED_R;
  memcpy(tilesComp,tiles,sizeof(tiles));
}


void initNeighbours(){
  initTile(2,3,9,10);
  initTile(3,1,14,15);
  initTile(4,5,1,2);
  initTile(5,3,17,18);
  initTile(6,7,3,4);
  initTile(7,5,20,21);
  initTile(8,9,5,6);
  initTile(9,7,23,24);
  initTile(10,1,7,8);
  initTile(11,12,1,9);
  initTile(12,10,24,25);
  initTile(13,14,10,11);
  initTile(14,12,27,28);
  initTile(15,2,12,13);
  initTile(16,17,2,14);
  initTile(17,15,28,29);
  initTile(18,5,15,16);
  initTile(19,20,4,17);
  initTile(20,18,29,30);
  initTile(21,6,18,19);
  initTile(22,23,6,20);
  initTile(23,21,30,26);
  initTile(24,8,21,22);
  initTile(25,11,8,23);
  initTile(26,27,11,24);
  initTile(27,25,22,30);
  initTile(28,13,25,26);
  initTile(29,16,13,27);
  initTile(30,19,16,28);
  initTile(26,22,19,29);
}

void initTile(byte n1, byte n2, byte n3, byte n4){
  neighbours[tileInitNr][N1]=n1-1;
  neighbours[tileInitNr][N2]=n2-1;
  neighbours[tileInitNr][N3]=n3-1;
  neighbours[tileInitNr][N4]=n4-1;
  tileInitNr++;
}


void seed(){
  for(int ti=0; ti<30; ti++){
    tiles[ti]=random(255);
    ShiftPWM.SetRGBWsimple(ti,maxBrightness,maxBrightness,maxBrightness,maxBrightness);
  }
  memcpy(tilesComp,tiles,sizeof(tiles));
}


boolean allDeadR(){
  for(int ti=0; ti<30; ti++){
    if(tiles[ti]&LED_R==LED_R)return false;
  }
  return true;
}


void compute(){
  if(phase==PHASE_COMPUTE){
 
    for(int ti=0; ti<30; ti++){
      byte nnr = 0;
      byte nng = 0;
      byte nnb = 0;
      byte nnw = 0;
      
      for(int ne=0; ne<4; ne++){        
        byte neighbour = tiles[neighbours[ti][ne]];
        if((neighbour&LED_R)==LED_R)nnr++;
        if((neighbour&LED_G)==LED_G)nng++;
        if((neighbour&LED_B)==LED_B)nnb++;
        if((neighbour&LED_W)==LED_W)nnw++;
      }
      
      if((tiles[ti]&DIR_1)==DIR_1)
        if((tiles[ti]&LED_R)==LED_R){  nng++;nnb++;nnw++; }
      if((tiles[ti]&DIR_2)==DIR_2)
        if((tiles[ti]&LED_G)==LED_G){  nnr++;nnb++;nnw++; }
      if((tiles[ti]&DIR_3)==DIR_3)
        if((tiles[ti]&LED_B)==LED_B){  nnr++;nng++;nnw++; }
      if((tiles[ti]&DIR_4)==DIR_4)
        if((tiles[ti]&LED_W)==LED_W){  nnr++;nng++;nnb++; }
      
      if((tiles[ti]&LED_R)==LED_R){
        if(nnr<2)tilesComp[ti]^=LED_R;
        else if(nnr>=4)tilesComp[ti]^=LED_R;
      }else{
        if(nnr==3)tilesComp[ti]^=LED_R;
      }
      
      if((tiles[ti]&LED_G)==LED_G){
        if(nng<2)tilesComp[ti]^=LED_G;
        else if(nng>=4)tilesComp[ti]^=LED_G;
      }else{
        if(nng==3)tilesComp[ti]^=LED_G;
      }
      
      if((tiles[ti]&LED_B)==LED_B){
        if(nnb<2)tilesComp[ti]^=LED_B;
        else if(nnb>=4)tilesComp[ti]^=LED_B;
      }else{
        if(nnb==3)tilesComp[ti]^=LED_B;
      }
      
      if((tiles[ti]&LED_W)==LED_W){
        if(nnw<2)tilesComp[ti]^=LED_W;
        else if(nnw>=4)tilesComp[ti]^=LED_W;
      }else{
        if(nnw==3)tilesComp[ti]^=LED_W;
      }
    }
    
    phase=PHASE_TRANSITION;
    transition=0.0;
    
  }else if(phase==PHASE_TRANSITION){
    float transfactor=(float)maxBrightness/(float)generationTime;
    transition+=transfactor;
    
    if(transition>=(float)maxBrightness){
      transition=maxBrightness;
      phase=PHASE_COMPUTE;
      memcpy(tiles,tilesComp,sizeof(tilesComp));
    }

  }
}


void draw(){
  checkInput();
  if(phase==PHASE_RANDOM){
    randomTime++;
    if(randomTime>20){
      randomTime=0;
      seed();
      phase=PHASE_COMPUTE;
    }
    for(int ti=0;ti<30;ti++){
      rr = random(maxBrightness);
      rg = random(maxBrightness);
      rb = random(maxBrightness);
      rw = random(maxBrightness);
      ShiftPWM.SetRGBWsimple(ti,rr,rg,rb,0);
    }
  }else if(phase==PHASE_WAIT){
    for(int ti=0;ti<30;ti++){
      //rr = random(2);
      //rg = random(2);
      //rb = random(2);
      rw = random(2);
      ShiftPWM.SetRGBWsimple(ti,0,0,0,rw);
    }
  }else{
    for(int ti=0; ti<30; ti++){
      byte tilesR = (tiles[ti]&LED_R)==LED_R;
      byte tilesG = (tiles[ti]&LED_G)==LED_G;
      byte tilesB = (tiles[ti]&LED_B)==LED_B;
      byte tilesW = (tiles[ti]&LED_W)==LED_W;
      
      byte tilesCompR = (tilesComp[ti]&LED_R)==LED_R;
      byte tilesCompG = (tilesComp[ti]&LED_G)==LED_G;
      byte tilesCompB = (tilesComp[ti]&LED_B)==LED_B;
      byte tilesCompW = (tilesComp[ti]&LED_W)==LED_W;
      
      byte r = getTransition(tilesR,tilesCompR);
      byte g = getTransition(tilesG,tilesCompG);
      byte b = getTransition(tilesB,tilesCompB);
      byte w = getTransition(tilesW,tilesCompW);
      
      ShiftPWM.SetRGBWsimple(ti,r,g,b,w);
      //ShiftPWM.SetRGBWsimple(ti,r,0,0,0);
    }
  }
}


byte getTransition(byte t, byte tc){
  if(t+tc==2) return maxBrightness;
  else if(t-tc==1) return maxBrightness-(int)transition;
  else if(t-tc==-1) return (int)transition;
  return 0;
}




/***********************************************/
static uint8_t y8 = 1;
static uint16_t y16 = 1;
 
// returns values from 1 to 255 inclusive, period is 255
uint8_t xorshift8(void) {
    y8 ^= (y8 << 7);
    y8 ^= (y8 >> 5);
    return y8 ^= (y8 << 3);
}
 
// returns values from 1 to 65535 inclusive, period is 65535
uint16_t xorshift16(void) {
    y16 ^= (y16 << 13);
    y16 ^= (y16 >> 9);
    return y16 ^= (y16 << 7);
}





/***********************************************/
/* TEMPERATURE SENSOR                          */
/***********************************************/

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



/***********************************************/
/* SERIAL INPUT                                */
/***********************************************/

void checkInput(){
  int serialInput=0;
  if (Serial.available()>=1) {
    
    serialInput = Serial.read();
    
    if(serialInput=='0'){
      phase=PHASE_WAIT;
      heartbeat=1;
    }
    if(serialInput=='1'){
      phase=PHASE_RANDOM;
      heartbeat=1;
      randomTime=0;
    }

  }
}
