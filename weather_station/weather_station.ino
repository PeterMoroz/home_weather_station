#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include <ESP8266WebServer.h>
#include <PubSubClient.h>

#include <Wire.h>
// #include <CCS811.h>
#include "ccs811.h"
#include "DHTesp.h"

#include "sensors_page.h"

#define EEPROM_SIZE 512

#define SSID_SIZE 32
#define PASS_SIZE 64

#define DEVICE_ID_SIZE 10


#define DEVICE_NAME "ESP8266"
#define AP_SSID DEVICE_NAME

#define READ_SENSOR_INTERVAL_MS 1000

ESP8266WebServer webServer(80);
CCS811 ccs811(-1, 0x5A);
DHTesp dht;

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

float temperature = 0.f;
float humidity = 0.f;
float co2 = 0.f;
float tvoc = 0.f;


void handle_setup() {
  Serial.println("- setup");
  // TO DO:
  // 1. read content from FS
  // 2. use char buffer and sprintf instead of String
  String content;
  content = "<!DOCTYPE HTML>\r\n<html>Setup ";
  content += "<h1>WiFi settings</h1><br>";
  content += "<form method='get' action='setup_wifi'><label>SSID: </label><input name='ssid' length=32><br>PSK: <input name='pass' length=64><br><input type='submit'></form><br>";
  content += "<h1>MQTT settings</h1><br>";
  content += "<form method='get' action='setup_mqtt'><label>Broker: </label><input name='broker' length=64><br>port: <input name='port' length=5><br>";
  content += "<label>Device Id:</label><input name ='device-id' length=16><br><input type='submit'> </form> </html>";
  webServer.send(200, "text/html", content);
}


void handle_sensors() {
  String s = sensors_page;
  webServer.send(200, "text/html", s);
}

void handle_measurements() {
  Serial.println("- measurements");
  // TO DO: shrink buffer size (~96 should be enough)
  char buff[128];
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

//#define SSID_SIZE 32
//#define PASS_SIZE 64
//
//#define URL_SIZE 64
//#define PORT_SIZE 5
//#define DEVICE_ID_SIZE 16

void read_mqtt_endpoint(IPAddress& ipaddr, uint16_t& port) {
  uint8_t endpoint[6] = { 0 };
  // ssid += char(EEPROM.read(eepromIdx));

  int i = SSID_SIZE + PASS_SIZE, j = 0;
  for (; i < SSID_SIZE + PASS_SIZE + 6; i++, j++) {
    endpoint[j] = EEPROM.read(i);
  }

  ipaddr = IPAddress(endpoint[0], endpoint[1], endpoint[2], endpoint[3]);
  port = endpoint[4];
  port <<= 8;
  port += endpoint[5];
}


bool mqtt_reconnect(const IPAddress& mqttServer, uint16_t mqttPort, uint8_t attempts = 10) {
  pubSubClient.disconnect();
  Serial.printf("Set MQTT : server = %u.%u.%u.%u, port = %u\n", mqttServer[0], mqttServer[1], mqttServer[2], mqttServer[3], mqttPort);
  pubSubClient.setServer(mqttServer, mqttPort);

  while (!pubSubClient.connected() && attempts != 0) {
    Serial.println("Connect MQTT client...");
    if (!pubSubClient.connect(DEVICE_NAME)) {
      Serial.printf("connect failed (errcode %d)\n", pubSubClient.state());
      delay(1000);
      attempts--;  
    }
  }

  return pubSubClient.connected();
}

bool mqtt_connect() {
  IPAddress addr;
  uint16_t port;
  Serial.println("Connect to MQTT broker.");
  read_mqtt_endpoint(addr, port);
  Serial.printf("Endpoint - %s:%u\n", addr, port);
  return mqtt_reconnect(addr, port);
}

void handle_setup_mqtt() {
  Serial.println("- setup_mqtt");
  String broker = webServer.arg("broker");
  String port = webServer.arg("port");
  String deviceId = webServer.arg("device-id");

  Serial.printf("broker: %s, port: %u, deviceId: %s\n", broker.c_str(), atoi(port.c_str()), deviceId.c_str());

  String mqttServer;
  uint16_t mqttPort = 1883;

  bool needReconnect = broker.length() > 0 && port.length() > 0;

  if (broker.length() > 0 && port.length() > 0) {
    mqttServer = broker;
    mqttPort = atoi(port.c_str());
  } else if (broker.length() > 0) {
    mqttServer = broker;
  }

  if (needReconnect) { 
    IPAddress brokerIP;
    if (!WiFi.hostByName(broker.c_str(), brokerIP)) {
      Serial.println("WiFi.hostByName failed!");
      webServer.sendHeader("Access-Control-Allow-Origin", "*");
      webServer.send(400, "text/plain", "Could not resolve MQTT broker IP.");
      return ;
    }
  
    Serial.printf("Broker IP: %u.%u.%u.%u\n", brokerIP[0], brokerIP[1], brokerIP[2], brokerIP[3]);

    IPAddress addr;
    uint16_t port;
    read_mqtt_endpoint(addr, port);
    if (addr == brokerIP && port == mqttPort) {
      needReconnect = false;
    } else {
      if (mqtt_reconnect(brokerIP, mqttPort, 2)) {
        if (addr != brokerIP) {
          int i = SSID_SIZE + PASS_SIZE;
          EEPROM.put(i, brokerIP[0]);
          EEPROM.put(i + 1, brokerIP[1]);
          EEPROM.put(i + 2, brokerIP[2]);
          EEPROM.put(i + 3, brokerIP[3]);
        }

        if (port != mqttPort) {
          int i = SSID_SIZE + PASS_SIZE + 4;
          EEPROM.put(i, (uint8_t)(mqttPort >> 8));
          EEPROM.put(i + 1, (uint8_t)(mqttPort & 0xFF));
        }

        if (!EEPROM.commit()) {
          webServer.sendHeader("Access-Control-Allow-Origin", "*");
          webServer.send(500, "text/plain", "Could not save parameters to EERPOM.");
          return ;
        }
        
      } else {
        webServer.sendHeader("Access-Control-Allow-Origin", "*");
        webServer.send(400, "text/plain", "Could not connect to MQTT broker.");
        return ;
      }
    }
  }

  webServer.sendHeader("Access-Control-Allow-Origin", "*");
  webServer.send(200, "text/plain", "Settings changed.");

  // TO DO: reconnect if needed
}


void readSensors() {
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

void publishSensors() {
  char topic[32];
  char message[92];
  sprintf(topic, "%s/%s", DEVICE_NAME, "CCS811");
  sprintf(message, "temperature: %0.2f deg, humidity: %0.2f %%, CO2: %0.2f ppm, TVOC: %0.2f ppb", temperature, humidity, co2, tvoc);

  if (!pubSubClient.publish(topic, message)) {
    Serial.println("Could not publish message");
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

//  String mqttBroker("test.mosquitto.org");
//  uint16_t mqttPort = 1883;
//
//  Serial.println("Setup MQTT client");
//  pubSubClient.setServer(mqttBroker.c_str(), mqttPort);

  if (!mqtt_connect()) {
    Serial.println("Could not connect to MQTT broker.");
    return;
  }

  Serial.println("Start web-sesrver");

  webServer.on("/setup", handle_setup);
  webServer.on("/sensors", handle_sensors);
  webServer.on("/setup_mqtt", handle_setup_mqtt);
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
  // publishSensors();
  delay(1000);
  webServer.handleClient();
//  if (!pubSubClient.loop()) {
//    mqtt_connect();
//  }

//  bool c = pubSubClient.connected();
//  if (!c) {
//    Serial.println("MQTT client disconnected.");
//  }
//  
//  bool l = pubSubClient.loop();
//  if (!l) {
//    Serial.println("MQTT lost connection.");
//  }
}
