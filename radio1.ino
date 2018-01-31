#include <Si4703_Breakout.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROM.h>

//#define CE 9
//#define RST 10 
//#define DC 11
//#define DIN 12 
//#define CLK  13  

#define PUSH 4

Adafruit_PCD8544 lcd = Adafruit_PCD8544(13, 12, 11, 9, 10);

int resetPin = A3; 
int SDIO = A4;
int SCLK = A5;

int LED = A0;

Si4703_Breakout radio(resetPin, SDIO, SCLK);
int channel = 1052;
int volume = 7;
char rdsBuffer[10];

volatile int number = 0;                
int oldnumber = number;
int lastnumber;

volatile boolean halfleft = false;      // Used in both interrupt routines
volatile boolean halfright = false;

int fmFreq[11] = {1052, 967, 1072, 1060, 958, 967, 971, 974, 1004, 1011, 1059};
char* fmName[11] = {"Wave105", "Heart", "Breeze", "Sam FM", "Mersey","City FM", "Buzz", "Rock FM", "Smooth", "Classic ", "Cty Tlk"};
                      
byte savedContrast = 0;                      

int mode = 1;
int limit = 10;
long timer;
int contrast = 55;
bool rds = false;
float volts;
long count;

void setup() {
  
  pinMode(10, OUTPUT);        
  pinMode(9, OUTPUT);        
  pinMode(11, OUTPUT);         
  pinMode(12, OUTPUT);       
  pinMode(13, OUTPUT); 
  
  pinMode(LED, OUTPUT);

  pinMode(resetPin, OUTPUT); 
  
  pinMode(2, INPUT);
  digitalWrite(2, HIGH);                // Turn on internal pullup resistor
  pinMode(3, INPUT);
  digitalWrite(3, HIGH);                // Turn on internal pullup resistor
  attachInterrupt(1, isr_2, FALLING);   // Call isr_2 when digital pin 3 goes LOW
  attachInterrupt(0, isr_3, FALLING);   // Call isr_3 when digital pin 2 goes LOW 
  
  pinMode(PUSH, INPUT);
  digitalWrite(PUSH, HIGH); 
  
  radio.powerOn();
  radio.setVolume(volume);
  
  number = EEPROM.read(0);
  if (number > 10) {
    number = 10;
  }
  radio.setChannel(fmFreq[number]);
  
  contrast = EEPROM.read(1);
  if (contrast > 60) {
    contrast = 60;
  }
  lcd.begin(contrast);            // set LCD contrast
  lcd.clearDisplay();
  digitalWrite(LED, HIGH);

  if (digitalRead(PUSH) != LOW) {
   lcd.println("Preset"); lcd.println();
   lcd.setTextSize(2);
   lcd.print(fmName[number]); 
   mode = 1;
  } else {
    lcd.println("Set");
    mode = 3;
  }
  lcd.display();   
  while(digitalRead(PUSH) == LOW);  
  count = millis();
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

void showDisplay() {
   if (mode == 1) {
      limit = 10;
      radio.setChannel(fmFreq[number]);
        
      lcd.clearDisplay();
      lcd.setTextSize(1);
      lcd.println("Preset "); lcd.println();
      lcd.setTextSize(2);
      lcd.print(fmName[number]);  
      lcd.setCursor(52, 36);
      lcd.setTextSize(1);
      lcd.print(volts);
      lcd.print("v");
      lcd.display();   
   }
   if (mode == 2) {
      limit = 10;
      if (number != oldnumber){
        channel = fmFreq[number];
        oldnumber < number ? channel = radio.seekUp() : channel = radio.seekDown();
        oldnumber = number;
      }
      lcd.clearDisplay();
      lcd.setTextSize(1);
      lcd.println("Seek");lcd.println();
      lcd.setTextSize(2);
      if (channel < 1000) lcd.print(" ");
      lcd.print((float) channel/10, 1);
      lcd.setTextSize(1);
      lcd.setCursor(58, 23);
      lcd.print(" MHz");
      lcd.setCursor(0, 36);
      lcd.print(rdsBuffer);      
      lcd.setCursor(52, 36);
      lcd.print(volts);
      lcd.print("v");      
      lcd.display();
   }   
}


void loop() {
  
  volts = readVcc();
  volts = volts / 1000;  
  if (millis() - count > 30000) {
      lcd.setContrast(contrast - (int)volts % 10);
      showDisplay();
      count = millis();
  }       
  
  if (digitalRead(PUSH) == LOW) {
    EEPROM.write(0, number);
    EEPROM.write(1, contrast);
    delay(50);
     mode == 1? mode = 2 : mode = 1 ;
     lcd.clearDisplay();
     lcd.setTextSize(1);
     if (mode == 1) {
        lcd.println("Preset");
        lcd.println();
        lcd.setTextSize(2); 
        number = lastnumber;
        lcd.print(fmName[number]); 
     }     
     if (mode == 2) {
        lcd.println("Seek");
        lcd.println();
        lcd.setTextSize(2); 
        lastnumber = number;
        channel = fmFreq[number];
        if (channel < 1000) lcd.print(" ");  
        lcd.print((float) channel/10, 1);
        lcd.setTextSize(1);
        lcd.setCursor(58, 23);
        lcd.print(" MHz");     
     }     
     lcd.display();     
     while(digitalRead(PUSH) == LOW);
     delay(200);
  }
  
  if(number != oldnumber){              // Change in value ?
  
   if (mode == 1) {
      limit = 10;
      radio.setChannel(fmFreq[number]);
      showDisplay();  
      
   }
   if (mode == 2) {
      limit = 10;
      if (number != oldnumber){
        channel = fmFreq[number];
        oldnumber < number ? channel = radio.seekUp() : channel = radio.seekDown();
        oldnumber = number;
      }
      showDisplay();
   }
   if (mode == 3) {
      limit = 60;
      lcd.setContrast(contrast+number);
      contrast = contrast + number;
   }
    oldnumber = number;
  } 
  
  rds = rdsBuffer[0] != '\0';
  
  if (!rds && (mode == 2) && (millis() - timer > 5000)){
    radio.readRDS(rdsBuffer, 2000);
    if (rdsBuffer[0] != '\0') {
      rds = true;
      lcd.setCursor(0, 36);
      lcd.print(rdsBuffer);
      lcd.display();
    } 
  } 
}


void isr_2(){                                              // Pin2 went LOW
  delay(1);                                                // Debounce time
  timer= millis();
  rds = false;
  if(digitalRead(2) == LOW){                               // Pin2 still LOW ?
    if(digitalRead(3) == HIGH && halfright == false){      // -->
      halfright = true;                                    // One half click clockwise
    }  
    if(digitalRead(3) == LOW && halfleft == true){         // <--
      halfleft = false;                                    // One whole click counter-
      number--;                                            // clockwise
      if (number < 0) number = 10;
    }
  }
}

void isr_3(){                                             // Pin3 went LOW
  delay(1);                                               // Debounce time
  timer = millis();
  rds = false;
  if(digitalRead(3) == LOW){                              // Pin3 still LOW ?
    if(digitalRead(2) == HIGH && halfleft == false){      // <--
      halfleft = true;                                    // One half  click counter-
    }                                                     // clockwise
    if(digitalRead(2) == LOW && halfright == true){       // -->
      halfright = false;                                  // One whole click clockwise
      number++;
      if (number > 10) number = 0;
    }
  }
}


