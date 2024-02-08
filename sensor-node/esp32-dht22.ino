#include <DHTesp.h>
#include <PubSubClient.h>
#include <WiFi.h>

const int WATER_SENSOR_SIGNAL_PIN = 32;
const int GAS_SENSOR_SIGNAL_PIN = 34;
const int DHT_PIN = 15;

DHTesp dhtSensor;
WiFiClient clientWifi;
PubSubClient client(clientWifi);

// mqtt
const char* mqtt_server = "64.227.111.202";
const int mqtt_port = 1883;
const char* mqtt_client = "ESP32_Client";

// topics
const char* temperature_topic = "esp/temperature";
const char* humidity_topic = "esp/humidity";
const char* water_topic = "esp/water";
const char* gas_topic = "esp/gas";

void wifiSetup(){
  const char* ssid = "Wokwi-GUEST";
  const char* password = "";

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  pinMode(WATER_SENSOR_SIGNAL_PIN, INPUT);
  pinMode(GAS_SENSOR_SIGNAL_PIN, INPUT);
  wifiSetup();
  client.setServer(mqtt_server, mqtt_port);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

int checkWater(){
  int water = analogRead(WATER_SENSOR_SIGNAL_PIN);
  Serial.print("Water: ");
  Serial.println(water);
  return water;
}

TempAndHumidity checkDHT() {
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  return data;
}

int checkGas(){
  int gas = analogRead(GAS_SENSOR_SIGNAL_PIN);
  Serial.print("Gas: ");
  Serial.println(gas);
  return gas;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  TempAndHumidity dhtData = checkDHT();

  client.publish(temperature_topic, String(dhtData.temperature).c_str());
  client.publish(humidity_topic, String(dhtData.humidity).c_str());
  client.publish(water_topic, String(checkWater()).c_str());
  client.publish(gas_topic, String(checkGas()).c_str());

  delay(1000);
}
