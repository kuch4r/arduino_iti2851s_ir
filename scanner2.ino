// IR DEFINITIONS
#define IRpin_PIN       PIND
#define IRpin           2

#define MAX_CMD         15
#define MAXPULSE        9000                   // the maximum pulse we'll listen for - 65 ms
#define MAX_PULSE_PAIRS 60                      // maximum number of pulse pairs to store

// we will store up to 60 pulse pairs (this is -a lot-)
uint16_t pulses[MAX_PULSE_PAIRS][2];
uint16_t command[MAX_CMD][5];  // pair is high and low pulse
uint32_t stats[16][4];
uint8_t currentPulse = 0;                       // index for pulses we're storing
uint8_t currentCommand = 0;

static uint8_t


// variables for storing times
unsigned long currMicros, lastMicros, diffMicros, lastCommand;
boolean newCodeToRead = false;                  // is a new IR code ready to be read?

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7);  // Set the LCD I2C address


void readIR()
{
  if(newCodeToRead)                             // if the new code is unread, exit (to prevent overwriting)
    return;
    
  currMicros = micros();

  if(currentPulse < MAX_PULSE_PAIRS)            // if the pulses array isn't full
  {
    diffMicros = currMicros - lastMicros;       // time since last pin change

    if(IRpin_PIN & (1 << IRpin))                // HIGH? (no signal)
    {
      pulses[currentPulse++][1] = diffMicros;   // save ON value
      lastMicros = currMicros;                  // current time becomes the time of last signal value change
    }
    else                                        // LOW? (signal!)
    {
      pulses[currentPulse][0] = diffMicros;     // save OFF value
      lastMicros = currMicros;                  // current time becomes the time of last signal value change
    }
  }
  else                                          // set newCodeToRead flag if maximum buffer limit reached
    newCodeToRead = true;
}

int decodeIR()
{
  unsigned long lastTime = lastMicros;
  unsigned long nowTime = micros();

  if(nowTime > lastTime)                        // This check is so diffTime is positive. lastTime can sometimes be greater if interrupt is called inbetween
  {
    unsigned long diffTime = (nowTime - lastTime);

    // if there is new code to be read, or we've been waiting too long and the code has ended
    if(newCodeToRead || (diffTime > MAXPULSE && currentPulse != 0))
    {
      detachInterrupt(0);                       // disable interrupt to prevent value changing
      store_pulse();
      newCodeToRead = false;                    // newCode has been read. Start recording for next IR code
      currentPulse = 0;                         // reset pulse counter
      attachInterrupt(0, readIR, CHANGE);       // re-enable interrupts

      return 0;                                 // success! :D
    }
  }

  return -1;                                     // no new data :(
}

void store_pulse(void) {
  for( uint8_t i = 0 ; i < currentPulse ; i++ ) {
    if( currentCommand < MAX_CMD && pulses[i][0] > 12700 && pulses[i][0] < 13100 && i+8 < currentPulse && pulses[i+1][0] > 750 && pulses[i+1][0] < 850 ) {
      for( uint8_t j = 0 ; j < 5 ; j++ ) {
        command[currentCommand][j] = pulses[i+2+j][0];
//        Serial.print( command[currentCommand][j], DEC );
  //      Serial.print(", ");
      }
      currentCommand++;
      lastCommand = micros();
    //  Serial.println("");
    }
  }
}

void print_commands(void) {
  lcd.clear();
  lcd.setCursor ( 0, 0 );        // go to the next line
  delay(200);
  
  for( uint8_t i = 0 ; i < currentCommand ; i++ ) {
    uint16_t * cmd = command[i];  
    uint16_t vals[5];
    for( uint8_t j = 0 ; j < 5 ; j++ ) {
      vals[j] = match_value(cmd[j]);
      if(vals[j] > 15 ) {
        lcd.print("?");
        Serial.print("??: ");
        Serial.println(vals[j]);
      } else {
        lcd.print(vals[j],HEX);
      }
    }
    lcd.print(" ");
    if( i == 1 ) {
      lcd.setCursor ( 0, 1 ); 
    }
  }
  currentCommand = 0;
}

uint16_t match_value( uint16_t raw ) {
  static uint16_t lookup[][3] = {
    {600,760, 70},   // 0
    {770,890, 83},   // 1
    {890,1010, 96},  // 2
    {1010,1180, 110},// 3
    {1180,1320, 125},// 4
    {1320,1440, 138},// 5
    {1440,1580, 151},// 6
    {1580,1730, 165},// 7
    {1730,1870, 179},// 8
    {1870,2000, 194},// 9
    {2001,2150, 206},// A (10)
    {2150,2280, 221},// B (11)
    {2280,2400, 235},// C (12)
    {2400,2550, 248},// D (13)
    {2550,2700, 261},// E (14)
    {2700,2900, 275},// F (15)
    {0,0,0}
  };
  for( uint8_t i = 0 ; lookup[i][0] > 0 ; i++ ) {
    if( raw > lookup[i][0] && raw <= lookup[i][1] ) {
      stats[i][0]++;
      stats[i][1] += raw;
      if( raw > stats[i][2] ) {
        stats[i][2] = raw;
      } 
      if( stats[i][3] == 0 || raw < stats[i][3] ) {
        stats[i][3] = raw;
      }
      return i;  
      //return lookup[i][2]; 
    }
  }
  return raw;
}

void setup(void)
{
    lcd.begin(16,2);               // initialize the lcd 
  lcd.setBacklightPin(3,POSITIVE);
  lcd.setBacklight(HIGH);

  lcd.home ();                   // go home
  lcd.print("Waiting for signal");  

  
  lastCommand = lastMicros = micros();                        // initialize lastMicros
  attachInterrupt(0, readIR, CHANGE);           // setup interrupt to call readIR() whenever INT0 pin (pin 2) changes

  Serial.begin(9600);                           // Begin serial data transfer
  Serial.println("Ready to decode IR!!!");
}

void loop(void) {
  int decoded = decodeIR();
  if( currentCommand > 0 && micros() - lastCommand > 500000 ) {
    print_commands();
/*    for( uint8_t i = 0 ; i < 16 ; i++ ) {
      Serial.print("Mean: ");
      Serial.print(stats[i][1]/stats[i][0], DEC);
      Serial.print(" Count: ");
      Serial.print(stats[i][0], DEC);
      Serial.print(" Max: ");
      Serial.print(stats[i][2], DEC);
      Serial.print(" Min: ");
      Serial.print(stats[i][3], DEC);
      Serial.println("");
    }*/
  }
}
