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


void beginSunrise(){
  Serial.println("Starting Sunrise");
    for (int i=0; i < strip.numPixels(); i++) {
       strip.setPixelColor(i , strip.Color(255, 0, 0));
       strip.show();
      }
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
  Serial.print(alarmHour);
  Serial.print(":");
  Serial.println(alarmMinute);
  if (alarmHour != 25 && now.Hour() == alarmHour && now.Minute() == alarmMinute){ 
    beginSunrise();
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
    html += "</h1>\
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
