// set pin numbers:

//output 1,5,3,4
#include <math.h>
#include <TimerOne.h>
#include <TimerThree.h>
#include <EEPROM.h>

const int EEPROM_OFFSET=1024;
const int DIMENSION = 32;
const int UNDEFINED=255;



//BATMAN!!!

volatile const int lamp [] = {
                  9, 10, 8, 11, 7, 12, 6, 13,
                  22,21,24,20,26,19,28,18,
                  32,30,36,34,40,38,44,42,
                  33,31,37,35,41,39,45,43
};

/*
//KYLIE!!!
volatile const int lamp [] = {  9, 10, 8, 11, 7, 12, 6, 13,
                  24, 26, 21, 20, 28, 19, 30, 18,
                  32, 38, 36, 40, 42, 39, 41, 43,
                  45, 47, 33, 35, 37, 44, 46, 34 
                };
*/


int preset[32];
int lampPos=0;
int presetPos=0;
boolean isSetup=false;
int sequence=0;
int oldSeq=-1;
int seqFade=0;

//int phase=0;

volatile float sinus=10.0;



int selectBright=50;


volatile int dim [] =  {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};
volatile int wait [] =  {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

volatile int lust = 0;
volatile int maxlust = 100;


volatile int numBulbs=32;
volatile int bright=67;
volatile int toBright=67;

volatile int decr=35;
volatile int toDecr=35;

volatile float speedi=0.037;
volatile float toSpeed=speedi;

int heartbeat=0;
int hb=0;

volatile boolean fire[32];
volatile int z=0;

long bfire = B00000000;
float sine[515];

const int freqStep = 75;    // Set the delay for the frequency of power (65 for 60Hz, 78 for 50Hz) per step (using 128 steps)

boolean switcher=false;
boolean strobe=false;

int strobeCount=0;
int strobeFreq=2;

/** logic variables */
float count=2*PI;
int fadeSpeed=50;

int micPin = 4;


boolean isSine=false;

/******* cellular *******/
volatile int maxBrightness = 50;

const byte CELL  = B10000000;
const byte DIR_1 = B01000000;
const byte DIR_2 = B00100000;
const byte DIR_3 = B00010000;
const byte DIR_4 = B00001000;
const byte DIR_5 = B00000100;
const byte DIR_6 = B00000010;
const byte RAND  = B00000001;

byte rr=0;
byte rg=0;
byte rb=0;
byte rw=0;
byte neighbours[32][4];
byte tiles[32];
byte tilesComp[32];

const byte PHASE_COMPUTE = 0;
const byte PHASE_TRANSITION = 1;
const byte PHASE_RANDOM = 2;
const byte PHASE_WAIT = 3;

byte phase = PHASE_RANDOM;
int generation=0;
int randomTime=0;

const int generationTime = 50;
const int seedTime = 2000;
float transition = 0.0;

int tileInitNr=0;

boolean isCellular=true;

b



void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);
  
  setupSine();
  /*
  Serial.println("clear Presets");
  clearPreset();
  */
  Serial.println("init Presets");
  Serial3.println("booting 3....");
  initPreset();
  loadPreset();
  for (int i = 0;i<numBulbs; i++) {
    fire[i]=false;
    pinMode(lamp[i], OUTPUT);  
    digitalWrite(lamp[i],LOW); 
  }
  attachInterrupt(1, zero_cross_detect, RISING);   // Attach an Interupt to Pin 2 (interupt 0) for Zero Cross Detection

  Timer3.initialize(freqStep);
  Timer3.setPeriod(freqStep);
  Timer3.attachInterrupt(dim_check2);
  
  
  /***** cellular *****/
  initTiles();
  initNeighbours();
  
  randomSeed(1365363600);
  
  seed();

}


/***** cellular *****/

void initTiles(){
  for(int i=0; i<30; i++){
    tiles[i]=B00000000;
  }
  memcpy(tilesComp,tiles,sizeof(tiles));
}


void initNeighbours(){
  initTile(1,2,3,4,5,99);

  initTile(0,5,6,12,7,2);
  initTile(0,1,7,13,8,3);
  initTile(0,2,8,14,9,4);
  initTile(0,3,9,15,10,5);
  initTile(0,4,10,11,6,1);

  initTile(1,5,11,17,12,99);
  initTile(2,1,12,18,13,99);
  initTile(3,2,13,19,14,99);
  initTile(4,3,14,20,15,99);
  initTile(5,4,15,16,11,99);

  initTile(5,10,16,22,17,6);
  initTile(1,6,17,23,18,7);
  initTile(2,7,18,24,19,8);
  initTile(3,8,19,25,20,9);
  initTile(4,9,20,21,16,10);

  initTile(10,15,21,27,22,11);
  initTile(6,11,22,28,23,12);
  initTile(7,12,23,29,24,13);
  initTile(8,13,24,30,25,14);
  initTile(9,14,25,26,21,15);

  initTile(15,20,26,27,16,99);
  initTile(11,16,27,28,17,99);
  initTile(12,17,28,29,18,99);
  initTile(13,18,29,30,19,99);
  initTile(14,19,30,26,20,99);

  initTile(20,25,30,31,27,21);
  initTile(16,21,26,31,28,22);
  initTile(17,22,27,31,29,23);
  initTile(18,23,28,31,30,24);
  initTile(19,24,29,31,26,25);

  initTile(26,27,28,29,30,99);
  
}

void initTile(byte n1, byte n2, byte n3, byte n4, byte n5, byte n6){
  neighbours[tileInitNr][0]=n1;
  neighbours[tileInitNr][1]=n2;
  neighbours[tileInitNr][2]=n3;
  neighbours[tileInitNr][3]=n4;
  neighbours[tileInitNr][4]=n5;
  neighbours[tileInitNr][5]=n6;
  tileInitNr++;
}

void seed(){
  for(int ti=0; ti<32; ti++){
    tiles[ti]=random(255);
  }
  memcpy(tilesComp,tiles,sizeof(tiles));
}

void compute(){
  if(phase==PHASE_COMPUTE){
 
    for(int ti=0; ti<32; ti++){
      byte nAlive = 0;
      
      for(int ne=0; ne<6; ne++){
        byte nIndex = neighbours[ti][ne];
        if(nIndex!=99){
          byte neighbour = tiles[nIndex];
          if((neighbour&CELL)==CELL)nAlive++;
        }
      }
      
      if((tiles[ti]&CELL)==CELL){
        if(nAlive<2)tilesComp[ti]^=CELL;
        else if(nAlive>=4)tilesComp[ti]^=CELL;
      }else{
        if(nAlive==3)tilesComp[ti]^=CELL;
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

float testBrightf=60.0f;

void drawCells(){
  if(phase==PHASE_RANDOM){
    
    runStrobe();
    
    randomTime++;
    if(randomTime>50){
      count=1234.5;
      speedi=12.98776543;
      randomTime=0;
      phase=PHASE_COMPUTE;
      seed();
    }
  }else if(phase==PHASE_WAIT){
    runSine();
    /*
    for(int ti=0; ti<32; ti++){
      dim[ti]=120;
      fire[ti]=true;
    }*/
  }else{
    testBrightf+=0.1;
    int testBright = (int)testBrightf;
    
    for(int ti=0; ti<32; ti++){
      
      byte tilesCell = (tiles[ti]&CELL)==CELL;
      
      byte tilesCompCell = (tilesComp[ti]&CELL)==CELL;
      
      byte intensity = getTransition(tilesCell,tilesCompCell);
      
      //dim[ti]=intensity;
      dim[ti]=67+(maxBrightness-intensity);
      fire[ti]=true;
    }
  }
}

byte getTransition(byte t, byte tc){
  if(t+tc==2) return maxBrightness;
  else if(t-tc==1) return maxBrightness-(int)transition;
  else if(t-tc==-1) return (int)transition;
  return 0;
}









/** function to be fired at the zero crossing */
void zero_cross_detect() {
  heartbeat++;
  if(isSetup){
    runSetup();
  }else if(strobe){
    runStrobe();
  }else if(isSine){
    runSine();
  }else if(isCellular){
    compute();
    drawCells();

    if(heartbeat%seedTime==0){
      seed();
      phase=PHASE_RANDOM;
    }
  }
  z = 0;
}


/** Function will fire the triac at the proper time */
void dim_check2() {
  if(presetPos<32){
   for(int k=0;k<numBulbs;k++) {
     if(z>=dim[k]){
       if(fire[k]){
         digitalWrite(lamp[k], HIGH);    // Fire the Triac mid-phase
         fire[k]=false;
         wait[k]=0;
       }else{
         wait[k]++;
         if(wait[k]==2){
           dim[k]=500;
           digitalWrite(lamp[k], LOW);
         }
       }
     }
   }
  }else{
   for(int k=0;k<numBulbs;k++) {
     if(z>=dim[k]){
       if(fire[k]){
         digitalWrite(preset[k], HIGH);    // Fire the Triac mid-phase
         fire[k]=false;
         wait[k]=0;
       }else{
         wait[k]++;
         if(wait[k]==2){
           dim[k]=500;
           digitalWrite(preset[k], LOW);
         }
       }
     }
   }
  }
  
  z++;
}


void loop(){
  checkInput();
}





/***********************************************/
/*******           strobe                *******/
/***********************************************/

void runStrobe(){
  if(switcher){
    blitz(switcher);
    switcher=false;
  }else{
    strobeCount++;
    if(strobeCount%strobeFreq==0){
      blitz(switcher);
      switcher=true;
    }
  }
}

void blitz(boolean blitzer){
  if(blitzer){
    for (int i=0;i<numBulbs; i++) {
      fire[i]=true;
      dim[i]=bright;
    }
  }else{
    for (int i=0;i<numBulbs; i++) {
      fire[i]=true;
      dim[i]=67;
    }
  }
}






/***********************************************/
/*******             sine                *******/
/***********************************************/

void runSine(){
  /*
  if(lust==0){
    toBright=20;
    speedi+=0.000007731;
    maxlust=150;
  }else if(lust==1){
    toBright=50;
    speedi+=0.0007731;
    maxlust=1000;
  }else if(lust==2){
    toBright=70;
    speedi+=0.007731;
    maxlust=1000000;
  }else if(lust==3){
    toBright=77;
    speedi+=0.07731;
    maxlust=10000000;
  }else{
    toBright=67;
    speedi+=0.00003731;
    maxlust=10000;
  }*/
  
    
  toBright=100;
  speedi+=0.007731;
  count+=speedi;
  
  /*
  if(count>maxlust){
    lust++;
    count=0;
    speedi=0;
    if(lust>5)lust=0;
  }*/
  
  for (int i=0;i<numBulbs; i++) {
    sinus = sinus0to1( count+( ((float)(i)/32.0)*2*PI ));
    sinus*=sinus*sinus;
    sinus*=(decr);

    dim[i]=(bright-sinus);
    fire[i]=true;
  }
}

/** initialize sine lookup table (sine LUT) */
void setupSine(){
  for(int n=0;n<514;n++){
    sine[n] = (sin(2.0*PI*((float)n/512.0))+1.0) / 2.0;
  }
}

/** get sine from lookuptable between 0 and 2*PI */
float sinus0to1(float val){
  float si = fmod(val,2*PI);
  return sine[round(si*(512.0/(2.0*PI)))];
}





/***********************************************/
/*******           presets               *******/
/***********************************************/


void initPreset(){
  for(int i=0;i<DIMENSION;i++){
    preset[i]=UNDEFINED;
  }
}

void savePreset(){
  for(int i=0;i<DIMENSION;i++){
    int address = EEPROM_OFFSET+i;
    EEPROM.write(address,preset[i]);
  }
}

void loadPreset(){
  Serial.println("loading Preset");
  for(int i=0;i<DIMENSION;i++){
    int address = EEPROM_OFFSET+i;
    preset[i]=EEPROM.read(address);
    if(preset[i]!=UNDEFINED)presetPos++;
    Serial.print(preset[i]);
    Serial.print(", ");
  }
  if(presetPos==32){
    Serial.println("");
    Serial.println("PRESET COMPLETE");
  }
}

void clearPreset(){
  for(int i=0;i<DIMENSION;i++){
    int address = EEPROM_OFFSET+i;
    EEPROM.write(address,UNDEFINED);
  }
}


/***********************************************/
/*******            setup                *******/
/***********************************************/

void runSetup(){
  for (int i=0;i<numBulbs; i++) {
    boolean found=false;
    for(int j=0; j<presetPos; j++){
      if(lamp[i]==preset[j]){
        dim[i]=98;
        //if(dim[i]<90)dim[i]+=5;
        //if(dim[i]>90)dim[i]-=4;
        found=true;
      }
      if(found)break;
    }
    
    if(!found){
      dim[i]=110;
    }
    
  }
  
  dim[lampPos]=selectBright;
  selectBright++;
  if(selectBright>110)selectBright=60;
  
  //animate tail
  if(presetPos>5){
    int seq = sequence/12;
    sequence++;
    seqFade++;
    if(seq!=oldSeq){
      seqFade=0;
      oldSeq=seq;
    }
    
    if(seq<presetPos){
      //if(sequence%10==0)
      if(seq==0||seq==presetPos-1){
        dim[getLampPos(preset[seq])]=75+seqFade*2;
      }else{
        dim[getLampPos(preset[seq])]=88+seqFade;
      }
    }else{
      sequence=0;
    }
  }
  
  for (int i=0;i<numBulbs; i++) {
    fire[i]=true;
  }
}

void apfelZ(){
  presetPos--;
  preset[presetPos]=UNDEFINED;
  Serial.print("presetPos ");
  Serial.println(presetPos);
}
void fixPos(){
  preset[presetPos]=lamp[lampPos];
  presetPos++;
  Serial.print("presetPos ");
  Serial.println(presetPos);
}

void nextPosEasy(){
  lampPos++;
  if(lampPos>31)lampPos=31;
  Serial.print("lampPos ");
  Serial.println(lampPos);
}
void prevPosEasy(){
  lampPos--;
  if(lampPos<0)lampPos=0;
  Serial.print("lampPos ");
  Serial.println(lampPos);
}


void nextPos(int depth){
  lampPos++;
  if(lampPos>=DIMENSION)lampPos=0;
  boolean found=false;
  for(int i=0;i<presetPos;i++){
    if(lamp[lampPos]==preset[i]){
      found=true;
    }
  }
  if(depth<=-32){
    Serial.println("--ALL LAMPS SET--");
    Serial.print("presetPos should be 31: ");
    Serial.println(presetPos);
  }else if(found)nextPos(depth-1);
  
  
  Serial.print("lampPos ");
  Serial.println(lampPos);
  
}

void prevPos(int depth){
  lampPos--;
  if(lampPos<=0)lampPos=DIMENSION-1;
  boolean found=false;
  for(int i=0;i<presetPos;i++){
    if(lamp[lampPos]==preset[i]){
      found=true;
    }
  }
  if(depth<=-32){
    Serial.println("--ALL LAMPS SET--");
    Serial.print("presetPos should be 31: ");
    Serial.println(presetPos);
  }else if(found)prevPos(depth-1);
  
  
  Serial.print("lampPos ");
  Serial.println(lampPos);
  
}


int getLampPos(int lampNr){
  for(int i=0;i<DIMENSION;i++){
    if(lamp[i]==lampNr)return i;
  }
}




/***********************************************/
/*******           serial                *******/
/***********************************************/

/** check for control messages from serial/USB port */
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





