#include "DHTesp.h"

DHTesp dht;

int minSamplingPeriodMs = 0x7FFFFFFF;
long lastTimeReadSensor = 0;

// the type 'TempAndHumidity' defined in DHTesp library
TempAndHumidity th = { 0.f, 0.f };

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // the sensor connected to D0 pin of Wemos D1 board
  // the sensor's model is DHT22
  dht.setup(D0, DHTesp::DHT22);

  minSamplingPeriodMs = dht.getMinimumSamplingPeriod();  
}

void loop() {
  long currTime = millis();
  if ((currTime - lastTimeReadSensor) > minSamplingPeriodMs) {
    lastTimeReadSensor = currTime;
    th = dht.getTempAndHumidity();
    Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", th.temperature, th.humidity);
  }
}
