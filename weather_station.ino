#include <Wire.h>

#include <Adafruit_CCS811.h>
#include "DHTesp.h"

#define CCS811_SAMPLING_PERIOD 60000

Adafruit_CCS811 ccs811;
DHTesp dht;


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

  if (!ccs811.begin()) {
    Serial.println("Could not initialize CCS811 library!");
    while (1) ;
  }

  ccs811.setDriveMode(CCS811_DRIVE_MODE_60SEC);

  // wait for the sensor to be ready
  while (!ccs811.available()) ;

  Serial.println("Setup complete.");
}

void loop() {
  long currTime = millis();
  if ((currTime - lastTimeReadTH) > dhtSamplingPeriod) {
    lastTimeReadTH = currTime;
    th = dht.getTempAndHumidity();
    Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", th.temperature, th.humidity);
    if (!isnan(th.humidity) && !isnan(th.temperature)) {
      ccs811.setEnvironmentalData(th.humidity, th.temperature);
    }
  }

  if ((currTime - lastTimeReadTVOC) > CCS811_SAMPLING_PERIOD) {    
    if (ccs811.available()) {
      uint8_t retcode = ccs811.readData();
      if (retcode == 0) {
        uint16_t tvoc = ccs811.getTVOC();
        uint16_t eco2 = ccs811.geteCO2();
        lastTimeReadTVOC = currTime;
      } else {
        // TO DO: decode error
        Serial.printf("Error when read CCS811 data: %02X\n", retcode);
      }
    } else {
      Serial.println("No data from CCS811");
    }
  }

}
