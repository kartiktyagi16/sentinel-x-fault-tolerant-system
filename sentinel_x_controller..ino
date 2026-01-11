/*
 * Sentinel X ESP32 Node
 * Flash this SAME code to BOTH ESP32 boards
 * Only change NODE_ID for each board
 */

// --- LIBRARIES ---
#include <WiFi.h>
#include <PubSubClient.h>  // MQTT Client
#include <Wire.h>           // I2C
#include <Adafruit_HTU21DF.h> // Temp/Humidity Sensor
#include <ArduinoJson.h>    // For MQTT payloads

// --- CONFIGURATION (!!! CHANGE THIS FOR EACH ESP!!!) ---
#define NODE_ID "esp1" // For board 1
// #define NODE_ID "esp2" // For board 2 (uncomment for second ESP)

// --- WiFi & MQTT ---
const char* WIFI_SSID = "ESP32TEST";        // ← CHANGE THIS
const char* WIFI_PASS = "";    // ← CHANGE THIS
const char* MQTT_BROKER_IP = "192.168.102.90";   // ← CHANGE TO YOUR PI'S IP
const int MQTT_BROKER_PORT = 1883;

// --- HARDWARE PINS ---
// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
// Motor (L298N)
#define MOTOR_IN1_PIN 25
#define MOTOR_IN2_PIN 26
#define MOTOR_ENA_PIN 27
// Failure Simulation Button
#define FAIL_BUTTON_PIN 4

// --- GLOBAL STATE ---
enum NodeRole { ROLE_STANDBY, ROLE_ACTIVE, ROLE_FAILED };
volatile NodeRole current_role = ROLE_STANDBY;

int motor_speed = 0;
volatile bool failure_flag = false;
unsigned long last_status_publish = 0;
const long STATUS_PUBLISH_INTERVAL = 2000; // Publish every 2 seconds

// --- OBJECTS ---
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_HTU21DF htu = Adafruit_HTU21DF();

// --- MQTT TOPICS ---
String cmd_topic = "sentinel/" + String(NODE_ID) + "/cmd";
String status_topic = "sentinel/" + String(NODE_ID) + "/status";

// --- FUNCTION DECLARATIONS (Forward declarations) ---
void controlMotor(int speed);
void publishStatus(String status, float temp = 0.0, int rpm = 0);

// --- INTERRUPT HANDLER ---
void IRAM_ATTR handleFailureInterrupt() {
  failure_flag = true;
}

// --- WIFI CONNECTION ---
void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// --- MOTOR CONTROL ---
void controlMotor(int speed) {
  if (speed > 0) { // Forward
    digitalWrite(MOTOR_IN1_PIN, HIGH);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    analogWrite(MOTOR_ENA_PIN, speed);
  } else if (speed < 0) { // Reverse
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, HIGH);
    analogWrite(MOTOR_ENA_PIN, -speed);
  } else { // Stop
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    analogWrite(MOTOR_ENA_PIN, 0);
  }
}

// --- PUBLISH STATUS TO MQTT ---
void publishStatus(String status, float temp, int rpm) {
  StaticJsonDocument<200> doc;
  doc["status"] = status;
  
  if (status == "active") {
    doc["temperature"] = temp;
    doc["motor_rpm"] = rpm;
  } else if (status == "failed") {
    doc["reason"] = "manual_button_press";
  }

  char buffer[200];
  size_t n = serializeJson(doc, buffer);
  mqttClient.publish(status_topic.c_str(), buffer, n);
}

// --- MQTT MESSAGE HANDLER ---
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* role = doc["role"];
  
  if (strcmp(role, "active") == 0) {
    Serial.println("Received command: ROLE_ACTIVE");
    
    if (current_role != ROLE_ACTIVE) {
      if (!htu.begin()) {
        Serial.println("ERROR: Couldn't find HTU21 sensor!");
        failure_flag = true;
        return;
      } else {
        Serial.println("HTU21 sensor found. Becoming active.");
      }
    }
    
    current_role = ROLE_ACTIVE;
    motor_speed = doc["motor_speed"] | 0;
    
  } else if (strcmp(role, "standby") == 0) {
    Serial.println("Received command: ROLE_STANDBY");
    current_role = ROLE_STANDBY;
    motor_speed = 0;
    controlMotor(0);
    
  } else if (strcmp(role, "reboot") == 0) {
    Serial.println("Received command: REBOOT. Restarting...");
    publishStatus("rebooting", 0.0, 0);
    delay(500);
    ESP.restart();
  }
}

// --- MQTT RECONNECTION ---
void mqtt_reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqttClient.connect(NODE_ID)) {
      Serial.println("connected.");
      mqttClient.subscribe(cmd_topic.c_str(), 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// --- SETUP FUNCTION ---
void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting Sentinel X Node: " + String(NODE_ID));

  // 1. Setup Pins
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(MOTOR_ENA_PIN, OUTPUT);
  pinMode(FAIL_BUTTON_PIN, INPUT_PULLUP);

  // Initial state
  controlMotor(0);

  // 2. Setup Failure Button Interrupt
  attachInterrupt(digitalPinToInterrupt(FAIL_BUTTON_PIN), handleFailureInterrupt, FALLING);

  // 3. Connect to WiFi
  setup_wifi();

  // 4. Connect to MQTT
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  mqttClient.setCallback(mqtt_callback);
}

// --- MAIN LOOP ---
void loop() {
  if (!mqttClient.connected()) {
    mqtt_reconnect();
  }
  mqttClient.loop();

  // Check for button press
  if (failure_flag) {
    failure_flag = false;
    current_role = ROLE_FAILED;
    motor_speed = 0;
    controlMotor(0);
    Serial.println("!!! FAILURE BUTTON PRESSED! Reporting to supervisor. !!!");
    publishStatus("failed");
  }

  // Handle different roles
  if (current_role == ROLE_ACTIVE) {
    float temp = htu.readTemperature();
    controlMotor(motor_speed);
    
    if (millis() - last_status_publish > STATUS_PUBLISH_INTERVAL) {
      Serial.printf("Active. Temp: %.2f C, Motor Speed: %d\n", temp, motor_speed);
      publishStatus("active", temp, 0);
      last_status_publish = millis();
    }
    
  } else if (current_role == ROLE_STANDBY) {
    controlMotor(0);
    
    if (millis() - last_status_publish > STATUS_PUBLISH_INTERVAL) {
      Serial.println("Standby.");
      publishStatus("standby");
      last_status_publish = millis();
    }
    
  } else if (current_role == ROLE_FAILED) {
    if (millis() - last_status_publish > STATUS_PUBLISH_INTERVAL) {
      Serial.println("Failed. Awaiting reboot command.");
      publishStatus("failed");
      last_status_publish = millis();
    }
  }
}