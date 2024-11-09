#include <EEPROM.h>
#include <FS.h>

#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <PubSubClient.h>

#include <ESP8266mDNS.h>


#include "ccs811.h"
#include "DHTesp.h"

// #define CCS811_SAMPLING_PERIOD 60000
#define CCS811_SAMPLING_PERIOD 1000

#define MAX_MQTT_TOPIC_LENGTH 64

#define DEVICE_NAME "ESP8266-weather-station-v1.0"

CCS811 ccs811;
DHTesp dht;


///////////////////////////////////////////////////////
// WiFi
WiFiClient wifiClient;

String wifiNetworks;

String scanWiFiNetworks() {
  int n = WiFi.scanNetworks();

  String ssid("\"SSID\":["), rssi("\"RSSI\":["), encrypted("\"encrypted\":[");
  for (int i = 0; i < n; i++) {
    if (i != 0) {
      ssid += ", ";
      rssi += ", ";
      encrypted += ", ";
    }
    rssi += (WiFi.RSSI(i));
    ssid += "\"" + WiFi.SSID(i) + "\"";
    encrypted += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "0" : "1";
  }

  ssid += "]";
  rssi += "]";
  encrypted += "]";

  String result = "{ " + rssi + ", " + ssid + ", " + encrypted + " }";
  return result;
}

// #define MAX_WIFI_CONNECT_ATTEMPT 120
#define MAX_WIFI_CONNECT_ATTEMPT 60

bool checkWiFiConnection() {
  int attempt = 0;
  while (attempt++ < MAX_WIFI_CONNECT_ATTEMPT) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
  }
  return false;
}

///////////////////////////////////////////////////////
// Configuration
#define SSID_LENGTH 32
#define PASS_LENGTH 32
#define MQTT_USER_LENGTH 32
#define MQTT_PASS_LENGTH 32
#define MQTT_BROKER_LENGTH 32
#define MQTT_ROOT_TOPIC_LENGTH (MAX_MQTT_TOPIC_LENGTH >> 1)
struct settings {
  char ssid[SSID_LENGTH];
  char password[PASS_LENGTH];
  char mqtt_user[MQTT_USER_LENGTH];
  char mqtt_pass[MQTT_PASS_LENGTH];
  char mqtt_broker[MQTT_BROKER_LENGTH]; // endpoint to connect to broker
  char mqtt_root_topic[MQTT_ROOT_TOPIC_LENGTH]; // the root of all topics
} settings;

void readConfig() {
  EEPROM.get(0, settings);
}

//void clearConfig() {
//  for (int i = 0; i < sizeof(settings); i++) {
//    EEPROM.write(i, 0);  
//  }
//  EEPROM.commit();
//}

void clearWiFiCredentials() {
  const size_t ssid_offset = offsetof(struct settings, ssid);
  const size_t pass_offset = offsetof(struct settings, password);

  size_t idx = ssid_offset;
  for (size_t i = 0; i < sizeof(settings.ssid); i++, idx++) {
    EEPROM.write(idx, 0);
  }

  idx = pass_offset;
  for (size_t i = 0; i < sizeof(settings.password); i++, idx++) {
    EEPROM.write(idx, 0);
  }
  
  EEPROM.commit();
}

bool storeWiFiCredentials(const char *ssid, const char *pass) {
  const size_t ssid_offset = offsetof(struct settings, ssid);
  const size_t pass_offset = offsetof(struct settings, password);

  size_t idx = ssid_offset;
  for (size_t i = 0; i < sizeof(settings.ssid); i++, idx++) {
    EEPROM.write(idx, ssid[i]);
  }

  idx = pass_offset;
  for (size_t i = 0; i < sizeof(settings.password); i++, idx++) {
    EEPROM.write(idx, pass[i]);
  }
  
  const bool success = EEPROM.commit();
  return success;
}

bool storeMQTTParams(const char *user, const char *pass, const char *broker, const char *topic) {
  const size_t user_offset = offsetof(struct settings, mqtt_user);
  const size_t pass_offset = offsetof(struct settings, mqtt_pass);
  const size_t broker_offset = offsetof(struct settings, mqtt_broker);
  const size_t topic_offset = offsetof(struct settings, mqtt_root_topic);

  size_t idx = user_offset;
  for (size_t i = 0; i < sizeof(settings.mqtt_user); i++, idx++) {
    EEPROM.write(idx, user[i]);
  }

  idx = pass_offset;
  for (size_t i = 0; i < sizeof(settings.mqtt_pass); i++, idx++) {
    EEPROM.write(idx, pass[i]);
  }

  idx = broker_offset;
  for (size_t i = 0; i < sizeof(settings.mqtt_broker); i++, idx++) {
    EEPROM.write(idx, broker[i]);
  }

  idx = topic_offset;
  for (size_t i = 0; i < sizeof(settings.mqtt_root_topic); i++, idx++) {
    EEPROM.write(idx, topic[i]);
  }  
  
  const bool success = EEPROM.commit();
  return success;  
}


///////////////////////////////////////////////////////
// global state variables
bool needRestart = false;

int dhtSamplingPeriod = 0x7FFFFFFF;
long lastTimeReadTH = 0;
long lastTimeReadTVOC = 0;


///////////////////////////////////////////////////////
// sensors data

TempAndHumidity th = { 0.f, 0.f }; /* the type 'TempAndHumidity' defined in DHTesp library */

struct {
  uint16_t eco2 = 0;
  uint16_t etvoc = 0;  
} air_quality;


///////////////////////////////////////////////////////
// MQTT
PubSubClient mqttClient(wifiClient);

char temperature_topic[MAX_MQTT_TOPIC_LENGTH];
char humidity_topic[MAX_MQTT_TOPIC_LENGTH];
char co2_topic[MAX_MQTT_TOPIC_LENGTH];
char tvoc_topic[MAX_MQTT_TOPIC_LENGTH];

bool mqttConnect(const char *user, const char *pass, const char *host, const uint16_t port) {

  mqttClient.disconnect();
  
  Serial.println("Connect to MQTT broker...");
  Serial.print("username: ");
  if (user)
    Serial.print(user);
  else
    Serial.print("<none>");

  Serial.print(" password: ");
  if (pass)
    Serial.print(pass);
  else
    Serial.print("<none>");

  Serial.print(" host: ");
  Serial.print(host);

  Serial.print(" port: ");
  Serial.println(port);

  const char* username = strlen(user) != 0 ? user : NULL;
  const char* password = strlen(pass) != 0 ? pass : NULL;

  mqttClient.setServer(host, port);

  uint8_t attempt = 0;
  while (attempt++ < 10) {
    if (mqttClient.connect(DEVICE_NAME, username, password)) {
      return true;
    } else {
      Serial.print("Failed with state ");
      Serial.println(mqttClient.state());
      delay(200);
    }
  }

  return false;
}

bool mqttConnect() {
  char host[MQTT_BROKER_LENGTH];
  uint16_t port = 1883;

  strncpy(host, settings.mqtt_broker, MQTT_BROKER_LENGTH);
  char *p = strchr(host, ':');
  if (p != NULL) {
    *p = '\0';
    port = atoi(p + 1);
  }
  
  return mqttConnect(settings.mqtt_user, settings.mqtt_pass, host, port);
}

void mqttSetupTopics() {
  sprintf(temperature_topic, "%s/temperature", settings.mqtt_root_topic);
  sprintf(humidity_topic, "%s/humidity", settings.mqtt_root_topic);
  sprintf(co2_topic, "%s/co2", settings.mqtt_root_topic);
  sprintf(tvoc_topic, "%s/tvoc", settings.mqtt_root_topic);
}

///////////////////////////////////////////////////////
// WebServer
AsyncWebServer webServer(80);

void initRequestHandlers() {
  // depending on WiFi mode, use different handlers' set
  const WiFiMode_t wifiMode = WiFi.getMode();

  if (wifiMode == WIFI_STA) {
    
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/main.html", "text/html");
    });

    webServer.on("/reset_wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
      // TO DO: make a dedicated page
      clearWiFiCredentials();
      request->send(SPIFFS, "/reboot.html", "text/html");
      needRestart = true;
    });


    // mqtt setup/status handlers
    webServer.on("/mqtt_setup", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/mqtt_setup.html", "text/html");
    });
  
    webServer.on("/setup_mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
      char user[MQTT_USER_LENGTH] = { 0 };
      char pass[MQTT_PASS_LENGTH] = { 0 };
      char broker[MQTT_BROKER_LENGTH] = { 0 };
      char topic[MQTT_ROOT_TOPIC_LENGTH] = { 0 };
  
      const int nparams = request->params();
      for (int i = 0; i < nparams; i++) {
        const AsyncWebParameter *p = request->getParam(i);
  
        Serial.print("i ");
        Serial.print(i);
        Serial.print(", name ");
        Serial.print(p->name().c_str());
        Serial.print(", value ");
        Serial.println(p->value());
        
        if (p->isPost()) {       
          if (!strcmp(p->name().c_str(), "mqtt-user")) {
            p->value().toCharArray(user, MQTT_USER_LENGTH);
          }
          if (!strcmp(p->name().c_str(), "mqtt-pass")) {
            p->value().toCharArray(pass, MQTT_PASS_LENGTH);
          }
          if (!strcmp(p->name().c_str(), "mqtt-broker")) {
            p->value().toCharArray(broker, MQTT_BROKER_LENGTH);
          }
          if (!strcmp(p->name().c_str(), "mqtt-topic")) {
            p->value().toCharArray(topic, MQTT_ROOT_TOPIC_LENGTH);
          }
        }
      }
  
      Serial.print("MQTT user  ");
      Serial.print(user);
      Serial.print(", pass  ");
      Serial.print(pass);
      Serial.print(", broker  ");
      Serial.print(broker);
      Serial.print(", topic  ");
      Serial.println(topic);    
  
      if (strlen(broker) == 0 || strlen(topic) == 0) {
        request->send(400, "text/plain", "The broker connection string and root topic must be specified.");
        return;
      }

      if (storeMQTTParams(user, pass, broker, topic)) {
        // update settings and topics
        strncpy(settings.mqtt_user, user, MQTT_USER_LENGTH);
        strncpy(settings.mqtt_pass, pass, MQTT_PASS_LENGTH);
        strncpy(settings.mqtt_broker, broker, MQTT_BROKER_LENGTH);
        strncpy(settings.mqtt_root_topic, topic, MQTT_ROOT_TOPIC_LENGTH);

        mqttSetupTopics();
                
        mqttClient.disconnect();
        request->send(SPIFFS, "/mqtt_setup_complete.html", "text/html");
      } else {
        request->send(500, "text/plain", "Couldn't store MQTT params into non-volatile memory!");
      }
    });
    
  } else if (wifiMode == WIFI_AP) {
   
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/wifi_setup.html", "text/html");
    });

    webServer.on("/wifi_networks", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/json", wifiNetworks.c_str());
    });

    webServer.on("/setup_wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
      char ssid[SSID_LENGTH] = { 0 };
      char pass[PASS_LENGTH] = { 0 };
      const int nparams = request->params();
      for (int i = 0; i < nparams; i++) {
        const AsyncWebParameter *p = request->getParam(i);
        if (p->isPost()) {
          if (!strcmp(p->name().c_str(), "ssid")) {
            p->value().toCharArray(ssid, SSID_LENGTH);
          }
          if (!strcmp(p->name().c_str(), "pass")) {
            p->value().toCharArray(pass, PASS_LENGTH);
          }
        }
      }

      if (strlen(ssid) == 0 || strlen(pass) == 0) {
        request->send(400, "text/plain", "Missed SSID and/or password.");
        return;
      }

      const bool success = storeWiFiCredentials(ssid, pass);
      if (success) {
        request->send(SPIFFS, "/wifi_setup_complete.html", "text/html");
        needRestart = true;
      } else {
        request->send(500, "text/plain", "Couldn't store wifi credentials into non-volatile memory!");
      }
      
    });

    webServer.on("/direct", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/main.html", "text/html");
    });
  }

  // reboot handler
  webServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/reboot.html", "text/html");
    needRestart = true;
  });

  // sensors data handlers
  webServer.on("/th_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    char data[48] = { 0 };
    sprintf(data, "{ \"temperature\": \"%0.2f\", \"rh\": \"%0.2f\" }", ::th.temperature, ::th.humidity);
    request->send(200, "text/json", data);
  });

  webServer.on("/air_quality_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    char data[48] = { 0 };
    sprintf(data, "{ \"eco2\": \"%u\", \"etvoc\": \"%u\" }", ::air_quality.eco2, ::air_quality.etvoc);
    request->send(200, "text/json", data);
  });

}



///////////////////////////////////////////////////////
// BuzzerController
class BuzzerController {

public:
  explicit BuzzerController(const byte pin);

  void setup();
  void tick();
  void setSoundDuration(unsigned v);
  void setAlarmOn(bool v);

  bool isAlarmOn() const { return alarmOn; }

private:
  void turnOver();
  void setSoundOn(bool v);
  
private:
  bool alarmOn = false;
  bool soundOn = false;
  const byte buzzerPin = 0xFF;
  unsigned halfPeriodOfSound = 2000;
  unsigned long lastTurnTime = 0;  
};

BuzzerController::BuzzerController(const byte pin)
  : buzzerPin(pin)
  {
  }

void BuzzerController::tick() {
  if (alarmOn) {
    if (millis() - lastTurnTime > halfPeriodOfSound) {
      turnOver();
    }
  }
}

void BuzzerController::setup() {  
  pinMode(buzzerPin, OUTPUT);
}

void BuzzerController::setSoundDuration(unsigned v) {
  halfPeriodOfSound = v / 2;
}

void BuzzerController::setAlarmOn(bool v) {
  if (!v) {
    alarmOn = false;
    setSoundOn(false);
  } else {
    alarmOn = true;
    setSoundOn(true);
    lastTurnTime = millis();
  }
}

void BuzzerController::turnOver() {
  if (soundOn)
    setSoundOn(false);
  else
    setSoundOn(true);
  lastTurnTime = millis();
}

void BuzzerController::setSoundOn(bool v) {
  soundOn = v;
  digitalWrite(buzzerPin, v ? HIGH : LOW);
}


BuzzerController buzzerController(D5);


///////////////////////////////////////////////////////
// AlarmController

/* To accumulate CO2 samples during 15 minutes interval,
 * and taking into account that sampling period is 1 s 
 * the required size of buffer is 15 * 60 = 900 
 */
#define CO2_SAMPLES_BUFF_SIZE 900

/* The averaging of CO2 samples is performed each 15 min during 8 hours interval,
 * the required buffer size is (8 * 60) / 15 = 32   
 */
#define CO2_AVERAGES_BUFF_SIZE 32


class AlarmController {
  
public:
  explicit AlarmController(BuzzerController &aBuzzerController);

  void registerMeasurements(uint16_t tvoc, uint16_t co2);

  void setCO2ExposureThreshold(uint16_t v) {
    co2ExposureThreshold = v;
  }

  void setTVOCExposureThreshold(uint16_t v) {
    tvocExposureThreshold = v;
  }

private:
  void processCO2Sample(uint16_t v);

private:
  BuzzerController& buzzerController;
  uint16_t co2ExposureThreshold = 2000;
  uint16_t tvocExposureThreshold = 2500;
  uint16_t co2SustainedThresholdShort = 30000; // threshold to sustained CO2 value during a short term (15 min)
  uint16_t co2SustainedThresholdLong = 5000;  // threshold to sustained CO2 value during a long term (8 hr)
  bool turnOnAlarm = false;
  uint16_t co2SamplesIdx = 0;
  uint16_t co2AveragesIdx = 0;
  uint16_t co2Samples[CO2_SAMPLES_BUFF_SIZE] = { 0 };
  uint16_t co2Averages[CO2_AVERAGES_BUFF_SIZE] = { 0 };
};

AlarmController::AlarmController(BuzzerController &aBuzzerController)
  : buzzerController(aBuzzerController)
  {
    
  }

void AlarmController::registerMeasurements(uint16_t tvoc, uint16_t co2) {

  turnOnAlarm = false;

  processCO2Sample(co2);
  
  if ((tvoc >= tvocExposureThreshold || co2 >= co2ExposureThreshold) && !buzzerController.isAlarmOn()) {
    turnOnAlarm = true;
  }

  if (turnOnAlarm) {
    buzzerController.setAlarmOn(true);    
  } else {
    if ((tvoc < tvocExposureThreshold && co2 < co2ExposureThreshold) && buzzerController.isAlarmOn()) {
      buzzerController.setAlarmOn(false);
    }
  }
}

void AlarmController::processCO2Sample(uint16_t v) {
  co2Samples[co2SamplesIdx] = v;
  ++co2SamplesIdx;
  if (co2SamplesIdx == CO2_SAMPLES_BUFF_SIZE) {
    uint32_t sum = 0;
    do {
      co2SamplesIdx--;
      sum += co2Samples[co2SamplesIdx];
    } while (co2SamplesIdx != 0);
    uint32_t avg = sum / CO2_SAMPLES_BUFF_SIZE;
    if (avg >= co2SustainedThresholdShort) {
      turnOnAlarm = true;
    }

    co2Averages[co2AveragesIdx] = static_cast<uint16_t>(avg);
    ++co2AveragesIdx;
    if (co2AveragesIdx == CO2_AVERAGES_BUFF_SIZE) {
      sum = 0;
      do {
        co2AveragesIdx--;
        sum += co2Averages[co2AveragesIdx];
      } while (co2AveragesIdx != 0);
      avg = sum / CO2_AVERAGES_BUFF_SIZE;
      if (avg >= co2SustainedThresholdLong) {
        turnOnAlarm = true;
      }
    }
  }
}

AlarmController alarmController(buzzerController);



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("Could not mount filesystem!");
    while (1) ;
  }
  delay(100);


  EEPROM.begin(sizeof(settings));
  delay(10);

  readConfig();


  // the sensor connected to D0 pin of Wemos D1 board
  // the sensor's model is DHT22
  dht.setup(D0, DHTesp::DHT22);  
  dhtSamplingPeriod = dht.getMinimumSamplingPeriod();

  buzzerController.setup();

  // enable I2C
  Wire.begin();

  Serial.print("CCS811 library version: ");
  Serial.println(CCS811_VERSION);

// TO DO: check - is it really needed ?
  //ccs811.set_i2cdelay(500);  
  
  if (!ccs811.begin()) {
    Serial.println("Could not initialize CCS811 library!");
    while (1) ;
  }

  int v = ccs811.hardware_version();
  if (v != -1)
    Serial.printf("CCS811 hardware version: %02x\n", v);
  else
    Serial.println("CCS811 hardware version - couldn't get due to I2C error");

  v = ccs811.bootloader_version();
  if (v != -1)
    Serial.printf("CCS811 bootloader version: %04x\n", v);
  else
    Serial.println("CCS811 bootloader version - couldn't get due to I2C error");

  v = ccs811.application_version();
  if (v != -1)
    Serial.printf("CCS811 application version: %04x\n", v);
  else
    Serial.println("CCS811 application version - couldn't get due to I2C error");        

//  if (!ccs811.start(CCS811_MODE_60SEC)) {
//    Serial.println("CCS811 start failed");
//  }

  if (!ccs811.start(CCS811_MODE_1SEC)) {
    Serial.println("CCS811 start failed");
  }

//  WiFi.begin(SSID, PKEY);
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(1000);
//    Serial.println("Connecting to wifi ...");
//  }

  // connect to WiFi using station mode, when failed - connect using access point mode
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(settings.ssid, settings.password);
  bool connected = checkWiFiConnection();

  if (!connected) {
    WiFi.disconnect();
    delay(100);
    wifiNetworks = scanWiFiNetworks();
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(DEVICE_NAME)) {
      Serial.println("Could not setup soft AP!");
      while (1) ;
    }
    delay(100);
    Serial.print("SoftAP IP ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("Local IP ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway IP address: ");
    Serial.print(WiFi.gatewayIP());
  }

  initRequestHandlers();
  webServer.begin();

  if (!MDNS.begin("esp8266")) {
    Serial.println("Could not start MDNS responder!\n");
  }

  mqttSetupTopics();
  if (!mqttConnect()) {
    Serial.println("Could not connect to MQTT broker");
  }

  Serial.println("Setup complete.");
}

void publishEnvironmentalData(float temperature, float humidity) {
  if (mqttClient.connected() && strlen(settings.mqtt_root_topic) != 0) {
    char str[8];
    sprintf(str, "%.2f", temperature);
    mqttClient.publish(temperature_topic, str, true);
    sprintf(str, "%.2f", humidity);
    mqttClient.publish(humidity_topic, str, true);
  }
}

void publishAirQualityData(uint16_t eco2, uint16_t etvoc) {
  if (mqttClient.connected()) {
    char str[8];
    sprintf(str, "%u", eco2);
    mqttClient.publish(co2_topic, str, true);
    sprintf(str, "%u", etvoc);
    mqttClient.publish(tvoc_topic, str, true);
  }
}

void loop() {

  if (!mqttClient.connected()) {
    mqttConnect();
  }

  MDNS.update();
  mqttClient.loop();

  if (needRestart) {
    delay(1000);
    ESP.restart();
  }

  unsigned long currTime = millis();

  if ((currTime - lastTimeReadTH) > dhtSamplingPeriod) {
    lastTimeReadTH = currTime;
    th = dht.getTempAndHumidity();
    Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", th.temperature, th.humidity);
    if (!isnan(th.humidity) && !isnan(th.temperature)) {
      ccs811.set_envdata_Celsius_percRH(th.temperature, th.humidity);
      publishEnvironmentalData(th.temperature, th.humidity);
    }
  }

  if ((currTime - lastTimeReadTVOC) > CCS811_SAMPLING_PERIOD) {    
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);
    lastTimeReadTVOC = currTime;

    if (errstat == CCS811_ERRSTAT_OK) {
      Serial.printf("CCS811 - eCO2: %u ppm, eTVOC: %u ppb\n", eco2, etvoc);
      air_quality = { eco2, etvoc };
      publishAirQualityData(eco2, etvoc);
      alarmController.registerMeasurements(etvoc, eco2);
    } else if (errstat == CCS811_ERRSTAT_OK_NODATA) {
      Serial.println("CCS811 - waiting for new data.");
    } else if (errstat & CCS811_ERRSTAT_I2CFAIL) {
      Serial.println("CCS811 - I2C error.");
    } else {
      Serial.printf("CCS811 - error %04x (%s)\n", errstat, ccs811.errstat_str(errstat));
    }
  }

  buzzerController.tick(); 
}
