/*
 * monine etal
 */

TaskHandle_t DHTt;
TaskHandle_t USt;
TaskHandle_t WLt;
TaskHandle_t BLYNKt;
TaskHandle_t HBt;

#define BLYNK_TEMPLATE_ID "TMPL6jQUaIRjD"
#define BLYNK_TEMPLATE_NAME "Molina et al"
#define BLYNK_AUTH_TOKEN "Lc64RiO00coApcIE5IctK1cB7VzXHZuV"
#define BLYNK_PRINT Serial

#include "soc/rtc_io_reg.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>
#include <ElegantOTA.h>
#include <ESPDash.h>

Preferences preferences;

String ssid = "OPPO A5s";
String pass = "12345678";

AsyncWebServer server(80);
ESPDash dashboard(&server); 

Card temperature(&dashboard, TEMPERATURE_CARD, "Temperature", "°C");
Card humidity(&dashboard, HUMIDITY_CARD, "Humidity", "%");
Card waterlevel(&dashboard, PROGRESS_CARD, "Water Level", "", 0, 255);
Card tempthresh(&dashboard, SLIDER_CARD, "Temperature Threshold", "", 0, 50);
Card humthresh(&dashboard, SLIDER_CARD, "Humidity Threshold", "", 0, 100);
Card seeder(&dashboard, BUTTON_CARD, "Seed water");
Card water(&dashboard, BUTTON_CARD, "water");

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

// HTML page for Wi-Fi configuration
const char* wifiConfigPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Config</title>
</head>
<body>
  <h2>WiFi Configuration</h2>
  <form action="/save" method="POST">
    <label for="ssid">SSID:</label><br>
    <input type="text" id="ssid" name="ssid" required><br><br>
    <label for="pass">Password:</label><br>
    <input type="text" id="pass" name="pass" required><br><br>
    <input type="submit" value="Save">
  </form>
</body>
</html>
)rawliteral";

void connectToWiFi(const char* ssid, const char* password) {
  Serial.printf("Connecting to Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, pass);

  int maxRetries = 20;
  while (WiFi.status() != WL_CONNECTED && maxRetries-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected! IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

int forwarddel = 100;//in msec
int forwardkddel = 1000; //kalaykay down delay forward
int fsek = 1000;// forward seed
int lturndel = 3100;//left turn delay
int rturndel = 3200;//right turn delay

int fk = 0;//final kposition
int ink = 45; //initial kposition
int sdd = 50;//seed dispenser down
int sdr = 0;//seed dispenser retract

int snd = 0;
int t = 0;
int h = 0;
int sd = 0; //seed dispenser
int hb = 0;//hb
int wp = 0; //water pump
int mc = 0; //manual control
int tt = 0; //temp threshold
int ht = 0; //humidity threshold
int wlp[] = {12, 16, 5, 18, 19, 21, 22};
int wls = 0; //water level sum
int wlt = 0; //water level total
int lp = 0;//last position
int x = 0;
int y = 0;
//int up = 0; //ultrasonic position
//int r = 0; //right
int f = 0; //front
//int l = 0; //left

#include <DHT11.h>
DHT11 dht11(2);

#include <NewPing.h>
#define MD 100
NewPing sonar(33, 34, MD);//te

#include <ESP32Servo.h>
Servo sservo;//header
Servo kservo;

void setup() {
  Serial.begin(115200);
  // Disable DAC1
  REG_CLR_BIT(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC);
  REG_SET_BIT(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC_XPD_FORCE);
  // Disable DAC2
  REG_CLR_BIT(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC);
  REG_SET_BIT(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC_XPD_FORCE);
  preferences.begin("my-app", false);
  sd = preferences.getInt("sd", 0);
  wp = preferences.getInt("wp", 0);
  tt = preferences.getInt("tt", 0);
  ht = preferences.getInt("ht", 0);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");  
  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  sservo.setPeriodHertz(50);
  sservo.attach(17, 0, 2000);
  kservo.setPeriodHertz(50);
  kservo.attach(23, 0, 2000);
  //left
  //ledcAttach(4, 100, 8); //PWMA
  pinMode(4, OUTPUT);
  pinMode(15, OUTPUT);//AN1
  pinMode(13, OUTPUT);//AN2
  
  //right
  //ledcAttach(26, 100, 8); //PWMB
  pinMode(26, OUTPUT);
  pinMode(14, OUTPUT);//BN1
  pinMode(27, OUTPUT);//BN2

  //analogWrite(4, 200);
  //analogWrite(26, 200);
  digitalWrite(15, HIGH);
  digitalWrite(13, LOW);
  digitalWrite(14, HIGH);
  digitalWrite(27, LOW);

  pinMode(32, OUTPUT);//RELAY

  //waterlevel
  //pinMode(15, INPUT_PULLUP);
  //pinMode(23, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  //pinMode(17, INPUT_PULLUP);  
  pinMode(5, INPUT_PULLUP);
  pinMode(18, INPUT_PULLUP);
  pinMode(19, INPUT_PULLUP);
  pinMode(21, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP);
//int wlp[] = {23, 12, 16, 5, 18, 19, 21, 22};
  
  xTaskCreatePinnedToCore(
    DHTc,     // Task function
    "DHT",      // Name of the task
    2000,          // Stack size (in words, not bytes)
    NULL,          // Task input parameter
    2,             // Priority of the task (higher priority)
    &DHTt,        // Task handle
    1);            // Core to run the task on (Core 1)

  xTaskCreatePinnedToCore(
    USc,     // Task function
    "US",      // Name of the task
    2000,          // Stack size (in words, not bytes)
    NULL,          // Task input parameter
    2,             // Priority of the task (higher priority)
    &USt,        // Task handle
    0);            // Core to run the task on (Core 0)

  xTaskCreatePinnedToCore(
    WLc,     // Task function
    "WL",      // Name of the task
    2000,          // Stack size (in words, not bytes)
    NULL,          // Task input parameter
    2,             // Priority of the task (higher priority)
    &WLt,        // Task handle
    0);            // Core to run the task on (Core 0)

    xTaskCreatePinnedToCore(
    BLYNKc,     // Task function
    "BLYNK",      // Name of the task
    4000,          // Stack size (in words, not bytes)
    NULL,          // Task input parameter
    2,             // Priority of the task (higher priority)
    &BLYNKt,        // Task handle
    0);            // Core to run the task on (Core 0)

    xTaskCreatePinnedToCore(
    HBc,     // Task function
    "HB",      // Name of the task
    4000,          // Stack size (in words, not bytes)
    NULL,          // Task input parameter
    2,             // Priority of the task (higher priority)
    &HBt,        // Task handle
    0);            // Core to run the task on (Core 0)

  WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid.c_str(), pass.c_str());
  } else {
    WiFi.softAP("Molina_et_al");
    Serial.println("start ap");
    // Route to serve the Wi-Fi configuration page
    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", wifiConfigPage);
    });

    // Route to save Wi-Fi credentials
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
      String ssid, pass;

      if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
        ssid = request->getParam("ssid", true)->value();
        pass = request->getParam("pass", true)->value();

        // Save credentials
        preferences.putString("ssid", ssid);
        preferences.putString("pass", pass);

        // Respond to client
        request->send(200, "text/html", "<h2>Credentials Saved! Restarting...</h2>");

        // Restart ESP32
        delay(1000);
        ESP.restart();
      } else {
        request->send(400, "text/html", "<h2>Missing SSID or Password!</h2>");
      }
    });

  }

  /* Attach Slider Callback */
  tempthresh.attachCallback([&](int value){
    /* Print our new slider value received from dashboard */
    tt = value;
    preferences.putInt("tt", value);
    /* Make sure we update our slider's value and send update to dashboard */
    tempthresh.update(value);
    dashboard.sendUpdates();
  });
  tempthresh.update(tt);

  /* Attach Slider Callback */
  humthresh.attachCallback([&](int value){
    /* Print our new slider value received from dashboard */
    ht = value;
    preferences.putInt("ht", value);
    /* Make sure we update our slider's value and send update to dashboard */
    humthresh.update(value);
    dashboard.sendUpdates();
  });
  humthresh.update(ht);

  seeder.attachCallback([&](int value){
  if(value==1){
    mc = 0l;
  }
  sd = value;
  seeder.update(value);
  dashboard.sendUpdates();
});
  seeder.update(sd);

  water.attachCallback([&](int value){
  if(value==1){
    mc = 0l;
  }
  wp = value;
  water.update(value);
  dashboard.sendUpdates();
});
  water.update(sd);

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();
  Serial.println("HTTP server started");
 }
 

BLYNK_CONNECTED() { 
  // Called when hardware is connected to Blynk.Cloud  
  // Request Blynk server to re-send latest values for all pins
  Blynk.syncAll();
}

BLYNK_WRITE(V3){
  tt = param.asInt();
  preferences.putInt("tt", tt);
}

BLYNK_WRITE(V4){
  ht = param.asInt();
  preferences.putInt("ht", ht);
}

BLYNK_WRITE(V5){
  sd = param.asInt();
  preferences.putInt("sd", sd);
}

BLYNK_WRITE(V6){
  wp = param.asInt();
  preferences.putInt("wp", wp);
}

BLYNK_WRITE(V9){
  mc = param.asInt();
  Serial.print(mc);
}

BLYNK_WRITE(V7) {   
  x = param.asInt();  // For datastream of data type integer
  //double x = param.asDouble();    // For datastream of data type double
  Serial.print("V7: x =");
  Serial.println(x);
}

BLYNK_WRITE(V8) {   
  y = param.asInt();  // For datastream of data type integer
  //double y = param.asDouble();    // For datastream of data type double
  Serial.print("V8: y =");
  Serial.println(y);
}
void BLYNKc(void * pvParameters) {
  for (;;) {
    Blynk.virtualWrite(V0, t);
    Blynk.virtualWrite(V1, h);
    Blynk.virtualWrite(V2, wlt);    
    vTaskDelay(30000);
  }
}

void DHTc(void * pvParameters) {
  for (;;) {
    vTaskDelay(2000);
    for(uint8_t  i=0; i<8; i++){
      wls += digitalRead(wlp[i]);
    }
    //Serial.println("water level: ");
    //Serial.println(wls);
    wlt = (100-(14*wls)+random(-2,2));
    wls = 0;
    dht11.readTemperatureHumidity(t, h);
    temperature.update(t);
    humidity.update(h);
    waterlevel.update(wlt);
    dashboard.sendUpdates();

    //Serial.print("Temp: ");
    //Serial.println(t);
    //Serial.print("Humidity: ");
    //Serial.println(h);
  }
}

void WLc(void * pvParameters) {
  for (;;) {
    vTaskDelay(1000);
    for(uint8_t  i=0; i<8; i++){
      wls += digitalRead(wlp[i]);
    }
    //Serial.println("water level: ");
    //Serial.println(wls);
    wlt = (100-(14*wls)+random(-2,2));
    wls = 0;
  }
}

void USc(void * pvParameters) {
  for (;;) {
    snd = sonar.ping_cm();
    if (snd>0){
      f = 1;
    }
    Serial.println(f);
    vTaskDelay(100);
  }
}

void HBc(void * pvParameters) {
  for (;;) {
    vTaskDelay(1);
    //sd=1;
    if(mc==1){
      //Serial.print("motor manual");
      if(y>0){
        digitalWrite(15, HIGH);
        digitalWrite(13, LOW);
        digitalWrite(14, HIGH);
        digitalWrite(27, LOW);
      }else{
        digitalWrite(15, LOW);
        digitalWrite(13, HIGH);
        digitalWrite(14, LOW);
        digitalWrite(27, HIGH);        
      }
      analogWrite(4, abs(y+x));
      //analogWrite(4, abs(y+x));
      //Serial.println(abs(y+x));
      //analogWrite(26, abs(y+x));
      analogWrite(26, abs(y-x));
    }else if(sd==1){//seeddispenser
      if (f==1){
        if(lp==1){
          right();
          delay(rturndel);
          lp=0;
          f=0;
        }else if(lp==0){
          left();
          delay(lturndel);
          lp=1;
          f=0;
        }

      } else if (f==0){
        kservo.write(ink);
        forward();
        vTaskDelay(forwarddel);
        sto();
        kservo.write(fk);
        vTaskDelay(500);
        forward();
        vTaskDelay(forwardkddel);
        sto();
        kservo.write(ink);
        sservo.write(sdd);
        vTaskDelay(1000);
        sservo.write(sdr);
        forward();
        vTaskDelay(fsek);
        sto();
        digitalWrite(32, HIGH);
        vTaskDelay(5000);
        digitalWrite(32, LOW);
        sto();
      }
    }else if(wp==1){
      if((t>tt)||(h<ht)){
        if (f==0){
          digitalWrite(32, HIGH);
          forward();
          delay(500);
        }else if ((f>0)&&(lp==0)){
          left();
          delay(lturndel);
          lp=1;
          f=0;
        }else if ((f>0)&&(lp==1)){
          right();
          delay(rturndel);
          lp=0;
          f=0;
        }
      }
    }
  }
}

void loop() {
  Blynk.run();
  ElegantOTA.loop();
}

void forward(){
  digitalWrite(15, HIGH);
  digitalWrite(13, LOW);
  digitalWrite(14, HIGH);
  digitalWrite(27, LOW);
  analogWrite(4, 255);
  analogWrite(26, 255);
}

void reverse(){
  digitalWrite(15, LOW);
  digitalWrite(13, HIGH);
  digitalWrite(14, LOW);
  digitalWrite(27, HIGH);
  analogWrite(4, 100);
  analogWrite(26, 100);
}

void left(){
  digitalWrite(15, HIGH);
  digitalWrite(13, LOW);
  digitalWrite(14, HIGH);
  digitalWrite(27, LOW);
  analogWrite(4, 50);
  analogWrite(26, 255);
}

void right(){
  digitalWrite(15, HIGH);
  digitalWrite(13, LOW);
  digitalWrite(14, HIGH);
  digitalWrite(27, LOW);
  analogWrite(4, 255);
  analogWrite(26, 50);
}

void sto(){
  digitalWrite(15, HIGH);
  digitalWrite(13, HIGH);
  digitalWrite(14, HIGH);
  digitalWrite(27, HIGH);
  analogWrite(4, 255);
  analogWrite(26, 255);
}

