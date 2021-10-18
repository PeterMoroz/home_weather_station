#include <ESP.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <DHT.h>

ESP8266WebServer webServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#define DHTPIN D4
#define DHTTYPE DHT22

// credentials: ssid, password
String s;
String p;

// sensors data
float h = 0.f, t = 0.f;

// TO DO: store in EEPROM at initial startup
// const char* MQTT_BROKER_IP = "192.168.0.105";
const char* MQTT_BROKER_ADDR = "public.mqtthq.com";


DHT dht(DHTPIN, DHTTYPE);

bool saveWiFiCredentials() {

  String ssid(webServer.arg("ssid"));
  String pass(webServer.arg("password"));
  
  char buff1[30];
  char buff2[30];
  ssid.toCharArray(buff1, 30);
  pass.toCharArray(buff2, 30);  

  uint8_t addr = 0;
  const char key[] = "WeMos";
  
  for (uint8_t i = 0; i < 5; i++) {
    EEPROM.write(addr++, key[i]);
  }
  for (uint8_t i = 0; i < 30; i++) {
    EEPROM.write(addr++, buff1[i]);
  }
  for (uint8_t i = 0; i < 30; i++) {
    EEPROM.write(addr++, buff2[i]);
  }

  if (!EEPROM.commit()) {
    Serial.print("EEPROM commit failed!");
    return false;
  }

  delay(100);

  addr = 5;
  for (uint8_t i = 0; i < 30; i++) {
    buff1[i] = '\0';
    buff1[i] = EEPROM.read(addr++);
  }

  for (uint8_t i = 0; i < 30; i++) {
    buff2[i] = '\0';
    buff2[i] = EEPROM.read(addr++);
  }  

  s = buff1;
  p = buff2;

  Serial.print("WiFi credentials: ");
  Serial.print(s);
  Serial.print(' ');
  Serial.print(p);

  if (ssid == s && pass == p) {
    return true;
  } else {
    return false;
  }
  return false;
}

void handleSubmit() {
  String responseSuccess = "<h1>Success</h1><h2>Device will restart in 3 seconds</h2>";
  String responseError = "<h1>Error</h1><h2><a href='/'>Go back</a> to try again</h2>";
  if (saveWiFiCredentials()){
    webServer.send(200, "text/html", responseSuccess);
    delay(3000);
    ESP.restart();
  } else {
    webServer.send(200, "text/html", responseError);
  }  
}

const char INDEX_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta content=\"text/html; charset=ISO-8859-1\""
" http-equiv=\"content-type\">"
" <meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>HomeSensors Authorization</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; text-align: center;}\""
"</style>"
"</head>"
"<body>"
"<h3>Enter your WiFi credentials</h3>"
"<form action=\"/\" method=\"post\">"
"<p>"
"<label>SSID:&nbsp;</label>"
"<input maxlength=\"30\" name=\"ssid\"><br>"
"<label>Key:&nbsp;&nbsp;&nbsp;&nbsp;</label><input maxlength=\"30\" name=\"password\"><br>"
"<input type=\"submit\" value=\"Save\">"
"</p>"
"</form>"
"</body>"
"</html>";


void handleRoot() {
  if (webServer.hasArg("ssid") && webServer.hasArg("password")) {
    handleSubmit();
  } else {
    webServer.send(200, "text/html", INDEX_HTML);
  }
}

void handleNotFound() {
  String msg = "File not found\n\n";
  msg += "URI: ";
  msg += webServer.uri();
  msg += "\nMethod: ";
  msg += (webServer.method() == HTTP_GET ? "GET" : (webServer.method() == HTTP_POST ? "POST" : "unknown"));
  msg += "\nArguments: ";
  msg += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    msg += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", msg);
}

const char MAIN_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<head>

<body>
  <center>
    <h4>WiFi Wemos demo</h4>
    <h1><span id="humidity">0</span><br></h1>
    <h1><span id="temperature">0</span><br></h1>
  </center>
  <script>
    setInterval(function() {
      updateMeasurements();
    }, 2000); // update each 2000 ms
    
    function updateMeasurements() {
      var requestHumidity = new XMLHttpRequest();
      requestHumidity.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          console.log('update humidity: ' + this.responseText);
          document.getElementById("humidity").innerHTML = this.responseText;
        }
      };
      requestHumidity.open("GET", "get_humidity", true);
      requestHumidity.send();
      
      var requestTemperature = new XMLHttpRequest();
      requestTemperature.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          console.log('update temperature: ' + this.responseText);
          document.getElementById("temperature").innerHTML = this.responseText;
        }
      };
      requestTemperature.open("GET", "get_temperature", true);
      requestTemperature.send();
    }
  </script>
</body>
</html>
)=====";

void onGetMainPage() {
  Serial.println("requested main page");
  webServer.send(200, "text/html", MAIN_HTML);
}

void onGetHumidity() {
  String txt(h);
  Serial.println("requested humidity");
  webServer.send(200, "text/plain", txt);
}

void onGetTemperature() {
  String txt(t);
  Serial.println("requested temperature");
  webServer.send(200, "text/plain", txt);
}

bool checkWiFiCredentials() {
  Serial.println("Checking WiFi credentials");

  // read signature
  char buff[30] = {'\0'};
  uint8_t addr = 0;

  for (uint8_t i = 0; i < 5; i++) {
    buff[i] = EEPROM.read(addr++);
  }

  if (strncmp(buff, "WeMos", 5)) {
    return false;
  }
  
  for (uint8_t i = 0; i < 30; i++) {
    buff[i] = '\0';
    buff[i] = EEPROM.read(addr++);
  }
  String ssid = buff;
  
  for (uint8_t i = 0; i < 30; i++) {
    buff[i] = '\0';
    buff[i] = EEPROM.read(addr++);
  }
  String pass = buff;

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("password: ");
  Serial.println(pass);

  if (ssid.length() > 0 && pass.length() > 0) {
    s = ssid;
    p = pass;
    return true;
  }

  return false;
}

void loadWiFiCredentialsForm() {
  Serial.println("Setting access point ... ");
  const char* ssid = "WeMos WiFi";
  const char* password = "12345678";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("Access Point IP: ");
  Serial.println(ip);

  webServer.on("/", handleRoot);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("Web server started.");
  while (s.length() <= 0 && p.length() <= 0) {
    webServer.handleClient();
    delay(100);
  }
}

void connectWiFi() {
  Serial.println("Setup station mode.");
  WiFi.mode(WIFI_STA);
  WiFi.begin(s, p);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("*");
  }
  Serial.print("\nThe IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  // loop untill succeeded
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker ... ");
    if (mqttClient.connect("ESP8266")) {
      Serial.println(" CONNECTED.");
      mqttClient.subscribe("esp8266/4");
      mqttClient.subscribe("esp8266/5");
    } else {
      Serial.print(" FAILED! error: ");
      Serial.print(mqttClient.state());
      Serial.println(" Try again in 5 seconds.");
      // wait for 5 seconds before retrying
      delay(5000);
    }
  }
}

void onMessageReceived(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" Message: ");

  unsigned int i = 0;
  for (; i < length; i++) {
    Serial.print((char)message[i]);    
  }
  Serial.println();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  EEPROM.begin(65); // signature length + max size of SSID + max size of password: 5 + 30 + 30 = 65

  if (!checkWiFiCredentials()) {
    Serial.println("No WiFi credentials in memory. Loading form...");
    loadWiFiCredentialsForm();
  } else {
    Serial.println("The WiFi credentials are in EEPROM. Connect to WiFi...");
    connectWiFi();
    Serial.println("Setup WebServer");
    webServer.on("/", HTTP_GET, onGetMainPage);
    webServer.on("/get_humidity", HTTP_GET, onGetHumidity);
    webServer.on("/get_temperature", HTTP_GET, onGetTemperature);
    Serial.println("Start WebServer");
    webServer.begin();
  }

  dht.begin();
  // mqttClient.setServer(MQTT_BROKER_IP, 1883);
  mqttClient.setServer(MQTT_BROKER_ADDR, 1883);
  mqttClient.setCallback(onMessageReceived);
}


unsigned long prevMs = 0;
const unsigned long READ_SENSORS_INTERVAL = 10000;

void readSensors() {
  // TO DO: measure how long does it take to read the sensor's data
  unsigned long currMs = millis();
  if (currMs - prevMs > READ_SENSORS_INTERVAL) {
    prevMs = currMs;

    h = dht.readHumidity();
    if (isnan(h)) {
      Serial.println("Could not read humidity from DHT22!");
      return;
    }
    t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println("Could not read temperature from DHT22!");
      return;
    }
    Serial.print("humidity: ");
    Serial.print(h);
    Serial.print(" temperature: ");
    Serial.println(t);

    char strHumidity[8];
    dtostrf(h, 6, 2, strHumidity);
    char strTemperature[8];
    dtostrf(t, 6, 2, strTemperature);

    // mqttClient.publish("/esp8266/humidity", strHumidity);
    // mqttClient.publish("/esp8266/temperature", strTemperature);
    mqttClient.publish("/home/wemos/humidity", strHumidity);
    mqttClient.publish("/home/wemos/temperature", strTemperature);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  
  readSensors();

  webServer.handleClient();
}
