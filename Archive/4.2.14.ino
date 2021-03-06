

#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <SoftwareSerial.h>
#include <SdFatUtil.h>
#include <SdFat.h>

#include <MIDIFile.h>
#include <SPI.h>

/*MIDI MAPPER / MIDI FILE PLAYER / MASTER CLOCK*
 
 **********SD Card is at FULL SPEED FOR TESTING!!!!!!!!!!!!**************************
 Need to MAP THE PINS THAT ARE USED
 Need to re-test MIDI stop/start, etc with newer SD interface (TFT)
 - will play by 'hearing' a start command.
 
 TFT Shield:
 -----------------------------------
 The shield uses the following pins:
 +5V
 GND
 Digital pin 4 (SD chip select)
 Digital pin 8 (TFT data/command)*  ---issue w/MIDI shield (2/10/14: changed MIDI to 2,3 to attempt workaround).
 Digital pin 10 (TFT chip select)   ---issue w/SD shield (2/14/14: removed ITEAD SD shield, used built in SD on TFT shield).
 Digital pins 11-13 for SPI communication (MOSI, MISO, SCK respectively)
 Digital pins 50-52 for SPI on Arduino Mega boards (all types)
 Analog pin 3 (joystick input)
 
 
 MIDI Shield
 ----------------
 D8, D9
 A0, A1
 
 
 ITEAD SD shield  (deprecated)
 --------------------
 D13 / SCK
 D12 / MISO
 D11 / MOSI ~
 D10 / SS ~
 
 
 
 
 TO DO:
 
TFT LCD
1.  Files are printed to screen.  Need to figure out a menu system 
 
 CLOCK
 2.  TO RESOLVE ISSUE 1, FIND A WAY TO TRANSLATE THE KORG ANALOG SYNC (5V PULSE) TO ARDUINO MASTER CLOCK. 
 -  idea:  run line into Analog input and use it as a digital input.  Use digitalread to see when it's 5 volts.  
 
 3.  need to change the MIDI TX/RX to 8,9 to support the LCD screen
 
 4.  Loop MIDI files?
 
 
 KORG ES1
 
 -NEED TO TEST NEW CODE ON THAT
 
 
 ISSUES:
 1.  A PLAY COMMAND STARTS THE KORG SYNTH PLAYING A SEQUENCE IN ITS MEMORY.  
 -  CHANGING THE CLOCK TO INTERNAL FIXES THAT, BUT THEN IT WON'T THEN FOLLOW THE MASTER CLOCK SENT BY THE ARDUINO.  
 
 
 */

/// Edit for Shield Pinouts!
#define SD_CS    4  // Chip select line for SD card
#define TFT_CS  10  // Chip select line for TFT display
#define TFT_DC   8  // Data/command line for TFT
#define TFT_RST  0  // Reset line for TFT (or connect to +5V)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define Neutral 0
#define Press 1
#define Up 2
#define Down 3
#define Right 4
#define Left 5

// Check the joystick position
int CheckJoystick()
{
  int joystickState = analogRead(3);

  if (joystickState < 50) return Left;
  if (joystickState < 150) return Down;
  if (joystickState < 250) return Press;
  if (joystickState < 500) return Right;
  if (joystickState < 650) return Up;
  return Neutral;
} 


byte	NoteOff	              = 0x80;	///< Note Off
byte	NoteOn                = 0x90;	///< Note On
byte	AfterTouchPoly        = 0xA0;	///< Polyphonic AfterTouch
byte	ControlChange         = 0xB0;	///< Control Change / Channel Mode
byte	ProgramChange         = 0xC0;	///< Program Change
byte	AfterTouchChannel     = 0xD0;	///< Channel (monophonic) AfterTouch
byte	PitchBend             = 0xE0;	///< Pitch Bend
byte	SystemExclusive       = 0xF0;	///< System Exclusive
byte	TimeCodeQuarterFrame  = 0xF1;	///< System Common - MIDI Time Code Quarter Frame
byte	SongPosition          = 0xF2;	///< System Common - Song Position Pointer
byte	SongSelect            = 0xF3;	///< System Common - Song Select
byte	TuneRequest           = 0xF6;	///< System Common - Tune Request
byte	Clock                 = 0xF8;	///< System Real Time - Timing Clock
byte	Start                 = 0xFA;	///< System Real Time - Start
byte	Continue              = 0xFB;	///< System Real Time - Continue
byte	Stop                  = 0xFC;	///< System Real Time - Stop
byte	ActiveSensing         = 0xFE;	///< System Real Time - Active Sensing
byte	SystemReset           = 0xFF;	///< System Real Time - System Reset
byte	InvalidType           = 0x00;   ///< For notifying errors


byte commandByte;
byte inchannel;
byte inNote;
byte invelocity;

SoftwareSerial SSerial(2, 3); // RX, TX

long bpm = 120;
long tempo = 1000/(bpm/60);
long prevmillis = 0;
long interval = tempo/24;    //interval is the number of milliseconds defined by tempo formula.

SdFat	SD;
MIDIFile SMF;
//SdFile file;

// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
//#define  SD_SELECT  10

#define	WAIT_DELAY	2000	// ms

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

const size_t MAX_FILE_COUNT = 500;

const char* FILE_EXT = "MID";

size_t fileCount = 0;

uint16_t fileIndex[MAX_FILE_COUNT];

// store error strings in flash to save RAM
#define error(s) SD.errorHalt_P(PSTR(s))


byte P = 0;  //program or patch number
byte M1 = 0;  //mute for part 1 off
byte M2 = 0;  //mute for part 2 off
byte M3 = 0;  //mute for part 3 off
byte M4 = 0;  //mute for part 4 off
byte M5 = 0;  //mute for part 5 off
byte M6a = 0;  //mute for part 6a off
byte M6b = 0;  //mute for part 6b off
byte M7a = 0;  //mute for part 7a off
byte M7b = 0;  //mute for part 7b off
byte S = 0;   //stop/start bit


/*void HandleClock() {
 
 //do stuff ?
 
 }
 */
 char name[13];
 String strname = "";
SdFile root;
void getFileNames(){
  
  while (root.openNext(SD.vwd(), O_READ)) {
      root.getFilename(name);
      root.close();
      strname = name;
      
      if (strname.endsWith(".MID")) {
     tft.println(name); 
      
      }
    
}}
//   root = SD.open("/");
//    printDirectory(root);
//}
//
//void printDirectory(SdFile dir) {
//    while(true) {
//
//        SdFile entry =  dir.openNextFile();
//        if (! entry) {
//            dir.rewindDirectory();
//            break;
//        }
//        else
//          entry.close(); 
//
//        tft.println(entry.name());
//   }
  
  
//  dir_t dir;
//  char name[13];
//  // start at beginning of root directory
//  SD.vwd()->rewind();
//
//  // find files
//  while (SD.vwd()->readDir(&dir) == sizeof(dir)) {
//    if (strncmp((char*)&dir.name[8], FILE_EXT, 3)) continue;
//    if (fileCount >= MAX_FILE_COUNT) error("Too many files");
//    fileIndex[fileCount++] = SD.vwd()->curPosition()/sizeof(dir) - 1;
//  }
//
//  PgmPrint("Found count: ");
//  Serial.println(fileCount);
//  char *tuneList[fileCount];
//
//  for (size_t i = 0; i < fileCount; i++) {
//    if (!file.open(SD.vwd(), fileIndex[i], O_READ)) error("open failed");
//    file.getFilename(name);
//    PgmPrint("Opened: ");
//    Serial.println(name);
//    // process file here
//    tuneList[i]=name;
//    file.close();
//  }
//  PgmPrintln("Done");
//}




//char *tuneList[] = root = SD.open("/");
//{
//  "LOOPDEMO.MID",
//  "XMAS.MID"
//};

// }





void sendProgramChange(byte Program, byte inchannel){
  SSerial.write(P);
  SSerial.write(inchannel);
}

void sendChangeControl(byte ControlNumber,byte ControlValue,byte inchannel){
  SSerial.write(ControlChange);
  SSerial.write(ControlNumber);
  SSerial.write(ControlValue);
  inchannel = inchannel;
  SSerial.write(inchannel);
}

void sendNoteOff(byte inNote, byte invelocity, byte inchannel){
  SSerial.write(NoteOff);
  SSerial.write(inNote);
  SSerial.write(invelocity);
  SSerial.write(inchannel);
}
void checkdata(){
  Serial.println(commandByte);
  Serial.println(inNote);
  Serial.println(invelocity);
}

void HandleNoteOn (byte commandByte, byte inNote, byte invelocity){
  //if note X is sent, send program change control to go up
  //todo: 

  if (invelocity>0){ //needs to be greater than zero, not equal to,
    switch (inNote) {
    case 58: //note value of A#3 or 0x3a    
      if (S == 0){
        S++;
        // MIDI.sendRealTime(Start);
        SSerial.write(Start);
        Play();  //
      }
      else
      {
        S--;
        // MIDI.sendRealTime(Stop);
        SSerial.write(Stop);  
        SMF.close(); ///probably working, but the player just moves to the next song...
        ////        MIDI.sendSongPosition(0);
      }
      break;
    case 60: //note value of C4  or 0x3C, handles program change 'up'
      if (P == 127){    //this notes that the program is at the highest of 127 and needs reset to 0
        P=0;
        sendChangeControl(99,05,10);  //double check these control change msgs
        sendChangeControl(98,107,10);
        sendChangeControl(06,0,10);
        sendChangeControl(99,05,10);
        sendChangeControl(98,108,10);
        sendChangeControl(06,0,10);
        sendProgramChange(P,10);
      }
      else
      {
        P++;
        sendChangeControl(99,05,10);
        sendChangeControl(98,107,10);
        sendChangeControl(06,0,10);
        sendChangeControl(99,05,10);
        sendChangeControl(98,108,10);
        sendChangeControl(06,0,10);
        sendProgramChange(P,10);

      }
      break;
    case 62: // note value of D4  or 0x3E, handles program change 'down'.
      if (P== 0){
        P=127;
        sendChangeControl(99,05,10);
        sendChangeControl(98,107,10);
        sendChangeControl(06,0,10);
        sendChangeControl(99,05,10);
        sendChangeControl(98,108,10);
        sendChangeControl(06,0,10);
        // MIDI.sendProgramChange(P,10); 
      }
      else 
      {
        P--;
        sendChangeControl(99,05,10);
        sendChangeControl(98,107,10);
        sendChangeControl(06,0,10);
        sendChangeControl(99,05,10);
        sendChangeControl(98,108,10);
        sendChangeControl(06,0,10);
        //  MIDI.sendProgramChange(P,10);

      }
      break;
    case 64:		//part 1 mute, M variable tells us if it's muted already;  all these mutes actually set the volume to zero, uses the volume NRPNs!
      if (M1==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,1,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M1=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,1,10);
        sendChangeControl(06,120,10);
        M1=0;
      }

      break;
    case 66:
      if (M2==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,9,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 2
        //need to toggle it
        M2=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,9,10);
        sendChangeControl(06,120,10);
        M2=0;
      }
      break;
    case 68:   //mute for part 3
      if (M3==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,17,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 3
        //need to toggle it
        M3=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,17,10);
        sendChangeControl(06,120,10);
        M3=0;
      }
      break;
    case 70:
      if (M4==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,25,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M4=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,25,10);
        sendChangeControl(06,120,10);
        M4=0;
      }
      break;
    case 72:
      if (M5==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,33,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M5=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,33,10);
        sendChangeControl(06,120,10);
        M5=0;
      }
      break;
    case 74:
      if (M6a==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,41,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M6a=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,41,10);
        sendChangeControl(06,120,10);
        M6a=0;
      }
      break;
    case 76:
      if (M6b==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,49,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M6b=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,49,10);
        sendChangeControl(06,120,10);
        M6b=0;
      }
      break;
    case 78:
      if (M7a==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,57,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M7a=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,57,10);
        sendChangeControl(06,120,10);
        M7a=0;
      }
      break;
    case 80:
      if (M7b==0) {
        sendChangeControl(99,5,10);
        sendChangeControl(98,65,10);
        sendChangeControl(06,0,10);  //sets level to zero, for part 1
        //need to toggle it
        M7b=1;
      }
      else {
        sendChangeControl(99,5,10);
        sendChangeControl(98,65,10);
        sendChangeControl(06,120,10);
        M7b=0;
      }
      break;


    } //end of switch (note)  



  }

  else{
    switch(inNote){
    case 58:
      sendNoteOff(58,0,10);
      break;
    case 60:
      sendNoteOff(60,0,10);
      break;
    case 62:
      sendNoteOff(62,0, 10);
      break;   
    case 64:
      sendNoteOff(64,0, 10);
      break;      
    case 66:
      sendNoteOff(66,0, 10);
      break;      
    case 68:
      sendNoteOff(68,0, 10);
      break;      
    case 70:
      sendNoteOff(70,0, 10);
      break;      
    case 72:
      sendNoteOff(72,0, 10);
      break;      
    case 74:
      sendNoteOff(74,0, 10);
      break;   
    case 76:
      sendNoteOff(76,0, 10);
      break;      
    case 78:
      sendNoteOff(78,0, 10);
      break;     
    case 80:
      sendNoteOff(80,0, 10);
      break;            

    }
  }

}




void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{

  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    SSerial.write(pev->data[0] | pev->channel);
    SSerial.write(&pev->data[1], pev->size-1);

  }
  else
  {
    SSerial.write(pev->data, pev->size);

  }


  //  DEBUG("\nM T");
  //  DEBUG(pev->track);
  //  DEBUG(":  Ch ");
  //  DEBUG(pev->channel+1);
  //  DEBUG(" Data ");
  //  for (uint8_t i=0; i<pev->size; i++)
  //  {
  //	DEBUGX(pev->data[i]);
  //    DEBUG(' ');
  //  }
}



void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event	ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}



void Sync(){

  unsigned long currentMillis = millis();
  if(currentMillis - prevmillis > interval) {
    // save the last time.
    prevmillis = currentMillis;
    // MIDI.sendRealTime(midi::Clock);   MIDI 4.0 CONVENTION
    SSerial.write(Clock); 
  }
}

/*-----------------------------SETUP--------------------------------------------------------*/
void setup(void)
{
  SSerial.begin(31250);
  Serial.begin(9600);
  
  //  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
    //  tft.fillScreen(ST7735_BLACK);

 tft.println("SD Init...");
  // Initialise SD
   if (!SD.begin(SD_CS, SPI_HALF_SPEED)) SD.initErrorHalt();
//  if (!SD.begin(SD_CS, SPI_HALF_SPEED))
//  {
//    //    DEBUG("\nSD init fail!");
//    tft.println("SD init fail!");
//    return;
//  }
tft.println("SD Init completed");
//  // Initialise MIDIFile
  SMF.begin(&SD);
 SMF.setMidiHandler(midiCallback);
//  SMF.setSysexHandler(sysexCallback);

  //MIDI.sendProgramChange(1, 1);
  
// tft.fillScreen(ST7735_BLACK);  
//getFileNames();   //WORKS!  PRINTS FULL LIST OF FILES AT ROOT OF DRIVE
///  tft.print(  SD.ls());

}

/*-----------------------------END SETUP-----------------------------------------------------*/

void Play(){
  int  err = -1;

  //  for (uint8_t i=0; i<ARRAY_SIZE(tuneList); i++)
  //{
  // reset LEDs
  // use the next file name and play it
  //    SMF.setFilename(tuneList[i]);
  //
  //    err = SMF.load();
  //
  //    if (err != -1)
  //    {
  //      //		DEBUG("\nSMF load Error ");
  //      //		DEBUG(err);
  //
  //      delay(WAIT_DELAY);
  //    }
  //    else
  //    {
  //      // play the file
  //      while (!SMF.isEOF())
  //      {
  //        if (SMF.getNextEvent())
  //          Sync();
  //           CheckMIDI();
  //             SMF.setTempo(bpm);
  //      }
  //      // done with this one
  //      SMF.close();
  //      midiSilence();
  //
  //      // signal finsh LED with a dignified pause
  //        delay(WAIT_DELAY);
  //    }
  //} 
}


void CheckMIDI(){
  do{
    if (SSerial.available()>0){

      byte indata = SSerial.read();
      if (indata >=144 && indata <= 159){          //144 is simply 0x90 or MIDI noteOn, there are 15 (0-15) possible channels so the max is 159 or 0x9F. I only care about NoteOn status messages.
        // Serial.println("NoteOn rxd");
        commandByte = indata;//read first byte
        inNote = SSerial.read();//read next byte
        invelocity = SSerial.read();//read final byte
        inchannel = (commandByte - 144);
        HandleNoteOn(commandByte, inNote, invelocity);
      }
    }
  }
  while (SSerial.available() > 2);//when at least three bytes available

}

void sendNoteOn(byte cmd, byte inNote, byte invelocity){
  SSerial.write(cmd);
  SSerial.write(inNote);
  SSerial.write(invelocity);
}

void goodnotes(){
  for (byte inNote = 60; inNote <64; inNote++){
    sendNoteOn(0x90, inNote, 120);
    delay(100);
    sendNoteOn(0x90, inNote, 0);
    delay(100);
  }
}


void loop()
{
 // CheckMIDI();





  //Play();

}


