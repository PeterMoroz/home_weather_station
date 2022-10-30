#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <ESP8266WebServer.h>


#define EEPROM_SIZE 512
#define SSID_SIZE 32
#define PASS_SIZE 64

#define DEVICE_NAME "ESP8266"
#define AP_SSID DEVICE_NAME

#define READ_SENSOR_INTERVAL_MS 4000

ESP8266WebServer webServer(80);

float temperature = 0.f;
float humidity = 0.f;


void handle_init() {
  Serial.println("- init");
  // TO DO: 
  // 1. read content from FS 
  // 2. use char buffer and sprintf instead of String
  String content;
  content = "<!DOCTYPE HTML>\r\n<html>Setup WiFi credentials to connect to WiFi network";
  content += "<form method='get' action='set_wifi_credentials'><label>SSID: </label><input name='ssid' length=32><br>PSK: <input name='pass' length=64><br><input type='submit'></form>";
  content += "</html>";
  webServer.send(200, "text/html", content);  
}

void handle_sensors() {
  // TO DO: calculate the length of the result string and reduce buffer size
  char page[1024];
  sprintf(page, "<html><head><title>%s Sensors</title>" \
                "<meta name='viewport' content=device-width, initial-scale=1.0'>" \
                "<style> h1 {text-align:center; } td {font-size: 50%%; padding-top: 30px;}" \
                ".temp { font-size:150%%; color: #FF0000; } .press { font-size:150%%; color: #FF0000; }" \
                "</style></head><body> <h1>%s Sensors</h1><div id='div1'><table>" \
                "<tr><td>Temperature</td><td class='temp'>%.2f</td></tr>" \
                "<tr><td>Pressure</td><td class='press'>%.2f</td></tr></div><body></html",
                DEVICE_NAME, DEVICE_NAME, temperature, humidity);
  webServer.send(200, "text/html", page);
}

void handle_set_wifi_credentials() {
  Serial.println("- set_wifi_credentials");
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
      content = "{\"Success\":\"saved to EEPROM. going to reset.\"}";
      statusCode = 200;
    } else {
      content = "{\"Failure\":\"could not save to EEPROM.\"}";
      statusCode = 500;
    }

  } else {
    content = "{\"Error\":\"404 not found\"}";
    statusCode = 404;
  }

  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(statusCode, "application/json", content);

  if (success) {
    ESP.reset();
  }
}


void readSensor() {
  static unsigned long lastMs;
  unsigned long currMs = millis();

  if (currMs - lastMs > READ_SENSOR_INTERVAL_MS) {
    lastMs = currMs;

  }  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  WiFi.disconnect();  


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

  webServer.on("/init", handle_init);
  webServer.on("/sensors", handle_sensors);
  webServer.on("/set_wifi_credentials", handle_set_wifi_credentials);  
  
  webServer.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  webServer.handleClient();
  readSensor();
}
