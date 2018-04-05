/*
 alarm clock by Markus Rohner
 Version 1.0     1.Apr.2018
 
 Funtion: Alarm clock and Movement detection. Buzzer serves as house alarm
  
 Aknowledgements:
 1. Adafruit Neopixel library: http://learn.adafruit.com/adafruit-neopixel-uberguide/neomatrix-library
                       
NANO Pins:
 D0: (RX) ESP8266 TX
 D1: (TX) ESP8266 RX 3.3V!
 D5: green LED  not used                                                                              
 D6: Yellow LED not used
 D7: Red LED wake_up call set
 D9: Neopixel dataline + over 200Ohm resistor   should be 330Ohm though
 D10: buzzer + over 100Ohm resistor (needs to be on pin D10)
 D11: Softserial RX to FTDI
 D12: Softserial TX to FTDI  
 D14(A0): Dial Switch  
 D15(A1): Set Switch
 D16(A2): Panic Switch 
 A3: Potentiometer to set brightness of Neopixel 
 D18(A4): PIR Movement     
 D19(A5): DHT sensor
  
 Bill of material:
 - Power Supply Converter AC 100-240V to DC 5V 1A Switching
   (all devices powered directly from the 5V power supply, not by the arduino)
 - Arduino Nano
 - Nice box
 - 3 x Push button (10k resistor to GND)
 - 3 x 10K Ohm resistor for switches
 - 1 x WS2811 WS2812 5050 RGB LED Light ring Individually Addressable DC5V 
 - 1 x 1000uF 16V 10x16mm Radial Capacitor Electrolytic to protect RGB LEDs
 - 1 x Resisor 330 Ohm for RGB LEDs data line
 - 1 buzzer
 - 1 x resistor 100 Ohm for Buzzer
 - 1ea. 5mm LEDs green, yellow, red
 - 3 x 330 Ohm for LEDs
 - cables etc.
 */
#include <TimeLib.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
SoftwareSerial MySerial(11,12); // RX, TX attached to a FTDI adapter

#include "DHT.h"
#define DHTPIN 19     // what pin we're connected to
#define DHTTYPE DHT21 // AM2301  
DHT dht(DHTPIN, DHTTYPE);

// RGB Lights
#include <Adafruit_NeoPixel.h>
#define NUM_BUTTONS  12    
#define PIN 9         // Signal Pin
const int long red = 16711680;
const int long green = 45568;//was 65280
const int long blue = 255;
int brightness = map(analogRead(A3),0,1023,0,255);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_BUTTONS, PIN, NEO_GRB + NEO_KHZ800);
// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

int hours, minutes;
const int waiting = 600;
const byte Green_LED = 5;     // LED green
const byte PirPin = 18;       // choose the input pin (for PIR sensor)
const byte Red_LED = 7;       // LED red
const byte Yellow_LED = 6;    // Yellow LED
const byte dial_Switch = 14;  // Switch 2
const byte panic_Switch = 16; // Panic Switch
const byte set_up_Switch = 15;// Switch 1
int pressed_switch = 0;
const int display_time = 6000;// micro seconds display is on
bool movement = 0;
bool movement_on = 0;
float h = 0;
float t = 0;
SimpleTimer timer;
int movetimer = -1;
int snoozetimer = -1;

// Buzzer
// TONES  ==========================================
// Start by defining the relationship between 
//       note, period, &  frequency. 
#define  cc     3830    // 261 Hz 
#define  dd     3400    // 294 Hz 
#define  ee     3038    // 329 Hz 
#define  ff     2864    // 349 Hz 
#define  gg     2550    // 392 Hz 
#define  aa     2272    // 440 Hz 
#define  bb     2028    // 493 Hz 
#define  CC     1912    // 523 Hz 
// Define a special note, 'R', to represent a rest
#define  RR     0
bool alarm = LOW;
bool snooze = LOW;
int speakerOut = 10;
// MELODY and TIMING  =======================================
//  melody[] is an array of notes, accompanied by beats[], 
//  which sets each note's relative length (higher #, longer note) 
int melody[] = {  CC,  bb,  gg,  CC,  bb,   ee,  RR,  CC,  cc,  gg, aa, CC };
int beats[]  = { 16, 16, 16,  8,  8,  16, 32, 16, 16, 16, 8, 8 }; 
int MAX_COUNT = sizeof(melody) / 2; // Melody length, for looping.

// Set overall tempo
long tempo = 10000;
// Set length of pause between notes
int pause = 1000;
// Loop variable to increase Rest length
int rest_count = 100; //<-BLETCHEROUS HACK; See NOTES

// Initialize core variables
int tone_ = 0;
int beat = 0;
long duration  = 0;

//EEPROM
#include <EEPROM.h>
struct MyObject {
  byte wake_hour;
  byte wake_minute;
  byte onoff;
  };
MyObject wakeup;


void setup() {
  Serial.begin(115200);
  MySerial.begin(19200);
  dht.begin();
  timer.setInterval(120000L,checkDHT);//2 mins
  checkDHT();
  pinMode(PirPin, INPUT);
  pinMode(speakerOut, OUTPUT);  
  pinMode (dial_Switch, INPUT);
  pinMode (set_up_Switch, INPUT);
  pinMode (panic_Switch, INPUT);
  pinMode (Green_LED, OUTPUT);
  digitalWrite(Green_LED, LOW);
  pinMode (Red_LED, OUTPUT);
  digitalWrite(Red_LED, LOW);
  pinMode (Yellow_LED, OUTPUT);
  digitalWrite(Yellow_LED, LOW);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  for (int y = 0; y < 4; y++) {
    blue_circle();
  }

  EEPROM.get(0, wakeup);
  if (wakeup.onoff) { //Alarm clock is set
    digitalWrite(Red_LED, HIGH);
    }
  MySerial.println();
  MySerial.println(F("Alarm data:"));
  MySerial.print(wakeup.wake_hour);
  MySerial.print(":");
  if (wakeup.wake_minute < 10) MySerial.print("0");
  MySerial.print(wakeup.wake_minute);
  MySerial.print(" ");
  if (wakeup.onoff == 0) MySerial.println("Off");
  else MySerial.println("On");
  delay(60000);
  requestTimefromESP();
  while (!Serial) {
    } //wait for the ESP to connect
  timer.setInterval(1000L,toggle_Yellow_LED);//1 sec 
  timer.setInterval(600000L,check_time_is_set);//10 min  
}


void loop() {
  getRequestFromESP();
  if (timeStatus() == timeSet) digitalWrite(Green_LED, HIGH);
  else digitalWrite(Green_LED, LOW);
  timer.run(); // Initiates SimpleTimer
  movement = digitalRead(PirPin);
  if (movement && !movement_on) {
    MySerial.println(F("Move detected"));
    movement_on = 1;
    sendUpdateToESP();
    movetimer = timer.setTimeout(600000, resetmove);//10 mins
    }
  if (digitalRead(panic_Switch) && alarm) {
    timer.disable(snoozetimer);
    snoozetimer = timer.setTimeout(360000, resetsnooze);//6 mins
    snooze = HIGH;
    alarm = LOW;
    MySerial.println(F("panic_Switch pressed"));
    pressed_switch = 1;
    }
  else if (digitalRead(panic_Switch)) {
    pressed_switch = 1;
    MySerial.println(F("panic_Switch pressed"));
    }  
  if (digitalRead(set_up_Switch)) {
    MySerial.println(F("set_up_Switch pressed"));
    wake_time();
    } 
  if ((alarm || (wakeup.onoff && ((wakeup.wake_hour == hour()) || ((wakeup.wake_hour + 12) == hour())) && (wakeup.wake_minute == minute())))&& !snooze) {
    alarm = HIGH;
    show_time();
    sound_Alarm();
    }
  if (pressed_switch == 1) show_time();
}


void show_time (){
  int neo_hour = hour();
  if (neo_hour >= 12) neo_hour = neo_hour - 12;
  int neo_minutes = minute() / 5;
  for (int x = 0; x < NUM_BUTTONS; x++) {
    uint32_t newcolor = 0;
    if (x == 0 || x == 6 || x == 3 || x == 9) newcolor = 20;
    if (neo_hour == x) newcolor = newcolor + red;
    if (neo_minutes == x) newcolor = newcolor + green;
  strip.setPixelColor(x,newcolor);
  waitandbright(15);
  }
  if (!alarm){
    waitandbright(display_time);
    blue_circle();
    blue_circle();
    pressed_switch = 0;
  }
}

 
void wake_time() {
  int neo_hour = wakeup.wake_hour;
  if (neo_hour >= 12) neo_hour = neo_hour - 12;
  int neo_minutes = wakeup.wake_minute / 5;
  if (neo_minutes > 11) neo_minutes = 0;
  show_waketime (neo_hour,neo_minutes);
  MySerial.println(F("Set wake time"));
  while (digitalRead(set_up_Switch) == LOW) {
    if (digitalRead(dial_Switch)) {
      neo_minutes++;
      if (neo_minutes == 12) {
        neo_minutes = 0;
        neo_hour++;
        if (neo_hour == 12) neo_hour = 0;
      }
      show_waketime (neo_hour,neo_minutes);
    }
  }
    
  neo_minutes = 5 * neo_minutes;
  wakeup.onoff = 1;
  digitalWrite(Red_LED,wakeup.onoff);
  waitandbright(waiting);
  MySerial.println(F("Set alarm on/off"));
  while (digitalRead(set_up_Switch) == LOW) {
     if (digitalRead(dial_Switch)) {
       wakeup.onoff = !wakeup.onoff;
       digitalWrite(Red_LED,wakeup.onoff);
       alarm = LOW;
       snooze = LOW;
       timer.disable(snoozetimer);
       waitandbright(waiting);
      }
    }
  while (digitalRead(set_up_Switch) == LOW) {
    } 
  MySerial.println(F("Write to EEPROM"));  
  wakeup.wake_hour = neo_hour;
  wakeup.wake_minute = neo_minutes;
  EEPROM.put(0, wakeup);
  wait(waiting);
  blue_circle();
 }


void show_waketime (int hour_position, int minute_position){
  for (int x = 0; x < NUM_BUTTONS; x++) {
    uint32_t newcolor = 0;
    if (x == 0 || x == 6 || x == 3 || x == 9) newcolor = 20;
    if (hour_position == x) newcolor = newcolor + red;
    if (minute_position == x) newcolor = newcolor + green;
  strip.setPixelColor(x,newcolor);
  waitandbright(15);
  }
}


void blue_circle() {
  for (int x = 0; x < NUM_BUTTONS; x++) {
    strip.setPixelColor(x,0);
  }
  strip.show();
  for (int x = 0; x < NUM_BUTTONS; x++) {
    strip.setPixelColor(x, blue); // bright blue color.
    waitandbright(20);
    strip.setPixelColor(x,0);
    waitandbright(20);
  } 
}


void sound_Alarm() {
  MySerial.println(F("Alarm Sound"));

  // Set up a counter to pull from melody[] and beats[]
  for (int i=0; i<MAX_COUNT; i++) {
    tone_ = melody[i];
    beat = beats[i];
    duration = beat * tempo; // Set up timing
    if (digitalRead(panic_Switch) && alarm) return;
    playTone(); 
    // A pause between notes...
    delayMicroseconds(pause);
  }
 }


// PLAY TONE  ==============================================
// Pulse the speaker to play a tone for a particular duration
void playTone() {
  long elapsed_time = 0;
  if (tone_ > 0) { // if this isn't a Rest beat, while the tone has 
    //  played less long than 'duration', pulse speaker HIGH and LOW
    while (elapsed_time < duration) {

      digitalWrite(speakerOut,HIGH);
      delayMicroseconds(tone_ / 2);

      // DOWN
      digitalWrite(speakerOut, LOW);
      delayMicroseconds(tone_ / 2);

      // Keep track of how long we pulsed
      elapsed_time += (tone_);
    } 
  }
  else { // Rest beat; loop times delay
    for (int j = 0; j < rest_count; j++) { // See NOTE on rest_count
      delayMicroseconds(duration);  
    }                                
  }                                 
}


void wait (int d_time) {
  for (int i = 0; i <  abs((d_time / 15)); i++)delayMicroseconds(15000);
}


void waitandbright (int d_time) {
  strip.setBrightness(map(analogRead(A3),0,1023,0,255)); 
  strip.show();
  for (int i = 0; i <  abs((d_time / 15)); i++)delayMicroseconds(15000);

}


void getRequestFromESP(){
  if (Serial.available()) {    
    StaticJsonBuffer<190> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(Serial);
    const char* command = root["command"];
    int what = root["what"];
    int h = root["h"];
    int m = root["m"];
    int s = root["s"];
    String commandString = String(command);
    if (commandString == "alarm" || commandString == "update" || commandString == "time") {
      MySerial.print(commandString);
      MySerial.print(" ");
      if (commandString == "time") {
        MySerial.print(h);
        MySerial.print(":");
        MySerial.print(m);
        MySerial.print(":");
        MySerial.print(s);
        MySerial.println("");
      }
      MySerial.println(what);
    }
    if (commandString == "alarm" && what == 1) alarm = HIGH;
    else if (commandString == "alarm" && what == 0) alarm = LOW;
    else if (commandString == "time") setTime(h,m,s,1,1,2018);
    else if (commandString == "update") sendUpdateToESP();
    commandString = "";
   }
}


void sendUpdateToESP(){
  MySerial.print("Temperature: ");
  MySerial.print(t);
  MySerial.print(" ");
  MySerial.print("Humidity: ");
  MySerial.print(h);
  MySerial.print(" ");
  MySerial.print("Move: ");
  MySerial.println(movement_on);
  
  StaticJsonBuffer<120> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
    root["temp"] = t;
    root["hum"] = h;
    root["move"] = movement_on;
  char output[120];
  root.printTo(output,sizeof(output));
  root.printTo(Serial);
  Serial.println();
} 


void checkDHT(){
  // Read temperature as Celsius
     t = dht.readTemperature();
     h = dht.readHumidity();
}


void resetmove(){
  movement_on = 0; 
}


void resetsnooze(){
  if (wakeup.onoff){
    alarm = HIGH; 
    snooze = LOW;
  } 
}


void requestTimefromESP(){
  MySerial.println("Requesting time");
  StaticJsonBuffer<60> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
    root["request"] = 1;
  char output[60];
  root.printTo(output,sizeof(output));
  root.printTo(Serial);
  Serial.println();
} 


void toggle_Yellow_LED() {
  if (timeStatus() == timeSet) digitalWrite(Yellow_LED,!digitalRead(Yellow_LED));
}


void check_time_is_set() {
  if (timeStatus() != timeSet)requestTimefromESP();
}

