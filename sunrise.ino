#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <RtcDS3231.h>

#ifndef STASSID
#define STASSID "XYZ"
#define STAPSK  "XYZ"
#endif


#define LED_PIN 13 // D7
#define LED_COUNT 30

const char* ssid = STASSID;
const char* password = STAPSK;

RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);

// begin with invalid hour
uint8_t alarmHour = 25;
uint8_t alarmMinute;
uint8_t sunriseLengthInMinutes = 15;


uint8_t r;
uint8_t g;
uint8_t b;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


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

void initStrip(){
  strip.begin();          
  strip.show();          
  strip.setBrightness(1);  
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
    server.send(200, "text/plain", "Successfully reset");
  });

  server.onNotFound(handleNotFound);
}

void setAlarm(String time){
  alarmHour = atoi(getValue(time, ':', 0).c_str()); 
  alarmMinute = atoi(getValue(time, ':', 1).c_str());
}

String getValue(String data, char separator, int index)
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
  return nowMinute >= nowMinute + (getCycleLength()*2) && nowMinute < alarmMinute + sunriseLengthInMinutes;
}

uint8_t getCycleLength(){
  return sunriseLengthInMinutes/3;
}

uint8_t getElapsedMinuteInCycle(uint8_t nowMinute){
  //hacky method
  uint8_t elapsedTime = nowMinute-alarmMinute+1;
  uint8_t cycleLength = getCycleLength();
  while (elapsedTime > cycleLength){
    elapsedTime -= cycleLength;
  }
  return elapsedTime;
}

void firstCycle(uint8_t nowMinute){
  // deep blues and purples -> blue to purple by increasing red
  uint8_t rStart = 55;
  uint8_t gStart = 63;
  uint8_t bStart = 135;
  
  uint8_t rStop = 85;
  uint8_t gStop = 56;
  uint8_t bStop = 135;

  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute);
}
void secondCycle(uint8_t nowMinute){
  // reds and oranges -> purple to red by decreasing blue; red to orange by increasing red and green
  uint8_t rStart = 85;
  uint8_t gStart = 56;
  uint8_t bStart = 135;
  
  uint8_t rStop = 229;
  uint8_t gStop = 108;
  uint8_t bStop = 56;

  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute);
}
void thirdCycle(uint8_t nowMinute){
  // yellows and white -> orange to yellow by increasing green; yellow to white by increasing all
  uint8_t rStart = 229;
  uint8_t gStart = 108;
  uint8_t bStart = 56;
  
  uint8_t rStop = 250;
  uint8_t gStop = 255;
  uint8_t bStop = 90;

  setColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute);
}

void setColor(uint8_t rStart, uint8_t gStart, uint8_t bStart, uint8_t rStop, uint8_t gStop, uint8_t bStop, uint8_t nowMinute){
  uint32_t color = getColor(rStart, gStart, bStart, rStop, gStop, bStop, nowMinute);
  for (int i=0; i < strip.numPixels(); i++) {
     strip.setPixelColor(i , color);
     strip.show();
  }
}

uint32_t getColor(uint8_t rStart, uint8_t gStart, uint8_t bStart, uint8_t rStop, uint8_t gStop, uint8_t bStop, uint8_t nowMinute){
  uint8_t cycleLength = getCycleLength();
  uint8_t rCurr = rStart + (getElapsedMinuteInCycle(nowMinute) * (rStop-rStart)/cycleLength);
  rCurr = rCurr >= 0 ? rCurr : 0;
  uint8_t gCurr = gStart + (getElapsedMinuteInCycle(nowMinute) * (gStop-gStart)/cycleLength);
  gCurr = gCurr >= 0 ? gCurr : 0;
  uint8_t bCurr = bStart + (getElapsedMinuteInCycle(nowMinute) * (bStop-bStart)/cycleLength);
  bCurr = bCurr >= 0 ? bCurr : 0;
//  Serial.print("Minute: ");
//  Serial.print(getElapsedMinuteInCycle(nowMinute));
//  Serial.print(" R: ");
//  Serial.print(rCurr);
//  Serial.print(" G: ");
//  Serial.print(gCurr);
//  Serial.print(" B: ");
//  Serial.println(bCurr);
  r = rCurr;
  g = gCurr;
  b = bCurr;
  return strip.Color(rCurr, gCurr, bCurr);
}

void setup(void) {
  Serial.begin(115200);
  initWifi();
  initStrip();
  initRtc();
  initEndpoints();  

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  RtcDateTime now = Rtc.GetDateTime();
  uint8_t nowMinute = now.Minute();
  if (alarmHour != 25 && 
      now.Hour() == alarmHour && 
      (nowMinute >= alarmMinute && nowMinute < alarmMinute + sunriseLengthInMinutes)){
    beginSunrise(nowMinute);
  }
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
    html += "</h1> <h3> Color: (";
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

void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by adn amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}
