#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRutils.h>
#include <map>

char* ssidv = "Vicens Wi-Fi 2.4GHz";
char* passwordv = "01422851999";
char* ssidt = "Fotsian";
char* passwordt = "30336422";

int intervalSensor = 2000;  // 2 seconds
int lastTimeSensor = 0;

int intervalTelegramNotif = 1000;  // 1 second
int lastTimeTelegramNotif = 0;

int intervalConsistency = 1000 * 60 * 5;  // 5 minutes
int lastTimeConsistency = 0;

int intervalAgua = 1000 * 60 * 1;  // 1 minute
int lastTimeAgua = 0;

int intervalAguaNotification = 1000 * 60 * 5;  // 5 minute
int lastTimeAguaNotification = 0;

int intervalGas = 1000 * 60 * 1;  // 1 minute
int lastTimeGas = 0;

int intervalGasNotification = 1000 * 60 * 5;  // 5 minute
int lastTimeGasNotification = 0;

int lastTimeActive = 0;
int currentMillis = 0;

String lastState = "apagado";

bool hayAgua = false;
bool hayGas = false;

String help = "Acciones disponibles:\n \
    prender - Prende el aire\n \
    apagar - Apaga el aire\n";

String help_mqtt = help + " \
    agua - Notifica si hay agua\n";

std::map<std::pair<String, String>, String>
  actions = {
    { { "apagado", "prender" }, "Prendiendo el aire" },
    { { "apagado", "apagar" }, "El aire ya esta apagado" },
    { { "prendido", "prender" }, "El aire ya esta prendido" },
    { { "prendido", "apagar" }, "Apagando el aire" },
    { { "prendido", "agua" }, "ATENCION: Agua detectada" },
    { { "apagado", "agua" }, "ATENCION: Agua detectada" },
    { { "prendido", "gas" }, "ATENCION: Gas detectado" },
    { { "apagado", "gas" }, "ATENCION: Gas detectado" },
  };

// Initialize Telegram BOT
#define BOTtoken "5921217958:AAH3VzXULbh7hQsxL04EPJbMCDNc199rDCs"
#define CHAT_ID "-936796293"

WiFiClient clientWifi;
WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOTtoken, clientSecure);

// mqtt
const char* mqtt_server = "64.227.111.202";
const int mqtt_port = 1883;
const char* mqtt_client = "ESP32_Actions";
const char* action_topic = "esp/action";

PubSubClient client(clientWifi);

// callback variables
char msg[50];
int value = 0;
long lastMsg = 0;

// IR, AC variables
const uint16_t kIrLed = 32;  // The ESP GPIO pin to use that controls the IR LED.
IRac ac(kIrLed);             // Create a A/C object using GPIO to sending messages with.
const decode_type_t protocol = decode_type_t::KELON;

void setNextState() {
  ac.next.protocol = protocol;                    // Set a protocol to use.
  ac.next.model = 1;                              // Some A/Cs have different models. Try just the first.
  ac.next.mode = stdAc::opmode_t::kCool;          // Run in cool mode initially.
  ac.next.celsius = true;                         // Use Celsius for temp units. False = Fahrenheit
  ac.next.degrees = 25;                           // 25 degrees.
  ac.next.fanspeed = stdAc::fanspeed_t::kMedium;  // Start the fan at medium.
  ac.next.swingv = stdAc::swingv_t::kOff;         // Don't swing the fan up or down.
  ac.next.swingh = stdAc::swingh_t::kOff;         // Don't swing the fan left or right.
  ac.next.light = false;                          // Turn off any LED/Lights/Display that we can.
  ac.next.beep = false;                           // Turn off any beep from the A/C if we can.
  ac.next.econo = false;                          // Turn off any economy modes if we can.
  ac.next.filter = false;                         // Turn off any Ion/Mold/Health filters if we can.
  ac.next.turbo = false;                          // Don't use any turbo/powerful/etc modes.
  ac.next.quiet = false;                          // Don't use any quiet/silent/etc modes.
  ac.next.sleep = -1;                             // Don't set any sleep time or modes.
  ac.next.clean = false;                          // Turn off any Cleaning options if we can.
  ac.next.clock = -1;                             // Don't set any current time if we can avoid it.
  ac.next.power = false;                          // Initially start with the unit off.
}

void setup_wifi() {
  clientSecure.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  char* ssid = ssidt;
  char* password = passwordt;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    int i = 0;
    Serial.print("Attempting connection to ");
    Serial.println(ssid);
    Serial.println(password);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && i < 10) {
      i++;
      Serial.print(".");
      delay(500);
    }
    if (ssid == ssidv) {
      ssid = ssidt;
      password = passwordt;
    } else if (ssid == ssidt) {
      ssid = ssidv;
      password = passwordv;
    }

    delay(500);
  }
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  setNextState();
}

void toggleAire(bool on) {
  ac.next.power = true;
  ac.sendAc();
  delay(1000);
  ac.next.turbo = true;
  ac.sendAc();
  delay(100);
  ac.next.turbo = false;
  ac.sendAc();
  delay(100);
  if (!on) {
    ac.next.power = false;
    ac.sendAc();
    delay(1000);
  }
}

bool calcConsistencyInterval(int currentMillis) {
  return intervalConsistency < currentMillis - lastTimeConsistency || lastTimeConsistency == 0;
}

bool calcNotificacionAguaInterval(int currentMillis) {
  return intervalAguaNotification < currentMillis - lastTimeAguaNotification || lastTimeAguaNotification == 0;
}

bool calcNotificacionGasInterval(int currentMillis) {
  return intervalGasNotification < currentMillis - lastTimeGasNotification || lastTimeGasNotification == 0;
}

void handleAction(String action, int currentMillis) {
  if (action == "prender" && !hayAgua && !hayGas) {
    if (lastState != "prendido") {
      lastState = "prendido";
      toggleAire(true);
      lastTimeActive = millis();
    } else if (calcConsistencyInterval(currentMillis)) {
      lastTimeConsistency = millis();
      toggleAire(true);
    }
  } else if (action == "apagar") {
    if (lastState != "apagado") {
      lastState = "apagado";
      toggleAire(false);
      lastTimeActive = millis();
    } else if (calcConsistencyInterval(currentMillis)) {
      lastTimeConsistency = millis();
      toggleAire(false);
    }
  } else if (action == "agua" && !hayAgua) {
    if (calcNotificacionAguaInterval(currentMillis)) {
      lastTimeAguaNotification = millis();
      bot.sendMessage(CHAT_ID, "ATENCION: Agua detectada");
    }
    lastTimeAgua = millis();
    hayAgua = true;
    toggleAire(false);  // apagamos por si el aire causa la perdida de agua
    lastState = "apagado";
  } else if (action == "gas" && !hayGas) {
    if (calcNotificacionGasInterval(currentMillis)) {
      lastTimeGasNotification = millis();
      bot.sendMessage(CHAT_ID, "ATENCION: Gas detectado");
    }
    lastTimeGas = millis();
    hayGas = true;
    toggleAire(false);  // apagamos por si el aire causa la perdida de gas
    lastState = "apagado";
  }
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("MQTT Topic: ");
  Serial.print(topic);
  Serial.print(" | Message: ");
  String text;
  for (int i = 0; i < length; i++) {
    text += (char)message[i];
  }
  Serial.println(text);

  if (String(topic) == action_topic
      && actions.find(std::make_pair(lastState, text)) != actions.end()) {
    if (text == "prender" && hayAgua && lastState == "apagado")
      Serial.println("No se puede realizar la accion ya que hay agua");
    else if (text == "prender" && hayGas && lastState == "apagado")
      Serial.println("No se puede realizar la accion ya que hay gas");
    else
      Serial.println(actions[{ lastState, text }]);
    handleAction(text, currentMillis);
  } else {
    Serial.println(help_mqtt);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client)) {
      Serial.println("connected");
      client.subscribe(action_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void handleNotifications(int currentMillis) {
  int newMessages = bot.getUpdates(bot.last_message_received + 1);
  while (newMessages) {
    for (int i = 0; i < newMessages; i++) {
      // Chat id of the requester
      String chat_id = String(bot.messages[i].chat_id);

      if (chat_id != CHAT_ID) {
        bot.sendMessage(chat_id, "Unauthorized user", "");
        continue;
      }
      String text = bot.messages[i].text;
      Serial.println("Telegram Message: " + text);

      if (actions.find(std::make_pair(lastState, text)) != actions.end() && text != "agua" && text != "gas") {
        if (text == "prender" && hayAgua && lastState == "apagado")
          bot.sendMessage(CHAT_ID, "No se puede realizar la accion ya que hay agua");
        else if (text == "prender" && hayGas && lastState == "apagado")
          bot.sendMessage(CHAT_ID, "No se puede realizar la accion ya que hay gas");
        else
          bot.sendMessage(CHAT_ID, actions[{ lastState, text }]);
        handleAction(text, currentMillis);
      } else {
        bot.sendMessage(CHAT_ID, help);
      }
    }
    newMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  currentMillis = millis();

  if (currentMillis - lastTimeTelegramNotif > intervalTelegramNotif) {
    lastTimeTelegramNotif = currentMillis;
    handleNotifications(currentMillis);
  }

  if (hayAgua && currentMillis - lastTimeAgua > intervalAgua) {
    hayAgua = false;
  }

  if (hayGas && currentMillis - lastTimeGas > intervalGas) {
    hayGas = false;
  }
}
