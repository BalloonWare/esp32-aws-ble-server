#include "logmacros.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#include "WiFi.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <MQTTClient.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>

#define DEBUG

#define AWS_IOT_SUBSCRIBE_TOPIC "thing/esp32/sub"
#define AWS_IOT_PUBLISH_TOPIC "thing/esp32/pub"

std::string bleAdvertisingString = "XXXXXXXXXX";
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint16_t deviceID;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

#ifdef AWS
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);
#endif

HTTPClient httpclient;

void connectToWIFI();
void connectToAWS();
void connectToHttps(const char *serverUrl);
void startBLEserver();
void startAdvertising();
void sendStats(std::string arg);
void messageHandler(String &topic, String &payload);

void setup() {
  spdlog::set_level((spdlog::level::level_enum)SPDLOG_LEVEL_DEBUG);
#ifdef DEBUG
  Serial.begin(115200);
#endif
  MEMREPORT("--- setup");
  // esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  // MEMREPORT("--- after   esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT");
  connectToWIFI();
#ifdef AWS
  connectToAWS();
#else
  connectToHttps(SERVER_URL);
#endif

  startBLEserver();
  startAdvertising();

#ifdef DEBUG
  Serial.println("Listening for new devices");
#endif
}

static bool is_connected;
void loop() {
#ifdef AWS
  client.loop();
#endif
#ifdef DEBUG
  Serial.println(".");
#endif
  // MEMREPORT("loop");
  delay(5000);
}

class CharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

#ifdef DEBUG
    Serial.println("client wrote something...");
#endif

    if (value.length() > 0) {
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }

      Serial.println();
    }
    sendStats(value);
  }

  void onRead(BLECharacteristic *pCharacteristic) {
#ifdef DEBUG
    Serial.println("Message received by client");
#endif
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;

    pCharacteristic->setValue(bleAdvertisingString);
    pCharacteristic->notify();

    BLEDevice::startAdvertising();

    deviceID = pServer->getConnId();

#ifdef DEBUG
    Serial.print("new device ");
    Serial.print(deviceID);
    Serial.println(" connected");
#endif

    sendStats("new device connected");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;

    deviceID = pServer->getConnId();

#ifdef DEBUG
    Serial.print("device ");
    Serial.print(deviceID);
    Serial.println(" disconnected");
#endif

    sendStats("device disconnected");

    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
  }
};

void startBLEserver() {
#ifdef DEBUG
  Serial.println("BLE server: Starting...");
#endif

  BLEDevice::init("ESP32-BLE");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ |
                               BLECharacteristic::PROPERTY_WRITE |
                               BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new CharCallbacks());

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

#ifdef DEBUG
  Serial.println("BLE server: Started");
#endif
}

/**
 * Start the server advertising its existence
 */
void startAdvertising() {
#ifdef DEBUG
  Serial.println("BLE advertising: starting...");
#endif

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // helps w/ iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

#ifdef DEBUG
  Serial.println("BLE advertising: started");
#endif
}

void connectToWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

#ifdef DEBUG
  Serial.println("Connecting to Wi-Fi");
#endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}
#ifdef AWS
void connectToAWS() {
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 443, net);

  // Create a message handler
  client.onMessage(messageHandler);

#ifdef DEBUG
  Serial.println("AWS IoT: Connecting...");
#endif

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT: Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

#ifdef DEBUG
  Serial.println("AWS IoT: Connected");
#endif
}
#endif

void connectToHttps(const char *serverUrl) {
  httpclient.begin(serverUrl);
  httpclient.addHeader("Content-Type", "application/json");
  httpclient.POST("{ \"blahfasel\": 42}");

  // client.POST(payload, size);
  // client.end();
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
}

void sendStats(std::string arg) {
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["device_id"] = 815;
  doc["message"] = arg;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
#ifdef AWS
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
#else
  Serial.printf("posting: %s \n", jsonBuffer);

  httpclient.POST(jsonBuffer);
#endif
}

