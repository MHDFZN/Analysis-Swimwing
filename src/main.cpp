#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ====== WiFi & Firebase Config ======
#define WIFI_SSID "REPLACE_WITH_YOUR_SSID"
#define WIFI_PASSWORD "REPLACE_WITH_YOUR_PASSWORD"

#define Web_API_KEY "REPLACE_WITH_YOUR_FIREBASE_PROJECT_API_KEY"
#define DATABASE_URL "REPLACE_WITH_YOUR_FIREBASE_DATABASE_URL"
#define USER_EMAIL "REPLACE_WITH_FIREBASE_PROJECT_EMAIL_USER"
#define USER_PASS "REPLACE_WITH_FIREBASE_PROJECT_USER_PASS"

// ====== Firebase Setup ======
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  if (aResult.isEvent())
    Firebase.printf("Event: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
  if (aResult.isDebug())
    Firebase.printf("Debug: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  if (aResult.isError())
    Firebase.printf("Error: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  if (aResult.available())
    Firebase.printf("Payload [%s]: %s\n", aResult.uid().c_str(), aResult.c_str());
}

// ====== Pin Definitions ======
#define WATER_TEMP_ONE_WIRE_PIN 33
#define TURBIDITY_SENSOR_PIN 34
#define ULTRASONIC_TRIG_PIN 26
#define ULTRASONIC_ECHO_PIN 27
#define PUMP_RELAY_PIN 25

// ====== Constants ======
const float ULTRASONIC_WATER_LEVEL_THRESHOLD_CM = 10.0;  // Set as needed (e.g. 10cm)
const float VREF = 3.3;
const int ADC_RES = 4095;

// ====== Globals ======
WebServer server(80);
OneWire oneWire(WATER_TEMP_ONE_WIRE_PIN);
DallasTemperature tempSensor(&oneWire);

float measuredTemperature = 0.0;
float measuredTurbidity = 0.0;
float measuredWaterLevel = 0.0; // in cm

// ====== Web Handlers ======
void handleRoot() {
  server.send(200, "text/plain", "ESP32 is running");
}
void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(measuredTemperature) + ",";
  json += "\"turbidity\":" + String(measuredTurbidity) + ",";
  json += "\"water_level_cm\":" + String(measuredWaterLevel);
  json += "}";
  server.send(200, "application/json", json);
}

// ====== Sensor Functions ======
float readWaterTemperature() {
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C || tempC == -127.0) {
    Serial.println("Temperature sensor error!");
    return NAN;
  }
  return tempC;
}

float readTurbidity() {
  // Simple analog read, convert to voltage and then NTU (calibration required for your sensor)
  int analogValue = analogRead(TURBIDITY_SENSOR_PIN);
  float voltage = analogValue * VREF / ADC_RES;
  // Example conversion: NTU = -1120.4*square(V) + 5742.3*V - 4352.9 (for some sensors)
  float ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  // Clamp to zero if negative
  if (ntu < 0) ntu = 0;
  return ntu;
}

float readUltrasonicWaterLevel() {
  // Returns water level (distance from sensor to water surface) in cm
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000); // max 30ms = 510 cm
  float distanceCm = duration * 0.034 / 2;
  return distanceCm;
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);

  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW);

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  tempSensor.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  ssl_client.setInsecure();
  ssl_client.setHandshakeTimeout(5);
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

// ====== Main Loop ======
void loop() {
  app.loop();
  server.handleClient();

  // === Read Sensors ===
  measuredTemperature = readWaterTemperature();
  measuredTurbidity = readTurbidity();
  measuredWaterLevel = readUltrasonicWaterLevel();

  // === Water Level Control ===
  if (!isnan(measuredWaterLevel) && measuredWaterLevel > ULTRASONIC_WATER_LEVEL_THRESHOLD_CM) {
    // Water level too LOW (ultrasonic measures distance from sensor to water)
    Serial.println("Water level low. Turning pump ON.");
    digitalWrite(PUMP_RELAY_PIN, HIGH);
  } else {
    // Water level sufficient
    digitalWrite(PUMP_RELAY_PIN, LOW);
  }

  // === Output to Serial ===
  Serial.printf("Temp: %.2f C, Turbidity: %.2f NTU, Water Level: %.2f cm\n",
                measuredTemperature, measuredTurbidity, measuredWaterLevel);

  // === Send Data to Firebase (except water level) ===
  static unsigned long lastSendTime = 0;
  const unsigned long sendInterval = 10000;
  unsigned long now = millis();
  if (app.ready() && now - lastSendTime >= sendInterval) {
    lastSendTime = now;
    if (!isnan(measuredTemperature))
      Database.set<float>(aClient, "/sensor/temperature", measuredTemperature, processData, "Send_Temp");
    if (!isnan(measuredTurbidity))
      Database.set<float>(aClient, "/sensor/turbidity", measuredTurbidity, processData, "Send_Turbidity");
    // Do NOT send water level to Firebase as requested
  }

  delay(2000);
}