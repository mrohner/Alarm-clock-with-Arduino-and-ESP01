/*
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic"
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// Update these with values suitable for your network.
const char* ssid = "xxxxxxxxxxxx";
const char* password = "xxxxxxxxxxxxx";
const char* mqtt_server = "192.168.178.59";
const char* mqtt_username = "mqtt";
const char* mqtt_password = "xxxx";
char* InTopic = "domoticz/out"; //subscribe to topic to be notified about
char* TimeTopic = "Time"; //subscribe to topic to be notified about
char* OutTopic = "domoticz/in"; 
char* OutTopic1 = "Request/Time"; 
const int ALARM_IDX = 50;
const int DHT11_SENSOR_IDX = 48;
const int MOVEMENT_IDX = 49;
float temp;
float hum;
int movement;
int timerequest = 0;
int nvalue;
int svalue;
int alarm = 0;

WiFiClient espClient;
PubSubClient client(espClient);
int value = 0;
int counter = 0;

// Timing
volatile int WDTCount = 0;
bool SyncNecessary = true;
time_t prevDisplay = 0; // when the digital clock was displayed
time_t t;
time_t lastMsg = 0;
const int INTERVAL = 600; // 10 mins = 600

void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);
  
   // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("ESP75-Bedroom");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"xxxx");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}


WiFiClient WiFiclient;
const int httpPort = 8080;


void loop() {
  if (!client.connected()) reconnect();
  if (now() > (t + 82800)) setReadyForClockUpdate();
  ArduinoOTA.handle();
  client.loop();
  yield();
  WDTCount = 0;
  time_t jetzt = now();
  getResponseFromNano();
  if (jetzt - lastMsg > INTERVAL) {
    lastMsg = jetzt;
    requestfromNano("update","");
   }
}


void setup_wifi() {
  WiFi.mode(WIFI_STA); 
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("BedroomClient",mqtt_username,mqtt_password)) {
      Serial.println("connected");
      counter = 0;
      // Once connected, publish an announcement...
      // ... and resubscribe
      client.subscribe(InTopic);
      delay(10);
      client.subscribe(TimeTopic);
      client.publish(OutTopic1,"1");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in .3 seconds");
      ++counter;
      if (counter > 180) ESP.reset();
      // Wait 0.3 seconds before retrying
      ArduinoOTA.handle();
      delay(300);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonBuffer<500> jsonBuffer;
  //const char* json = "{\"Battery\":255,\"RSSI\":12,\"dtype\":\"Light/Switch\",\"id\":\"0001406C\",\"idx\":28,\"name\":\"testswi\",\"nvalue\":0,\"stype\":\"Switch\",\"svalue1\":\"0\",\"switchType\":\"Blinds\",\"unit\":1}";
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
    int Battery = root["Battery"]; // 255
    int RSSI = root["RSSI"]; // 12
    const char* dtype = root["dtype"]; // "Light/Switch"
    const char* id = root["id"]; // "0001406C"
    int idx = root["idx"]; // 34
    const char* name = root["name"]; // "testswi"
    int nvalue = root["nvalue"]; // request: 0 = open, 1 = closed
    const char* stype = root["stype"]; // "Switch"
    const char* svalue1 = root["svalue1"]; // "0"
    const char* switchType = root["switchType"]; // "Blinds"
    int unit = root["unit"]; // 1
    int day_from_system = root["day"]; // 9
    int month_from_system = root["month"]; // 11
    int year_from_system = root["year"]; // 2017
    int hour_from_system = root["hour"]; // 16
    int minutes_from_system = root["minutes"]; // 29
    int seconds_from_system = root["seconds"]; // 26
   
    String what = String(nvalue);
    if (day_from_system && SyncNecessary || timeStatus() == timeNotSet) {
      setTime(hour_from_system,minutes_from_system,seconds_from_system,day_from_system,month_from_system,year_from_system); // alternative to above, yr is 2 or 4 digit yr
      Serial.println(F("Setting Time"));
      SyncNecessary = false;
      t = now();
      do wait(1);          // wait for clock to be at 0 secs
      while (second() != 0);
      sendTimetoNano();
      }
   if (idx == ALARM_IDX) requestfromNano("alarm",what);
}  


void getResponseFromNano(){
 if (Serial.available()){    
  StaticJsonBuffer<160> jsonBuffer;
  //const char* json = "{\"door\":0,\"CO2\":1023,\"move\":1}";
  JsonObject& root = jsonBuffer.parseObject(Serial);
  temp = root["temp"];
  hum = root["hum"];
  movement = root["move"];
  alarm = root["alarm"];
  timerequest = root["request"];
  if (timerequest) sendTimetoNano();
  else if (temp != 0) {
    publish(MOVEMENT_IDX,movement,0);
    delay(10);
    publish(DHT11_SENSOR_IDX,0,temp,hum);
    delay(10);
    //json.htm?type=command&param=udevice&idx=IDX&nvalue=LEVEL&svalue=TEXT
    //IDX = id of your device (This number can be found in the devices tab in the column "IDX")
    //Level = (0=gray, 1=green, 2=yellow, 3=orange, 4=red)
    //TEXT = Text you want to display
    //if (!alarm) publish(ALARM_IDX,0,0);
    }
  }
}


void requestfromNano(String command, String what){
  StaticJsonBuffer<60> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
    root["command"] = command;
    root["what"] = what;
  char output[60];
  root.printTo(output,sizeof(output));
  root.printTo(Serial);
  Serial.println();
}


void requestfromNano(String command, String h, String m, String s){
  StaticJsonBuffer<160> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
    root["command"] = command;
    root["h"] = h;
    root["m"] = m;
    root["s"] = s;
  char output[160];
  root.printTo(output,sizeof(output));
  root.printTo(Serial);
  Serial.println();
}


//{"idx":29,"nvalue":0,"svalue":"123"}
void publish(int idx, int nvalue, int svalue){ 
  char output[130];
  snprintf_P(output, sizeof(output), PSTR("{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%d\"}"),idx,nvalue,svalue);
  client.publish(OutTopic,output);
}

/*{
  "idx": 11,
  "nvalue": 0,
  "svalue": "23.5;55.0;1"
}
*/
void publish(int idx, int nvalue, float svalue1, float svalue2){ 
  char output[130];
  snprintf_P(output, sizeof(output), PSTR("{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%s;%s;1\"}"),idx,nvalue,String(svalue1,1).c_str(),String(svalue2,1).c_str());
   //sprintf("%s", String(myFloat).c_str());
  client.publish(OutTopic,output);
}


void wait (int ms) {
  for(long i = 0;i <= ms * 30000; i++) asm ( "nop \n" ); //80kHz Wemos D1
  ArduinoOTA.handle();
  client.loop();
  yield();
  WDTCount = 0;
}


void setReadyForClockUpdate() {
  SyncNecessary = true;  
}


void sendTimetoNano() {
  String parsedHours = String(hour());
  String parsedMinutes = String(minute());
  String parsedSeconds = String(second());
  requestfromNano("time",parsedHours,parsedMinutes,parsedSeconds);
}

