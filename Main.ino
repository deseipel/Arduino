

/*

 To Do:

2.   account for differnt PPQ?
3.  track count is only right after stopping playback?

 
 */
#include <MIDIFile.h>
#include <MIDIHelper.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <SdFatUtil.h>
#include <SdFat.h>
#include <SPI.h>



// Edit for Shield Pinouts!
#define sclk 52
#define mosi 51   
#define SD_CS    4  // Chip select line for SD card
#define TFT_CS  10  // Chip select line for TFT display
#define TFT_DC   8  // Data/command line for TFT
#define TFT_RST  0  // Reset line for TFT (or connect to +5V)
#define ADA_JOYSTICK 3
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

MIDIFile SMF;  
//MFTrack MF;
SdFat	SD;
//MIDIFile SMF2;
#define	BEAT_LED		13


// these are bytes that are mostly unused.  NoteOn, NoteOff, Program Change & Control changes are hard coded to channel 9 (which is channel 10 to external devices).
byte	NoteOff	              = 0x89;	///< Note Off
byte	NoteOn                = 0x99;	///< Note On
byte	AfterTouchPoly        = 0xA0;	///< Polyphonic AfterTouch
byte	ControlChange         = 0xB9;	///< Control Change / Channel Mode
byte	ProgramChange         = 0xC9;	///< Program Change
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

//byte Playing = 0;  //this tells the sync if a file is already playing or not.
byte commandByte;
byte inchannel;
byte outchannel=10;
byte inNote;
byte invelocity;
static boolean inBeat = false;
uint8_t track_cnt;
//SoftwareSerial SSerial(2, 3); // RX, TX

// Midi Clock  //
int  PPQ = 96; // Min BPM = 50 //
uint8_t DIV = (PPQ/24);
volatile uint8_t  sync24PPQ = 0;
long bpm = 100;
long interval = (6000000/(24 * bpm));  
long qnoteInterval = (6000000/bpm);  
byte startsignal = 1;
uint8_t midiClockRunning = 0; 
uint8_t midiClockType = 2; // 0=Internal Only (no Sync), 1=SyncToExternal (Slave), 2=SendSync (Master)
volatile byte tick;
volatile long long tick_count= 0 ;
byte started =0;


byte oldP = 1;  // a byte to compare to the new Program/patch number //
byte P = 1;  //program or patch number
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

#define	WAIT_DELAY	2000	// ms

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

const size_t MAX_FILE_COUNT = 500;                                                            // not used?
const char* FILE_EXT = "MID";
size_t fileCount = 0;


// store error strings in flash to save RAM
#define error(s) SD.errorHalt_P(PSTR(s))                                                      //not used?

//  Joystick directions - Works with our screen rotation (1), yaay
enum { 
  Neutral, Press, Up, Down, Right, Left };
const char*  Buttons[]={
  "Neutral", "Press", "Up", "Down", "Right", "Left" };
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

  if (joystickState < 50) return Down;
  if (joystickState < 150) return Right;
  if (joystickState < 250) return Press;
  if (joystickState < 500) return Up;
  if (joystickState < 650) return Left;
  return Neutral;
  // delay(100);
} 

enum COLORS { 	
  BLACK = 0x0000, BLUE = 0x001F, RED = 0xF800, ORANGE = 0xFA60, GREEN = 0x07E0,
  CYAN = 0x07FF, MAGENTA = 0xF81F, YELLOW = 0xFFE0, GRAY = 0xCCCC, WHITE = 0xFFFF
};

enum arrows { 
  uarr = 0x18, darr = 0x19, larr = 0x1b, rarr = 0x1a, upyr = 0x1e, dpyr = 0x1f, lpyr = 0x11, rpyr = 0x10 };


////////////////////////////////////////////////////////////////////////////////
//  size 1 template:	12345678901234567890123456 <-- if last char is ON 26, \n not req'd; driver inserts it
//static char version[]={"simpleMenu.ino 2013SEP12"};
char textx[64];  // Made buffer big enough to hold two lines of text
//char msg[64];
////////////////////////////////////////////////////////////////////////////////
//  Static menu manager
#define MAX_SELECTIONS 5
#define MAX_PROMPT_LEN 20

char  nameFiles[128][20];
char name[20];
String strname = "";
SdFile root;
int filePOS = 1;
int fileIndex = 0;
int listStart = 1;                            //should this be set to 1 instead?
int maxmidicount;
String current_song;
String next_song;


void setup(){
  //   Serial.begin(57600);
  Serial.begin(31250);

  pinMode(A5, OUTPUT);      // sets the digital pin as output, D19 on Rugged circuits MIDI shield = A5 on Mega.
  digitalWrite(A5, HIGH);
  tft.initR( INITR_BLACKTAB );    //  "tab color" from screen protector; there's RED and GREEN, too.
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(1); //  1 = 5x8, 2 = 10x16; chars leave blank pixel on bottom
  tft.println("TFT Setup Completed");
  tft.println("SD Init...");
  // Initialise SD
  if (!SD.begin(SD_CS, SPI_FULL_SPEED))
  {
    // Serial.print("\nSD init fail!");
    tft.println("SD init fail!");
    return;
  }
  tft.println("SD Init completed");

  // draw_menu();
  IndexFileNames();   //get a count of the MID files
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  // pinMode(13, OUTPUT);
  //digitalWrite(BEAT_LED, LOW);
  setCurrentSong();
  setNextSong();
   draw_menu();
}




void loop() {
  CheckMIDI();
  if( doMenu() ){
//
//    //		//  something happened with joystick, an update() action could be called here
  }
  
  if ((S==1) && (midiClockRunning ==1) ){
  Playit();
  }
}

