#include "logmacros.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#include "WiFi.h"
#include "secrets.h"
#include <ArduinoJson.h>
// #include <MQTTClient.h>
#include <WiFiClientSecure.h>

#include <HTTPClient.h>
#include "spdlog/fmt/bin_to_hex.h"

#define DEVICE_NAME "BallonVario"
std::string bleAdvertisingString = "XXXXXXXXXX";
uint16_t deviceID;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

HTTPClient httpclient;
bool httpclient_is_connected;
uint32_t ble_client_count;

void connectToWIFI();
void connectToHttps(const char *serverUrl);
void startBLEserver();
void startAdvertising();
void sendStats(uint16_t id, std::string arg);
void messageHandler(String &topic, String &payload);

void setup() {
  Serial.begin(115200);

  spdlog::set_level((spdlog::level::level_enum)SPDLOG_LEVEL_DEBUG);
  MEMREPORT("--- setup");
  // esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  // MEMREPORT("--- after
  // esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT");

  connectToWIFI();

  connectToHttps(SERVER_URL);

  startBLEserver();
  startAdvertising();

  LOGD("Listening for new devices");
}

void loop() {

  if (httpclient.connected() ^ httpclient_is_connected) { // edge
    LOGD("httpclient: {}connected", httpclient.connected() ? "" : "dis");
    MEMREPORT("loop");
    httpclient_is_connected = httpclient.connected(); // track
  }

  delay(10);
}

class CharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    LOGD("client wrote: {}", spdlog::to_hex(value));
    sendStats(9999, "my c aint good enough"); // spdlog::to_hex(value).
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    LOGD("Message received by client");
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    ble_client_count++;
    pCharacteristic->setValue(bleAdvertisingString);
    pCharacteristic->notify();

    BLEDevice::startAdvertising();
    deviceID = pServer->getConnId();
    LOGD("device {} disconnected", deviceID);
    sendStats(deviceID, "new device connected");
  };

  void onDisconnect(BLEServer *pServer) {
    ble_client_count--;
    deviceID = pServer->getConnId();
    LOGD("device {} disconnected", deviceID);
    sendStats(deviceID, "device disconnected");
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
  }
};

void startBLEserver() {

  LOGD("BLE server: Starting...");

  BLEDevice::init(DEVICE_NAME);

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

  LOGD("BLE server: Started");
}

/**
 * Start the server advertising its existence
 */
void startAdvertising() {

  LOGD("BLE advertising: starting...");

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // helps w/ iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  LOGD("BLE advertising: started");
}

void connectToWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  LOGD("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

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

void sendStats(uint16_t id, std::string arg) {
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["clients"] = ble_client_count;
  doc["device_id"] = id;
  doc["message"] = arg;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  Serial.printf("posting: %s \n", jsonBuffer);

  httpclient.POST(jsonBuffer);
}
