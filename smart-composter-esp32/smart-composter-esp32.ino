#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "env_var.h"

// Operation mode configuration
// Change to 0 or comment the line below to use REAL SENSORS
#define USE_MOCK_DATA 1

// Hardware definitions
#define TEMP_PIN_1 4
#define TEMP_PIN_2 16
#define MQ_SENSOR_PIN_1 32
#define MQ_SENSOR_PIN_2 33
#define HUMIDITY_SENSOR_PIN_1 34
#define HUMIDITY_SENSOR_PIN_2 35
#define ONBOARD_LED_PIN 2

// Sensor calibration constants
const int MQ135_RAW_MIN = 100;
const int MQ135_RAW_MAX = 3500;
const int MQ4_RAW_MIN = 50;
const int MQ4_RAW_MAX = 800;
const int HUMIDITY_RAW_DRY = 4095;
const int HUMIDITY_RAW_WET = 0;

// Sensor Objects
#if !USE_MOCK_DATA
OneWire oneWireBus1(TEMP_PIN_1);
OneWire oneWireBus2(TEMP_PIN_2);
DallasTemperature tempSensors1(&oneWireBus1);
DallasTemperature tempSensors2(&oneWireBus2);
#endif

// Network connection variables
const char *ssid = SSID;
const char *password = SSID_PASSWORD;

// Server connection variables
const char *serverHost = SERVER_HOST;
const int serverPort = SERVER_PORT;
const char *dataEndpoint = "/sensor-data";

// API key to access secured routes
const char *apiKey = API_KEY;

// Timing and hardware
const long POST_INTERVAL_MS = 150000; // Post data every 30 seconds
unsigned long lastPostTime = 0;
const int onBoardLedPin = ONBOARD_LED_PIN;

// Data structure
struct SensorData {
  float temp1_C;
  float temp2_C;
  float avg_temp_C;
  int mq1_raw;
  float mq1_percent;
  int mq2_raw;
  float mq2_percent;
  int humidity1_raw;
  float humidity1_percent;
  int humidity2_raw;
  float humidity2_percent;
  float avg_humidity;
};

// Post Data Structure
struct PostData {
  float avg_temp_C;
  float avg_humidity_percent;
  float mq1_percent;
  float mq2_percent;
};

// Mock data set (Uses the simplified PostData structure)
#if USE_MOCK_DATA
const PostData mockPostData[] = {
    {25.5, 50.0, 15.0, 5.0},
    {26.1, 52.5, 18.2, 6.5},
    {24.8, 48.0, 12.5, 4.0},
    {27.0, 55.0, 20.0, 8.0},
    {25.0, 49.5, 14.5, 5.5}};
const size_t MOCK_DATA_SIZE = sizeof(mockPostData) / sizeof(mockPostData[0]);
size_t mockIndex = 0;
#endif

// Utility Functions
float mapToPercent(int rawValue, int rawMin, int rawMax) {
  rawValue = constrain(rawValue, min(rawMin, rawMax), max(rawMin, rawMax));
  return (float)(rawValue - rawMin) / (rawMax - rawMin) * 100.0;
}

// Functions for real mode
#if !USE_MOCK_DATA
SensorData readAllSensors() {
  SensorData data;

  tempSensors1.requestTemperatures();
  tempSensors2.requestTemperatures();
  data.temp1_C = tempSensors1.getTempCByIndex(0);
  data.temp2_C = tempSensors2.getTempCByIndex(0);

  float totalTemp = 0.0;
  int validTemps = 0;
  if (data.temp1_C != DEVICE_DISCONNECTED_C) {
    totalTemp += data.temp1_C;
    validTemps++;
  }
  if (data.temp2_C != DEVICE_DISCONNECTED_C) {
    totalTemp += data.temp2_C;
    validTemps++;
  }
  data.avg_temp_C = validTemps ? totalTemp / validTemps : -999.0;

  data.mq1_raw = analogRead(MQ_SENSOR_PIN_1);
  data.mq2_raw = analogRead(MQ_SENSOR_PIN_2);
  data.mq1_percent = mapToPercent(data.mq1_raw, MQ135_RAW_MIN, MQ135_RAW_MAX);
  data.mq2_percent = mapToPercent(data.mq2_raw, MQ4_RAW_MIN, MQ4_RAW_MAX);

  data.humidity1_raw = analogRead(HUMIDITY_SENSOR_PIN_1);
  data.humidity2_raw = analogRead(HUMIDITY_SENSOR_PIN_2);
  data.humidity1_percent = mapToPercent(data.humidity1_raw, HUMIDITY_RAW_DRY, HUMIDITY_RAW_WET);
  data.humidity2_percent = mapToPercent(data.humidity2_raw, HUMIDITY_RAW_DRY, HUMIDITY_RAW_WET);
  data.avg_humidity = (data.humidity1_percent + data.humidity2_percent) / 2.0;

  return data;
}

PostData processSensorData(const SensorData &allData) {
  PostData post;
  post.avg_temp_C = allData.avg_temp_C;
  post.avg_humidity_percent = allData.avg_humidity;
  post.mq1_percent = allData.mq1_percent;
  post.mq2_percent = allData.mq2_percent;
  return post;
}
#endif

// Function for mock mode
#if USE_MOCK_DATA
PostData getMockPostData() {
  const PostData &currentData = mockPostData[mockIndex];
  mockIndex = (mockIndex + 1) % MOCK_DATA_SIZE;
  return currentData;
}
#endif

void postDataToServer() {
  PostData payloadData;

#if USE_MOCK_DATA
  payloadData = getMockPostData();
  Serial.println("--- MOCK MODE (Simulated Data) ---");
#else
  SensorData rawData = readAllSensors();
  payloadData = processSensorData(rawData);
  Serial.println("--- REAL MODE (Physical Sensors Data) ---");
#endif

  Serial.printf("AvgTemp: %.2fC, AvgHum: %.2f%%, MQ1: %.2f%%, MQ2: %.2f%%\n",
                payloadData.avg_temp_C, payloadData.avg_humidity_percent,
                payloadData.mq1_percent, payloadData.mq2_percent);
  Serial.println("---------------------------------");

  const int capacity = JSON_OBJECT_SIZE(4);
  StaticJsonDocument<capacity> doc;

  doc["avg_temp_C"] = payloadData.avg_temp_C;
  doc["avg_humidity_percent"] = payloadData.avg_humidity_percent;
  doc["mq1_percent"] = payloadData.mq1_percent;
  doc["mq2_percent"] = payloadData.mq2_percent;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.print("Sending payload: ");
  Serial.println(jsonPayload);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://" + String(serverHost) + ":" + String(serverPort) + String(dataEndpoint);
    http.begin(url);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", String(apiKey));

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.printf("[HTTP] POST successful, response code: %d\n", httpResponseCode);
      if (httpResponseCode == 200) {
        String response = http.getString();
        Serial.print("Server response: ");
        Serial.println(response);
      }
    } else {
      Serial.printf("[HTTP] POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();

    digitalWrite(onBoardLedPin, HIGH);
    delay(50);
    digitalWrite(onBoardLedPin, LOW);
  } else {
    Serial.println("WiFi not connected. Cannot POST data.");
  }
}

void setup() {
  Serial.begin(115200);

#if !USE_MOCK_DATA
  tempSensors1.begin();
  tempSensors2.begin();
  pinMode(MQ_SENSOR_PIN_1, INPUT);
  pinMode(MQ_SENSOR_PIN_2, INPUT);
  pinMode(HUMIDITY_SENSOR_PIN_1, INPUT);
  pinMode(HUMIDITY_SENSOR_PIN_2, INPUT);
#endif

  pinMode(onBoardLedPin, OUTPUT);
  digitalWrite(onBoardLedPin, LOW);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("FastAPI Server Target: http://" + String(serverHost) + ":" + String(serverPort) + String(dataEndpoint));
#if USE_MOCK_DATA
  Serial.println(">>> WARNING: Running in MOCK MODE (Simulated Data) <<<");
#else
  Serial.println(">>> REAL MODE (Reading from physical sensors) <<<");
#endif
}

void loop() {
  if (millis() - lastPostTime >= POST_INTERVAL_MS) {
    postDataToServer();
    lastPostTime = millis();
  }
}
