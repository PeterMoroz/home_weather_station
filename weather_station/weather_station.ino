#include "DHTesp.h"

DHTesp dht;
 
void setup() {
  Serial.begin(115200);
  dht.setup(D0, DHTesp::DHT22);
}
 
void loop() {
  readSensors(); 
  delay(2000);
}

unsigned long prevReadMs = 0;
const unsigned long READ_SENSORS_INTERVAL_MS = 10000;

void readSensors() {
  // TO DO: measure how long does it take to read the sensor's data  
  unsigned long currMs = millis();
  if (currMs - prevReadMs > READ_SENSORS_INTERVAL_MS) {
    prevReadMs = currMs;

    float h = dht.getHumidity();        
    if (isnan(h)) {
      Serial.println("Could not read humidity from DHT22!");
      return;
    }

    float t = dht.getTemperature();
    if (isnan(t)) {
      Serial.println("Could not read temperature from DHT22!");
      return;
    }
    Serial.print("humidity: ");
    Serial.print(h);
    Serial.print(" temperature: ");
    Serial.println(t);

    // read analog input (gas level)
    int g = analogRead(A0);

    Serial.print("{\"humidity\": ");
    Serial.print(h);
    Serial.print(", \"temp\": ");
    Serial.print(t);
    Serial.print(", \"gas level\": ");
    Serial.print(g);
    Serial.print("}\n");    
  }
  
}
