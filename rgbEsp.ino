/*

*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino_JSON.h>

// BLE
#define BLE_NAME                "ESP32-led-strip-beb5483e"
#define SERVICE_UUID            "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // For read
#define CHARACTERISTIC_UUID_TX  "beb5483e-36e1-4688-b7f5-c5c9c331914b"  // For notify
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Led
#define RED 25
#define BLUE 32
#define GREEN 33
// Button
#define btnRED 12
#define btnBLUE 14
#define btnGREEN 27
#define btnWHITE 4

const int freq = 5000;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
const int resolution = 8;

String _colorJSON = "{\"isOn\":true, \"red\":0,\"green\":0,\"blue\":255,\"alpha\":255}";
bool isOn = true;
int red = 0;
int green = 0;
int blue = 255;
int alpha = 255;

// 
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
       BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// When change from clients
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      // Test if change
      if (_colorJSON.c_str() != rxValue) {
        _colorJSON = rxValue.c_str();
        Serial.println("/*******");
        // Notify all connected devices
        pTxCharacteristic->setValue(_colorJSON.c_str());
        pTxCharacteristic->notify();
          
        // Parse json
        JSONVar myArray = JSON.parse(_colorJSON);
        if (JSON.typeof(myArray) == "undefined") {
          Serial.println("Parsing input failed!");
          return;
        }
        // isOn, red, green, blue, alpha
        isOn = (bool) myArray["isOn"];
        red = (int) myArray["red"];
        green = (int) myArray["green"];
        blue = (int) myArray["blue"];
        alpha = (int) myArray["alpha"];

        // Update led
        int redToSend = 0;
        int greenToSend = 0;
        int blueToSend = 0;
        if (isOn) {
          double alphaPercent = alpha / 255.0 * 100;
          Serial.println(alphaPercent);
          redToSend = (int) (red * alphaPercent / 100);
          greenToSend = (int) (green * alphaPercent / 100);
          blueToSend = (int) (blue * alphaPercent / 100);
        }
        Serial.println(_colorJSON);
        Serial.println("*******/");
        ledcWrite(redChannel, redToSend);
        ledcWrite(greenChannel, greenToSend);
        ledcWrite(blueChannel, blueToSend);
      }
    }
};

// Red button - Set red
void redInterrupt() {
  isOn = true;
  red = 255;
  green = 0;
  blue = 0;
  alpha = 255;
  ledcWrite(redChannel, 255);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 0);
  updateJson();
}

// Green button - Set green
void greenInterrupt() {
  isOn = true;
  red = 0;
  green = 255;
  blue = 0;
  alpha = 255;
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 255);
  ledcWrite(blueChannel, 0);
  updateJson();
}

// Blue button - Set blue
void blueInterrupt() {
  isOn = true;
  red = 0;
  green = 0;
  blue = 255;
  alpha = 255;
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 255);
  updateJson();
}

// White button - On/Off
void whiteInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200) {
    if (isOn) {
      ledcWrite(redChannel, 0);
      ledcWrite(greenChannel, 0);
      ledcWrite(blueChannel, 0);
    } else {
      ledcWrite(redChannel, red);
      ledcWrite(greenChannel, green);
      ledcWrite(blueChannel, blue);
    }
    isOn = !isOn;
    updateJson();
  }
  last_interrupt_time = interrupt_time;
}

// Update json && Send notify to all connected devices
void updateJson() {
  JSONVar myArray;
  myArray["isOn"] = isOn;
  myArray["red"] = red;
  myArray["green"] = green;
  myArray["blue"] = blue;
  myArray["alpha"] = alpha;
  _colorJSON = JSON.stringify(myArray).c_str();
  Serial.println(_colorJSON);
  pTxCharacteristic->setValue(_colorJSON.c_str());
  pTxCharacteristic->notify();
}


void setup() {
  Serial.begin(115200);

  // LED
  pinMode(btnRED, INPUT_PULLUP);
  pinMode(btnGREEN, INPUT_PULLUP);
  pinMode(btnBLUE, INPUT_PULLUP);
  pinMode(btnWHITE, INPUT_PULLUP);
  ledcSetup(redChannel, freq, resolution);
  ledcSetup(greenChannel, freq, resolution);
  ledcSetup(blueChannel, freq, resolution);
  
  ledcAttachPin(RED, redChannel);
  ledcAttachPin(GREEN, greenChannel);
  ledcAttachPin(BLUE, blueChannel);

  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 255);

  // BLE
  
  // Create the BLE Device
  BLEDevice::init(BLE_NAME);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic - For notify
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );    
  pTxCharacteristic->addDescriptor(new BLE2902());
  pTxCharacteristic->setValue(_colorJSON.c_str());

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue(_colorJSON.c_str());
  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  // Btn
  attachInterrupt(digitalPinToInterrupt(btnRED), redInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(btnGREEN), greenInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(btnBLUE), blueInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(btnWHITE), whiteInterrupt, CHANGE);

}

void loop() {
    // notify changed value
    if (deviceConnected) {
        delay(3);
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("Disconnected...");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        Serial.println("Connected...");
        oldDeviceConnected = deviceConnected;
    }
}
