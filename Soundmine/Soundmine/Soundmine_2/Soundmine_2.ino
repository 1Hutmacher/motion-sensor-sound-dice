
#include <WaveHC.h>
#include <WaveUtil.h>
/*
 * Define macro to put error messages in flash memory
 */
#define error(msg) error_P(PSTR(msg))

//the time we give the sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 20;       
 
//the time when the sensor outputs a low impulse
long unsigned int lowIn;        
 
//the amount of milliseconds the sensor has to be low
//before we assume all motion has stopped
long unsigned int pause = 1000; 
 
boolean lockLow = true;
boolean takeLowTime; 
 
int pirPin = 7;    //the digital pin connected to the PIR sensor's output
int ledPin = 12;

//Soundvariablen


SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time
int randNumber;
uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads




// Function definitions (we define them here, but the code is below)
void play(FatReader &dir, int index);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  randomSeed(analogRead(0));
  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(pirPin, LOW);
 
  //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
    for(int i = 0; i < calibrationTime; i++){
      Serial.print(".");
      delay(1000);
      }
    Serial.println(" done");
    Serial.println("SENSOR ACTIVE");
    delay(50);

//Sound Setup
  putstring_nl("\nWave test!");  // say we woke up!
  
  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(FreeRam());

  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    error("Card init. failed!");  // Something went wrong, lets print out why
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
  
  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                           // we found one, lets bail
  }
  if (part == 5) {                     // if we ended up not finding one  :(
    error("No valid FAT partition!");  // Something went wrong, lets print out why
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    error("Can't open root dir!");      // Something went wrong,
  }
  
  // Whew! We got past the tough parts.
  putstring_nl("Files found (* = fragmented):");

  // Print out all of the files in all the directories.
  root.ls(LS_R | LS_FLAG_FRAGMENTED);
  digitalWrite(ledPin, HIGH);   
  delay(5000);
  Serial.println("Ready");
  digitalWrite(ledPin, LOW);  
}

void loop() {
  // put your main code here, to run repeatedly:
       if(digitalRead(pirPin) == HIGH){
          //the led visualizes the sensors output pin state
       if(lockLow){ 
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow = false;           
         Serial.println("---");
         Serial.print("motion detected at ");
         Serial.print(millis()/1000);
         Serial.println(" sec");
         
         root.rewind();
         // get random file index
         int index = get_file_index(root);
         // reset directroy after checking files
         root.rewind();

         play(root, index);
         delay(50);

         } 
         takeLowTime = true;
       }
 
     if(digitalRead(pirPin) == LOW){      
       digitalWrite(ledPin, LOW);  //the led visualizes the sensors output pin state
 
       if(takeLowTime){
        lowIn = millis();          //save the time of the transition from high to LOW
        takeLowTime = false;       //make sure this is only done at the start of a LOW phase
        }
       //if the sensor is low for more than the given pause,
       //we assume that no more motion is going to happen
       if(!lockLow && millis() - lowIn > pause){ 
           //makes sure this block of code is only executed again after
           //a new motion sequence has been detected
           lockLow = true;                       
           Serial.print("motion ended at ");      //output
           Serial.print((millis() - pause)/1000);
           Serial.println(" sec");
           delay(50);
           }
       }
}

/////////////////////////////////// HELPERS
/*
 * print error message and halt
 */
void error_P(const char *str) {
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck();
  while(1);
}
/*
 * print error message and halt if SD I/O error, great for debugging!
 */
void sdErrorCheck(void) {
  if (!card.errorCode()) return;
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

/*
 * get the numbre of wav files in the card
 * We only use the top level directory, other should work as well though
 */
int get_file_index(FatReader &dir) {
  FatReader file;
  int count = 0;
  while (dir.readDir(dirBuf) > 0) {    // Read every file in the directory one at a time
    if (DIR_IS_SUBDIR(dirBuf)
        && strncmp_P((char *)&dirBuf.name[8], PSTR("WAV"), 3)) {
      continue;
    }
    if (!file.open(vol, dirBuf)) {        // open the file in the directory
      error("file.open failed");          // something went wrong
    }
    printEntryName(dirBuf);
    ++count;
  }
  return random(0, count);
}

/*
 * play recursively - possible stack overflow if subdirectories too nested
 */
void play(FatReader &dir, int index) {
  FatReader file;
  int counter = 0;
  
  while (dir.readDir(dirBuf) > 0) {    // Read every file in the directory one at a time
    // Skip it if a subdirectory and not a .WAV file
    // Files are named after 8.3 format
    if (DIR_IS_SUBDIR(dirBuf)
         && strncmp_P((char *)&dirBuf.name[8], PSTR("WAV"), 3)) {
      continue;
    }

    if (!file.open(vol, dirBuf)) {        // open the file in the directory
      error("file.open failed");          // something went wrong
    }
    
    if (file.isDir()) {                   // check if we opened a new directory
      // ignore subdirectories
      continue;
      putstring("Subdir: ");
      printEntryName(dirBuf);
      Serial.println();
      dirLevel += 2;                      // add more spaces
      // play files in subdirectory
      play(file, index);                         // recursive!
      dirLevel -= 2;    
    }
    else {
      // check against counter; only plays the provided index
      if (counter != index) {
        ++counter;
        continue;  
      }
      // Aha! we found a file that isnt a directory
      putstring("Playing ");
      printEntryName(dirBuf);              // print it out
      if (!wave.create(file)) {            // Figure out, is it a WAV proper?
        putstring(" Not a valid WAV");     // ok skip it
      } else {
        Serial.println();                  // Hooray it IS a WAV proper!
        wave.play();                       // make some noise!
        
        uint8_t n = 0;
        while (wave.isplaying) {// playing occurs in interrupts, so we print dots in realtime
          putstring(".");
          if (!(++n % 32))Serial.println();
          delay(100);
        }       
        sdErrorCheck();                    // everything OK?
        // if (wave.errors)Serial.println(wave.errors);     // wave decoding errors
        return;
      }
    }
  }
}
