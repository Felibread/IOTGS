// Mindly - ESP32 example (MQTT publish + HTTP to ThingSpeak)
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "SEU_SSID";
const char* WIFI_PASS = "SUA_SENHA";

const char* MQTT_SERVER = "test.mosquitto.org"; // troque se tiver outro broker
const int   MQTT_PORT = 1883;

const char* THINGSPEAK_WRITE_KEY = "Y89YBHF63E1E5Z0N";
const char* THINGSPEAK_READ_KEY  = "HGXNLTN47W6INQNP";
const char* THINGSPEAK_URL_WRITE = "http://api.thingspeak.com/update";
const char* THINGSPEAK_CHANNEL_ID = "3175519";

WiFiClient espClient;
PubSubClient mqtt(espClient);

const char* device_id = "esp32-01";
String baseTopic;

unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL = 10000; // 10s

// Simulated sensor readings
bool simulatePresence = true;
int simulateSound = 40;
int simulateLight = 100;

void callback(char* topic, byte* payload, unsigned int length) {
  // recebe comandos do gateway via MQTT
  String msg;
  for (unsigned int i=0;i<length;i++) msg += (char)payload[i];
  Serial.print("MQTT Recv ["); Serial.print(topic); Serial.print("] "); Serial.println(msg);
  // parse JSON e agir 
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, msg);
  if(!err) {
    const char* cmd = doc["cmd"];
    if (cmd && String(cmd) == "start_focus") {
      Serial.println("Comando: start_focus recebido");
      // implementar lógica local: piscar LED, etc.
    } else if (cmd && String(cmd) == "alert_beep") {
      Serial.println("Alerta beep recebido");
     
    }
  }
}

void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");
    String clientId = String("mindly-") + device_id + "-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      String topic = baseTopic + "/cmd";
      mqtt.subscribe(topic.c_str());
      Serial.print("Subscribed to: "); Serial.println(topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(". retrying in 2s");
      delay(2000);
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(callback);
  baseTopic = String("mindly/teamA/device/") + device_id;
}

void loop() {
  if (!mqtt.connected()) reconnect();
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastTelemetry > TELEMETRY_INTERVAL) {
    lastTelemetry = now;
    // Simula leituras
    simulateSound = 35 + (random(0,30));
    simulateLight = 80 + random(0,200);
    simulatePresence = random(0,10) > 1;

    // Monta JSON
    DynamicJsonDocument doc(256);
    doc["device_id"] = device_id;
    // usar timestamp real se quiser: doc["timestamp"] = millis();
    doc["timestamp"] = String("2025-11-21T08:00:00-03:00");
    doc["presence"] = simulatePresence;
    doc["sound_level"] = simulateSound;
    doc["light_lux"] = simulateLight;
    doc["session_state"] = "idle";
    doc["session_remaining_s"] = 0;

    String payload;
    serializeJson(doc, payload);

    // Publica no MQTT
    String topic = baseTopic + "/telemetry";
    mqtt.publish(topic.c_str(), payload.c_str());
    Serial.print("Published to "); Serial.println(topic);
    Serial.println(payload);

    // Envia para ThingSpeak via HTTP
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String(THINGSPEAK_URL_WRITE) + "?api_key=" + THINGSPEAK_WRITE_KEY +
                   "&field1=" + String(simulateSound) +
                   "&field2=" + String(simulateLight) +
                   "&field3=" + String(simulatePresence ? 1 : 0) +
                   "&field4=" + String(0); 
      http.begin(url);
      int httpCode = http.GET();
      Serial.print("ThingSpeak HTTP GET code: "); Serial.println(httpCode);
      http.end();
    }
  }
}