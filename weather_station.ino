#include <Wire.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <WiFiManager.h>

#include "ccs811.h"
#include "DHTesp.h"

// #define CCS811_SAMPLING_PERIOD 60000
#define CCS811_SAMPLING_PERIOD 1000

//#define SSID ""
//#define PKEY ""

#define MQTT_BROKER "broker.emqx.io"
#define MQTT_PORT 1883

#define TEMPERATURE_TOPIC "dht/temperature"
#define HUMIDITY_TOPIC "dht/humidity"
#define ECO2_TOPIC "ccs811/eco2"
#define TVOC_TOPIC "ccs811/tvoc"

CCS811 ccs811;
DHTesp dht;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

WiFiManager wifiManager;


int dhtSamplingPeriod = 0x7FFFFFFF;
long lastTimeReadTH = 0;
long lastTimeReadTVOC = 0;

// the type 'TempAndHumidity' defined in DHTesp library
TempAndHumidity th = { 0.f, 0.f };

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

void mqttConnectToBroker() {
  byte mac[6];
  WiFi.macAddress(mac);
  char clientId[32];
  sprintf(clientId, "esp8266-%02d%02d%02d%02d%02d%02d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.print("Connect to MQTT broker...   ");

  if (mqttClient.connect(clientId)) {
    Serial.println("connected");
  } else {
    Serial.print("Failed with state ");
    Serial.println(mqttClient.state());
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

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

  wifiManager.autoConnect();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  Serial.println("Setup complete.");
}

void publishTH() {
  if (mqttClient.connected()) {
    char str[16];
    sprintf(str, "%f", th.temperature);
    mqttClient.publish(TEMPERATURE_TOPIC, str, true);
    sprintf(str, "%f", th.humidity);
    mqttClient.publish(HUMIDITY_TOPIC, str, true);
  }
}

void publishAirQualityData(uint16_t eco2, uint16_t etvoc) {
  if (mqttClient.connected()) {
    char str[8];
    sprintf(str, "%u", eco2);
    mqttClient.publish(ECO2_TOPIC, str, true);
    sprintf(str, "%u", etvoc);
    mqttClient.publish(TVOC_TOPIC, str, true);
  }
}

void loop() {

  if (!mqttClient.connected()) {
    if (WiFi.status() == WL_CONNECTED) {
      mqttConnectToBroker();
    }
  }

  unsigned long currTime = millis();

  if ((currTime - lastTimeReadTH) > dhtSamplingPeriod) {
    lastTimeReadTH = currTime;
    th = dht.getTempAndHumidity();
    Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", th.temperature, th.humidity);
    if (!isnan(th.humidity) && !isnan(th.temperature)) {
      ccs811.set_envdata_Celsius_percRH(th.temperature, th.humidity);
      publishTH();
    }
  }

  if ((currTime - lastTimeReadTVOC) > CCS811_SAMPLING_PERIOD) {    
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);
    lastTimeReadTVOC = currTime;

    if (errstat == CCS811_ERRSTAT_OK) {
      Serial.printf("CCS811 - eCO2: %u ppm, eTVOC: %u ppb\n", eco2, etvoc);
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
