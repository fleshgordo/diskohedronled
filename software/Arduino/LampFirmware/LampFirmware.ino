const int ShiftPWM_latchPin = 8;
const bool ShiftPWM_invertOutputs = false;
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>  // include ShiftPWM.h after setting the pins!

unsigned char maxBrightness = 165;
unsigned char pwmFrequency = 60;

int numRGBleds = 30;

int bitsPerBoard = 4;
int registerSize = 8;

int numRegisters = (numRGBleds + 1) * bitsPerBoard / registerSize;

int heartbeat = 0;

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


const byte RECEIVING_N = 0;
const byte RECEIVING_R = 1;
const byte RECEIVING_G = 2;
const byte RECEIVING_B = 3;
const byte RECEIVING_W = 4;
byte receiving = RECEIVING_N;

byte rr = 0;
byte rg = 0;
byte rb = 0;
byte rw = 0;

byte neighbours[30][4];
byte tiles[30];
byte tilesComp[30];

const int MODE_OFF = 0;
const int MODE_BWFLICKER = 1;
const int MODE_COLORNOISE = 2;
const int MODE_GOL = 3;
const int MODE_BOOT = 4;
const int MODE_STREAMING = 99;
const int MODE_233 = 233;
int mode = MODE_BOOT;
int noSerial = 0;

byte param1 = 2;
byte param2 = 2;
byte param3 = 2;
byte param4 = 2;
byte param5 = 2;

byte bwintensity = 2;
byte noiseBrightnessR = maxBrightness;
byte noiseBrightnessG = maxBrightness;
byte noiseBrightnessB = maxBrightness;
byte noiseBrightnessW = maxBrightness;

const byte SERIAL_MODE = '0';
const byte SERIAL_PARAM1 = '1';
const byte SERIAL_PARAM2 = '2';
const byte SERIAL_PARAM3 = '3';
const byte SERIAL_PARAM4 = '4';
const byte SERIAL_PARAM5 = '5';
const byte SERIAL_GLOBAL1 = '101';
const byte SERIAL_GLOBAL2 = '102';
const byte SERIAL_GLOBAL3 = '103';
const byte SERIAL_GLOBAL4 = '104';
const byte SERIAL_GLOBAL5 = '105';

const byte PHASE_COMPUTE = 0;
const byte PHASE_TRANSITION = 1;
const byte PHASE_RANDOM = 2;
const byte PHASE_WAIT = 3;
const byte PHASE_STREAMING = 4;
const byte PHASE_OFF = 5;

byte phase = PHASE_COMPUTE;
int generation = 0;
int randomTime = 0;

int generationTime = 590;
int seedTime = 5000;
float transition = 0.0;

int aliveR = 2;
int aliveG = 3;
int aliveB = 3;
int aliveW = 3;

void setup() {
  Serial.begin(9600);

  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.SetAmountOfRegisters(numRegisters);

  ShiftPWM.Start(pwmFrequency, maxBrightness);
  ShiftPWM.SetAll(0);

  initTiles();
  initNeighbours();

  randomSeed(1365363600);  //viki birthdate

  seed();
  draw();
}

void loop() {
  heartbeat++;
  compute();
  draw();
}

int tileInitNr = 0;

void initTiles() {
  for (int i = 0; i < 30; i++) {
    tiles[i] = B00000000;
    //tiles[i]|=DIR_1;
  }
  //tiles[0]|=LED_R;
  memcpy(tilesComp, tiles, sizeof(tiles));
}


void initNeighbours() {
  initTile(2, 3, 9, 10);
  initTile(3, 1, 14, 15);
  initTile(4, 5, 1, 2);
  initTile(5, 3, 17, 18);
  initTile(6, 7, 3, 4);
  initTile(7, 5, 20, 21);
  initTile(8, 9, 5, 6);
  initTile(9, 7, 23, 24);
  initTile(10, 1, 7, 8);
  initTile(11, 12, 1, 9);
  initTile(12, 10, 24, 25);
  initTile(13, 14, 10, 11);
  initTile(14, 12, 27, 28);
  initTile(15, 2, 12, 13);
  initTile(16, 17, 2, 14);
  initTile(17, 15, 28, 29);
  initTile(18, 5, 15, 16);
  initTile(19, 20, 4, 17);
  initTile(20, 18, 29, 30);
  initTile(21, 6, 18, 19);
  initTile(22, 23, 6, 20);
  initTile(23, 21, 30, 26);
  initTile(24, 8, 21, 22);
  initTile(25, 11, 8, 23);
  initTile(26, 27, 11, 24);
  initTile(27, 25, 22, 30);
  initTile(28, 13, 25, 26);
  initTile(29, 16, 13, 27);
  initTile(30, 19, 16, 28);
  initTile(26, 22, 19, 29);
}

void initTile(byte n1, byte n2, byte n3, byte n4) {
  neighbours[tileInitNr][N1] = n1 - 1;
  neighbours[tileInitNr][N2] = n2 - 1;
  neighbours[tileInitNr][N3] = n3 - 1;
  neighbours[tileInitNr][N4] = n4 - 1;
  tileInitNr++;
}


void seed() {
  for (int ti = 0; ti < 30; ti++) {
    tiles[ti] = random(125);
    ShiftPWM.SetRGBWsimple(ti, maxBrightness, maxBrightness, maxBrightness, maxBrightness);
  }
  memcpy(tilesComp, tiles, sizeof(tiles));
}


boolean allDeadR() {
  for (int ti = 0; ti < 30; ti++) {
    if (tiles[ti] & LED_R == LED_R) return false;
  }
  return true;
}


void compute() {
  if (phase == PHASE_COMPUTE) {

    for (int ti = 0; ti < 30; ti++) {
      byte nnr = 0;
      byte nng = 0;
      byte nnb = 0;
      byte nnw = 0;

      for (int ne = 0; ne < 4; ne++) {
        byte neighbour = tiles[neighbours[ti][ne]];
        if ((neighbour & LED_R) == LED_R) nnr++;
        if ((neighbour & LED_G) == LED_G) nng++;
        if ((neighbour & LED_B) == LED_B) nnb++;
        if ((neighbour & LED_W) == LED_W) nnw++;
      }

      if ((tiles[ti] & DIR_1) == DIR_1)
        if ((tiles[ti] & LED_R) == LED_R) {
          nng++;
          nnb++;
          nnw++;
        }
      if ((tiles[ti] & DIR_2) == DIR_2)
        if ((tiles[ti] & LED_G) == LED_G) {
          nnr++;
          nnb++;
          nnw++;
        }
      if ((tiles[ti] & DIR_3) == DIR_3)
        if ((tiles[ti] & LED_B) == LED_B) {
          nnr++;
          nng++;
          nnw++;
        }
      if ((tiles[ti] & DIR_4) == DIR_4)
        if ((tiles[ti] & LED_W) == LED_W) {
          nnr++;
          nng++;
          nnb++;
        }



      if ((tiles[ti] & LED_R) == LED_R) {
        if (nnr < aliveR - 1) tilesComp[ti] ^= LED_R;
        else if (nnr >= aliveR + 1) tilesComp[ti] ^= LED_R;
      } else {
        if (nnr == aliveR) tilesComp[ti] ^= LED_R;
      }

      if ((tiles[ti] & LED_G) == LED_G) {
        if (nng < aliveG - 1) tilesComp[ti] ^= LED_G;
        else if (nng >= aliveG + 1) tilesComp[ti] ^= LED_G;
      } else {
        if (nng == aliveG) tilesComp[ti] ^= LED_G;
      }

      if ((tiles[ti] & LED_B) == LED_B) {
        if (nnb < aliveB - 1) tilesComp[ti] ^= LED_B;
        else if (nnb >= aliveB + 1) tilesComp[ti] ^= LED_B;
      } else {
        if (nnb == aliveB) tilesComp[ti] ^= LED_B;
      }

      if ((tiles[ti] & LED_W) == LED_W) {
        if (nnw < aliveW - 1) tilesComp[ti] ^= LED_W;
        else if (nnw >= aliveW + 1) tilesComp[ti] ^= LED_W;
      } else {
        if (nnw == aliveW) tilesComp[ti] ^= LED_W;
      }
    }

    phase = PHASE_TRANSITION;
    transition = 0.0;

  } else if (phase == PHASE_TRANSITION) {
    float transfactor = (float)maxBrightness / (float)generationTime;
    transition += transfactor;

    if (transition >= (float)maxBrightness) {
      transition = maxBrightness;
      phase = PHASE_COMPUTE;
      memcpy(tiles, tilesComp, sizeof(tilesComp));
    }
  }
}


void draw() {
  checkInput();
  if (mode == MODE_233) {
    for (int ti = 0; ti < 30; ti++) {
      rr = random(noiseBrightnessR);
      rg = random(noiseBrightnessG);
      rb = random(noiseBrightnessB);
      rw = random(noiseBrightnessW);
      ShiftPWM.SetRGBWsimple(ti, rr, 0, 0, 0);
    }
  } else if (mode == MODE_COLORNOISE) {
    for (int ti = 0; ti < 30; ti++) {
      rr = random(noiseBrightnessR);
      rg = random(noiseBrightnessG);
      rb = random(noiseBrightnessB);
      rw = random(noiseBrightnessW);
      ShiftPWM.SetRGBWsimple(ti, rr, rg, rb, rw);
    }
  } else if (mode == MODE_BWFLICKER) {
    for (int ti = 0; ti < 30; ti++) {
      //rr = random(bwintensity);
      //rg = random(bwintensity);
      //rb = random(bwintensity);
      rw = random(bwintensity);
      ShiftPWM.SetRGBWsimple(ti, 0, 0, 0, rw);
    }
  } else if (mode == MODE_GOL) {
    if (heartbeat % seedTime == 0) {
      int aliveR2 = aliveR;
      aliveR = aliveG;
      aliveG = aliveB;
      aliveB = aliveW;
      aliveW = aliveR;

      seed();
      //phase=PHASE_RANDOM;
    }

    for (int ti = 0; ti < 30; ti++) {
      byte tilesR = (tiles[ti] & LED_R) == LED_R;
      byte tilesG = (tiles[ti] & LED_G) == LED_G;
      byte tilesB = (tiles[ti] & LED_B) == LED_B;
      byte tilesW = (tiles[ti] & LED_W) == LED_W;

      byte tilesCompR = (tilesComp[ti] & LED_R) == LED_R;
      byte tilesCompG = (tilesComp[ti] & LED_G) == LED_G;
      byte tilesCompB = (tilesComp[ti] & LED_B) == LED_B;
      byte tilesCompW = (tilesComp[ti] & LED_W) == LED_W;

      byte r = getTransition(tilesR, tilesCompR);
      byte g = getTransition(tilesG, tilesCompG);
      byte b = getTransition(tilesB, tilesCompB);
      byte w = getTransition(tilesW, tilesCompW);

      ShiftPWM.SetRGBWsimple(ti, r, g, b, w);
      //ShiftPWM.SetRGBWsimple(ti,r,0,0,0);
    }
  } else if (mode == MODE_OFF) {
    for (int ti = 0; ti < 30; ti++) {
      ShiftPWM.SetRGBWsimple(ti, 0, 0, 0, 0);
    }
  } else if (mode == MODE_BOOT) {
    bootLeds();
  }
}


byte getTransition(byte t, byte tc) {
  if (t + tc == 2) return maxBrightness;
  else if (t - tc == 1) return maxBrightness - (int)transition;
  else if (t - tc == -1) return (int)transition;
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
/* SERIAL INPUT                                */
/***********************************************/
const int STATE_UNKNOWN = -1;
const int STATE_MODE = 0;
const int STATE_PARAM = 1;
int state = STATE_UNKNOWN;

int paramnum = 0;
int field = 0;
boolean readNumber = false;
int number = 0;

int bootbright = maxBrightness;
float bootspeed = 0.1663195;


void checkInput() {
  //if(Serial.available()>0){
  while (Serial.available() > 0) {
    byte val = Serial.read();

    if (val == 'm') {
      state = STATE_MODE;
      readNumber = false;  //really needed?
    } else if (val == 'p') {
      field = 0;
      state = STATE_PARAM;
      readNumber = false;  //really needed?
      Serial.println("received p");
    }

    if (val == ',') {
      parseParam();
      readNumber = false;
    }

    if ((val >= 48) && (val <= 57)) {
      if (!readNumber) number = 0;
      readNumber = true;
      number = (number * 10) + (val - '0');
    }

    if (val == ';') {
      if (state == STATE_MODE) {
        if (!readNumber) {
          Serial.println("expected mode, no number read!");
        } else {
          mode = number;
          Serial.print("set mode to ");
          Serial.println(number);
          state = STATE_UNKNOWN;
        }
      } else if (state == STATE_PARAM) {
        parseParam();
      }
    }
  }
}

void parseParam() {
  if (readNumber) {
    if (state = STATE_PARAM) {
      if (mode == MODE_GOL) {
        if (field == 0) seedTime = number * 50;
        if (field == 1) generationTime = number * 5;
      } else if (mode == MODE_BWFLICKER) {
        if (field == 0) bwintensity = number;
      } else if (mode == MODE_COLORNOISE) {
        if (field == 0) noiseBrightnessR = min(number, maxBrightness);
        if (field == 1) noiseBrightnessG = min(number, maxBrightness);
        if (field == 2) noiseBrightnessB = min(number, maxBrightness);
        if (field == 3) noiseBrightnessW = min(number, maxBrightness);
      } else if (mode == MODE_BOOT) {
        if (field == 0) bootbright = number;
        if (field == 1) bootspeed = 21.0 / (float)(number + 1);
      }
      Serial.print("set param to ");
      Serial.println(number);
      field++;
    }
  }
}




/*
void checkInput(){
  if(mode==MODE_STREAMING){
    noSerial++;

    if(noSerial>=100){
      mode=MODE_BWFLICKER;
      noSerial=0;
      Serial.flush();
    }
    
    if (Serial.available()>=32) {
      noSerial=0;
      byte channel = Serial.read();
      for(int ti=0;ti<30;ti++){
        byte streamcolor = Serial.read();
        byte r,g,b,w;
        r=0;g=0;b=0;w=0;
        if(channel==0){
          r=streamcolor;
        }else if(channel==1){
          g=streamcolor;
        }else if(channel==2){
          b=streamcolor;
        }else if(channel==3){
          w=streamcolor;
        }
        ShiftPWM.SetRGBWsimple(ti,r,g,b,w);
      }
      byte endStreaming = Serial.read();
      if(endStreaming!='0'){
        mode=MODE_OFF;
      }
    }
  }else{
    if(Serial.available()>=2){
      byte serialId = Serial.read();
      byte serialValue = Serial.read();
      
      if(serialId==SERIAL_MODE){
        mode=serialValue;
      }
      else if(serialId==SERIAL_PARAM1){
        if(mode==MODE_GOL)seedTime=serialValue*50;
        else if(mode==MODE_BWFLICKER)bwintensity=serialValue;
        else if(mode==MODE_COLORNOISE)noiseBrightnessR=max(serialValue,maxBrightness);
        param1=serialValue;
      }else if(serialId==SERIAL_PARAM2){
        if(mode==MODE_GOL)generationTime=serialValue*5;
        else if(mode==MODE_BWFLICKER)bwintensity=serialValue;
        else if(mode==MODE_COLORNOISE)noiseBrightnessG=max(serialValue,maxBrightness);
        param2=serialValue;
      }else if(serialId==SERIAL_PARAM3){
        if(mode==MODE_COLORNOISE)noiseBrightnessB=max(serialValue,maxBrightness);
        //if(mode==MODE_GOL)aliveR=1;
        param3=serialValue;
      }else if(serialId==SERIAL_PARAM4){
        if(mode==MODE_COLORNOISE)noiseBrightnessW=max(serialValue,maxBrightness);
        param4=serialValue;
      }else if(serialId==SERIAL_PARAM5){
        param5=serialValue;
      }else{
        //Serial.flush();
      }
    }
  }
}

*/



/******** additional modes ********/


float tr = 0.0;
float tg = 0.0;
float tb = 0.0;
float tw = 0.0;

int bl1 = 0;
int bl2 = 0;
int bl3 = 0;
int bl4 = 0;
int bl5 = 0;
int bl6 = 0;

int currentLED = 0;


int ledrows36[] = {
  1, 2, 3, 1, 2, 3,
  4, 5, 9, 10, 14, 15,
  7, 12, 17, 7, 12, 17,
  6, 8, 11, 13, 16, 18,
  20, 24, 28, 20, 24, 28,
  19, 21, 23, 25, 27, 29,
  22, 26, 30, 22, 26, 30
};
float bootY = 0.0;
int bootC = 0;
float speedy = 0.0;


void bootLeds() {
  bootY += 0.1669314;  //+(bootC/50.0);
  if (bootY < -2) bootY = 9.0;
  if (bootY > 9.0) {
    bootY = -2.0;
    bootC++;
    if (bootC > 11) bootC = 0;
  }
  int by = (int)bootY;
  float bright = 0.0;
  int br = 0;
  int dr = 0;
  for (int i = 0; i < 42; i++) {
    float brbr = (float)min(bootbright, maxBrightness);
    if (i % 6 == 0) {
      bright = abs((bootY - ((i / 6) + 0.5)) * 0.27);
      if (bright <= 1.0) {
        bright = abs(bright - 1.0);
        bright *= bright;
        bright = abs(bright - 1.0);
        br = brbr - (int)(bright * brbr);
      } else {
        br = 0;
      }
    }
    if (br > brbr) br = brbr;
    if (br < 0) br = 0;
    dr = brbr - br;
    setLedTest(i, br, dr);
  }
}
void setLedTest(int lnr, int bbr, int ddr) {
  int c1 = 0;
  int c2 = 0;
  int c3 = 0;
  int c4 = 0;
  if ((bootC % 4) == 0) c1 = bbr;
  if ((bootC % 4) == 1) c2 = bbr;
  if ((bootC % 4) == 2) c3 = bbr;
  if ((bootC % 4) == 3) c4 = bbr;
  ShiftPWM.SetRGBWsimple(ledrows36[lnr] - 1, c1, c2, c3, c4);
}
