#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <ESP8266WebServer.h>

#include <Wire.h>
#include <FS.h>

#include "ccs811.h"
#include "DHTesp.h"

#include "sensors_page.h"

#define EEPROM_SIZE 512

#define SSID_SIZE 32
#define PASS_SIZE 64

#define DEVICE_ID_SIZE 10


#define DEVICE_NAME "ESP8266"
#define AP_SSID DEVICE_NAME

#define READ_SENSOR_INTERVAL_MS 2000

ESP8266WebServer webServer(80);
CCS811 ccs811(-1, 0x5A);
DHTesp dht;

WiFiClient wifiClient;

float temperature = 0.f;
float humidity = 0.f;
float co2 = 0.f;
float tvoc = 0.f;


void handle_setup() {
  Serial.println("- setup");
  /* TO DO:
   1. read content from FS
   2. send content from opened file
   
      File file = SPIFFS.open(path, "r");
      size_t n = webServer.streamFile(file, "text/html");
      file.close();  
  */

  /*
  String content;
  content = "<!DOCTYPE HTML>\r\n<html>Setup ";
  content += "<h1>WiFi settings</h1><br>";
  content += "<form method='get' action='setup_wifi'><label>SSID: </label><input name='ssid' length=32><br>PSK: <input name='pass' length=64><br><input type='submit'></form><br></html>";
  webServer.send(200, "text/html", content);
  */

  File file = SPIFFS.open("/setup.html", "r");
  if (file) {
    size_t n = webServer.streamFile(file, "text/html");
    file.close();    
  } else {
    Serial.println("Could not open file 'setup.html'");
    webServer.send(404, "text/plain", "not found setup.html");
  }
  
}

void handle_sensors() {
  /* TO DO:
   1. read content from FS
   2. send content from opened file
   
      File file = SPIFFS.open(path, "r");
      size_t n = webServer.streamFile(file, "text/html");
      file.close();  
  */	
  /* 
  String s = sensors_page;
  webServer.send(200, "text/html", s);
  */

  File file = SPIFFS.open("/sensors.html", "r");
  if (file) {
    size_t n = webServer.streamFile(file, "text/html");
    file.close();    
  } else {
    Serial.println("Could not open file 'sensors.html'");
    webServer.send(404, "text/plain", "not found sensors.html");
  }
}

void handle_measurements() {
  Serial.println("- measurements");
  char buff[96];
  sprintf(buff, "{\"temperature\": %0.2f, \"humidity\": %0.2f, \"tvoc\": %0.2f, \"co2\": %0.2f }", ::temperature, ::humidity, ::tvoc, ::co2);
  webServer.send(200, "text/html", buff);
}

void handle_setup_wifi() {
  Serial.println("- setup_wifi");
  String ssid = webServer.arg("ssid");
  String pass = webServer.arg("pass");
  int statusCode = 0;
  String content;
  bool success = false;

  if (ssid.length() > 0 && pass.length() > 0) {
    Serial.printf("ssid: %s, pass: %s\n", ssid.c_str(), pass.c_str());
    Serial.println("clear EEPROM");
    for (int i = 0; i < (SSID_SIZE + PASS_SIZE); i++)
    {
      EEPROM.write(i, 0);
    }
    EEPROM.end();

    EEPROM.begin(EEPROM_SIZE);

    for (int i = 0; i < ssid.length(); i++) {
      EEPROM.write(i, ssid[i]);
    }
    for (int i = 0; i < pass.length(); i++) {
      EEPROM.write(i + SSID_SIZE, pass[i]);
    }

    success = EEPROM.commit();
    delay(1000);

    if (success) {
      content = "Saved to EEPROM, going to reset";
      statusCode = 200;
    } else {
      content = "Could not save credentials to EEPROM.";
      statusCode = 500;
    }

  } else {
    content = "Empty SSID, password.";
    statusCode = 400;
  }

  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(statusCode, "text/plain", content);

  if (success) {
    ESP.reset();
  }
}

void readSensors() {
  // TO DO: read DHT22 at frequency 0.5 Hz (one reading every two seconds)
  //  static unsigned long lastMs;
  //  unsigned long currMs = millis();
  //
  //  if (currMs - lastMs > READ_SENSOR_INTERVAL_MS) {
  //    lastMs = currMs;
  //
  //  }

  float h = dht.getHumidity();
  float t = dht.getTemperature();

  if (!isnan(h)) {
    ::humidity = h;
  } else {
    Serial.println("DHT22 could not get humidity.");
  }

  if (!isnan(t)) {
    ::temperature = t;
  } else {
    Serial.println("DHT22 could not get temperature.");
  }

  if (!isnan(h) && !isnan(t)) {
    Serial.printf("DHT - temperature: %0.2f deg, humidity: %0.2f %%\n", t, h);
    if (!ccs811.set_envdata_Celsius_percRH(t, h)) {
      Serial.println("CCS811 set environment data failed.");
    }
  }

  uint16_t eco2, etvoc, errstat, raw;
  ccs811.read(&eco2, &etvoc, &errstat, &raw);

  if (errstat == CCS811_ERRSTAT_OK)
  {
//    float co2 = eco2;
//    float tvoc = etvoc;
    ::co2 = eco2;
    ::tvoc = etvoc;
    Serial.printf("CCS811 - CO2: %0.2f ppm, TVOC: %0.2f ppb\n", co2, tvoc);
  }
  else if (errstat == CCS811_ERRSTAT_OK_NODATA)
  {
    Serial.println("CCS811 - waiting for new data.");
  }
  else if (errstat & CCS811_ERRSTAT_I2CFAIL)
  {
    Serial.println("CCS811 - I2C error.");
  }
  else
  {
    Serial.printf("CCS811 - error %04x (%s)\n", errstat, ccs811.errstat_str(errstat));
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.disconnect();

  if (!SPIFFS.begin()) {
    Serial.println("Could not mount FS.");
  }


  EEPROM.begin(EEPROM_SIZE);
  delay(100);

  int eepromIdx = 0;

  // TO DO: use fixed length buffers
  String ssid;
  String pass;

  Serial.println("Read Wi-Fi credentials from EEPROM");
  for (int i = 0; i < SSID_SIZE; i++, eepromIdx++)
  {
    ssid += char(EEPROM.read(eepromIdx));
  }
  for (int i = 0; i < PASS_SIZE; i++, eepromIdx++)
  {
    pass += char(EEPROM.read(eepromIdx));
  }

  bool connected = false;

  Serial.printf("Connect with credentials SSID: %s, pass: %s\n", ssid.c_str(), pass.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  int cnt = 0;
  while (cnt++ < 100) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(100);
  }

  if (!connected) {
    Serial.printf("Start AP '%s'\n", AP_SSID);
    WiFi.softAP(AP_SSID);
    delay(100);
    Serial.print("SoftAP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
  }

  Serial.println("Start web-sesrver");

  webServer.on("/setup", handle_setup);
  webServer.on("/sensors", handle_sensors);
  webServer.on("/setup_wifi", handle_setup_wifi);
  webServer.on("/measurements", handle_measurements);

  webServer.begin();

  dht.setup(16, DHTesp::DHT22);

  Wire.begin();
  ccs811.set_i2cdelay(50);

  if (!ccs811.begin()) {
    Serial.println("CCS811 start failed.");
  }

  Serial.print("CCS811 hardware version: ");
  Serial.println(ccs811.hardware_version(), HEX);
  Serial.print("CCS811 bootloader version: ");
  Serial.println(ccs811.bootloader_version(), HEX);
  Serial.print("CCS811 application version: ");
  Serial.println(ccs811.application_version(), HEX);

  if (!ccs811.start(CCS811_MODE_1SEC)) {
    Serial.println("CCS811 start failed");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  readSensors();
  delay(1000);
  webServer.handleClient();
}
