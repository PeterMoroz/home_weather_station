#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "ccs811.h"
#include "DHTesp.h"

// #define CCS811_SAMPLING_PERIOD 60000
#define CCS811_SAMPLING_PERIOD 1000

#define SSID ""
#define PKEY ""

CCS811 ccs811;
DHTesp dht;

HTTPClient httpClient;


int dhtSamplingPeriod = 0x7FFFFFFF;
long lastTimeReadTH = 0;
long lastTimeReadTVOC = 0;

// the type 'TempAndHumidity' defined in DHTesp library
TempAndHumidity th = { 0.f, 0.f };

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // the sensor connected to D0 pin of Wemos D1 board
  // the sensor's model is DHT22
  dht.setup(D0, DHTesp::DHT22);

  dhtSamplingPeriod = dht.getMinimumSamplingPeriod();

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

  WiFi.begin(SSID, PKEY);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to wifi ...");
  }

  Serial.println("Setup complete.");
}


void sendAirQualityData(uint16_t eco2, uint16_t etvoc) {
  char url[64] = { '\0' };
  sprintf(url, "http://192.168.0.105:5000/ccs811?eco2=%u&etvoc=%u", eco2, etvoc);
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient wifiClient;
    HTTPClient httpClient;
    httpClient.begin(wifiClient, url);
    int rcode = httpClient.GET();
    Serial.printf("url: %s\trcode: %d\n", url, rcode);
    httpClient.end();
  }
}

void loop() {
  long currTime = millis();
  if ((currTime - lastTimeReadTH) > dhtSamplingPeriod) {
    lastTimeReadTH = currTime;
    th = dht.getTempAndHumidity();
    Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", th.temperature, th.humidity);
    if (!isnan(th.humidity) && !isnan(th.temperature)) {
      ccs811.set_envdata_Celsius_percRH(th.temperature, th.humidity);

      // TO DO: send measurements, using POST
      char s[64] = { '\0' };
    }
  }

  if ((currTime - lastTimeReadTVOC) > CCS811_SAMPLING_PERIOD) {    
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);
    lastTimeReadTVOC = currTime;

    if (errstat == CCS811_ERRSTAT_OK) {
      Serial.printf("CCS811 - eCO2: %u ppm, eTVOC: %u ppb\n", eco2, etvoc);
      sendAirQualityData(eco2, etvoc);
    } else if (errstat == CCS811_ERRSTAT_OK_NODATA) {
      Serial.println("CCS811 - waiting for new data.");
    } else if (errstat & CCS811_ERRSTAT_I2CFAIL) {
      Serial.println("CCS811 - I2C error.");
    } else {
      Serial.printf("CCS811 - error %04x (%s)\n", errstat, ccs811.errstat_str(errstat));
    }
  }
 
}
