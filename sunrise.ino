#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <RtcDS3231.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#ifndef STASSID
#define STASSID "XYZ"
#define STAPSK  "XYZ"
#endif


#define LED_PIN 13 // D7
#define LED_COUNT 30
#define EEPROM_SIZE 2

const char* ssid = STASSID;
const char* password = STAPSK;
const uint8_t C1_BRIGHTNESS = 20;
const uint8_t C2_BRIGHTNESS = 50;
const uint8_t C3_BRIGHTNESS = 70;
const uint8_t SUNRISE_LENGTH_IN_MINUTES = 15;

RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);

// begin with invalid hour
uint8_t alarmHour = 25;
uint8_t alarmMinute;

uint8_t r;
uint8_t g;
uint8_t b;
uint8_t cycle;
boolean off = true;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup(void) {
  Serial.begin(115200);
  initEEPROM();
  initWifi();
  initOTA();
  initStrip();
  initRtc();
  initEndpoints();  

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  ArduinoOTA.handle();
  server.handleClient();
  RtcDateTime now = Rtc.GetDateTime();
  uint8_t nowMinute = now.Minute();
  if (alarmHour != 25 && 
      now.Hour() == alarmHour && 
      (nowMinute >= alarmMinute && nowMinute < alarmMinute + SUNRISE_LENGTH_IN_MINUTES + 1)){
    beginSunrise(nowMinute);
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void initEEPROM(){
  EEPROM.begin(EEPROM_SIZE);
  alarmHour = EEPROM.read(0);
  alarmMinute = EEPROM.read(1);
}
 
void initStrip(){
  strip.begin();  
  strip.setBrightness(C1_BRIGHTNESS);
  strip.clear();
  strip.show();   
}

void initWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initRtc(){
  Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    if (!Rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }  
}

void initEndpoints(){
  server.on("/", [](){
      server.send(200, "text/html", buildHomePage());
   });

  server.on("/time", [](){
    server.send(200, "text/html", buildTimePage());
  });

  server.on("/current", HTTP_GET, [](){
    String response;
    if (alarmHour == 25){
      response = "No alarm set";
    }
    else{
      response = String(alarmHour) + ":" + String(alarmMinute);
    }
    server.send(200, "text/plain", response);
  });

  server.on("/set", HTTP_GET, [](){
    server.send(200, "text/html", buildSetPage());
  });

  server.on("/set", HTTP_POST, [](){
    setAlarm(server.arg("time"));
    server.send(200, "text/plain", String(server.arg("time")));
  });

  server.on("/reset", HTTP_POST, [](){
    setAlarm("25:00");
    setDebugValues(0,0,0,0,true);
    server.send(200, "text/plain", "Successfully reset");
  });

  server.onNotFound(handleNotFound);
}

void initOTA(){
  ArduinoOTA.setHostname("esp8266");
  ArduinoOTA.setPasswordHash("feed5d47c860f422712ac902a89865db");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setAlarm(String time){
  alarmHour = atoi(getValueFromArg(time, ':', 0).c_str());
  alarmMinute = atoi(getValueFromArg(time, ':', 1).c_str());
  EEPROM.write(0, alarmHour); 
  EEPROM.write(1, alarmMinute);
  EEPROM.commit();
}

String getValueFromArg(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void beginSunrise(uint8_t nowMinute){
  if (isFirstCycle(nowMinute)){
    firstCycle(nowMinute);
  }
  else if (isSecondCycle(nowMinute)){
    secondCycle(nowMinute);  
  }
  else if (isThirdCycle(nowMinute)){
    thirdCycle(nowMinute);  
  }
  else{
    setDebugValues(0,0,0,0,true);
    strip.clear();
    strip.show();
  }
}

bool isFirstCycle(uint8_t nowMinute){
  // Example
  // [30-35)
  return nowMinute >= alarmMinute && nowMinute < alarmMinute + getCycleLength();
}

bool isSecondCycle(uint8_t nowMinute){
  // Example
  // [35-40)
  return nowMinute >= alarmMinute + getCycleLength() && nowMinute < alarmMinute + (getCycleLength()*2);
}

bool isThirdCycle(uint8_t nowMinute){
  // Example
  // [40-45)
  return nowMinute >= alarmMinute + (getCycleLength()*2) && nowMinute < alarmMinute + SUNRISE_LENGTH_IN_MINUTES;
}

uint8_t getCycleLength(){
  return SUNRISE_LENGTH_IN_MINUTES/3;
}

uint8_t getElapsedMinuteInCycle(uint8_t nowMinute){
  // Example: Alarm Time 1:59; Current Time 2:02
  // 2 - 59 = - 57 (bad) -> 60+2 - 59 = 3
  nowMinute = nowMinute < alarmMinute ? nowMinute + 60 : nowMinute;
  // Example: Alarm at 1:06; Current Time: 1:18
  // 18-6-(5*(3-1))-1 = 12-10-1 = 1
  return nowMinute - alarmMinute - (getCycleLength()*(cycle-1))-1;
}

void firstCycle(uint8_t nowMinute){
  // deep blues and purples -> blue to purple by increasing red
  uint8_t rStart = 50;
  uint8_t gStart = 50;
  uint8_t bStart = 205;
  
  uint8_t rStop = 139;
  uint8_t gStop = 0;
  uint8_t bStop = 139;

  if(strip.getBrightness() != C1_BRIGHTNESS){
    strip.setBrightness(C1_BRIGHTNESS);   
  }
  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute, 1);
}
void secondCycle(uint8_t nowMinute){
  // reds and oranges -> purple to red by decreasing blue; red to orange by increasing red and green
  uint8_t rStart = 150;
  uint8_t gStart = 0;
  uint8_t bStart = 75;
  
  uint8_t rStop = 245;
  uint8_t gStop = 125;
  uint8_t bStop = 0;

  if(strip.getBrightness() != C2_BRIGHTNESS){
    strip.setBrightness(C2_BRIGHTNESS);   
  }
  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute, 2);
}
void thirdCycle(uint8_t nowMinute){
  // yellows and white -> orange to yellow by increasing green; yellow to white by increasing all
  uint8_t rStart = 235;
  uint8_t gStart = 140;
  uint8_t bStart = 00;
  
  uint8_t rStop = 255;
  uint8_t gStop = 255;
  uint8_t bStop = 125;

  if(strip.getBrightness() != C3_BRIGHTNESS){
    strip.setBrightness(C3_BRIGHTNESS);   
  }
  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute, 3);
}

void setColor(uint8_t rStart, uint8_t gStart, uint8_t bStart, uint8_t rStop, uint8_t gStop, uint8_t bStop, uint8_t nowMinute, uint8_t cycleNumber){
  uint32_t color = getColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute, cycleNumber);
   strip.fill(color);
   strip.show();
}

uint32_t getColor(uint8_t rStart, uint8_t gStart, uint8_t bStart, uint8_t rStop, uint8_t gStop, uint8_t bStop, uint8_t nowMinute, uint8_t cycleNumber){
  uint8_t cycleLength = getCycleLength();
  uint8_t rCurr = rStart + (getElapsedMinuteInCycle(nowMinute) * (rStop-rStart)/cycleLength);
  rCurr = rCurr > 0 ? rCurr : 0;
  uint8_t gCurr = gStart + (getElapsedMinuteInCycle(nowMinute) * (gStop-gStart)/cycleLength);
  gCurr = gCurr > 0 ? gCurr : 0;
  uint8_t bCurr = bStart + (getElapsedMinuteInCycle(nowMinute) * (bStop-bStart)/cycleLength);
  bCurr = bCurr > 0 ? bCurr : 0;
  setDebugValues(rCurr, gCurr, bCurr, cycleNumber, false);
  return strip.Color(rCurr, gCurr, bCurr);
}

void setDebugValues(uint8_t rVal, uint8_t gVal, uint8_t bVal, uint8_t cycleVal, bool offVal){
  r = rVal;
  g = gVal;
  b = bVal;
  cycle = cycleVal;
  off = offVal;  
}

String buildSetPage(){
  return "<html>\
  <head>\
    <title>Sunrise Time</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; font-size: 30px;}\
      #reset {background-color: #f44336; color: white}\
      input {font-size: 25px;}\
    </style>\
  </head>\
  <body>\
    <h1>Sunrise Time</h1>\
    <form action='/set' method='post'>\
      <input type='time' name='time' id='time' required>\
      <input type='submit' value='Set' />\
    </form>\
    <form action='/reset' method='post'>\
      <input id='reset' type='submit' value='Reset' />\
    </form>\
  </body>\
</html>"  ;
}

String buildTimePage(){
  RtcDateTime now = Rtc.GetDateTime();
  String strTime = String(now.Hour()) + ":" + String(now.Minute()) + ":" + String(now.Second());
  String html = "<html>\
  <head>\
    <title>Time</title>\
    <meta http-equiv='refresh' content='1'>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; font-size: 30px; }\
    </style>\
  </head>\
  <body>\
    <h1>The Current Time Is:</h1>\
    <h1>";
    html += strTime;
    if (off){
      html += "<h3>Off</h3>";
    }
    html += "</h1> <h3> Cycle ";
    html += cycle;
    html += "-";
    html += getElapsedMinuteInCycle(Rtc.GetDateTime().Minute());
    html+= ": (";
    html += r;
    html += ",";
    html += g;
    html += ",";
    html += b;
    html += ")";
    html += "</h3>\
      </body>\
    </html>";
  return html;
}

String buildHomePage(){
  String html = "<html>\
  <head>\
    <title>Home</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; font-size: 30px; }\
      a {text-decoration: none; color:#333;}\
      a:visited {color:#333;}\
      a:hover {color:#333;}\
      a:active {color:#333;} \
          </style>\
  </head>\
  <body>\
    <a href='/current'>Current Alarm</a>\
    <br>\
    <a href='/time'>Current Time</a>\
    <br>\
    <a href='/set'>Set Alarm</a>\
    </body>\
  </html>";
  return html;
}
