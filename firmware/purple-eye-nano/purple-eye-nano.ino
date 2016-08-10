/**
   Purple Eye Firmware ported to [BLE Nano](http://redbearlab.com/blenano/)
   Copyright (C) 2016, Uri Shaked
   License: MIT.
*/

#include <BLE_API.h>
#include <Servo.h>

#define DEVICE_NAME       "PurpleEye"
#define BATTERY_LEVEL_PIN A5

// Physical Web
#define EDDYSTONE_SERVICE_UUID 0xFEAA
uint8_t eddystoneData[]  = {
  0xAA, 0xFE,  // Eddystone Service Id
  0x10,        // Frame type: URL
  0xF8,        // Power
  0x03,        // https://
  'b', 'i', 't', '.', 'd', 'o', '/', 'p', 'r', 'p', 'l',
};

BLE    ble;
static const uint16_t advertisedServices[] = { GattService::UUID_BATTERY_SERVICE, 0x5100, EDDYSTONE_SERVICE_UUID };

// Servos
Servo              servos[4];
static uint8_t     servoValues[4]         = {0, 0, 0, 0};
GattCharacteristic servosChar(0x5200, servoValues, sizeof(servoValues), sizeof(servoValues), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
GattCharacteristic *servosChars[] = {&servosChar, };
GattService        servosService(0x5100, servosChars, sizeof(servosChars) / sizeof(GattCharacteristic *));

// Battery
Ticker             batteryTask;
static uint8_t     batteryLevel[1];
GattCharacteristic batteryLevelChar(GattCharacteristic::UUID_BATTERY_LEVEL_CHAR, batteryLevel, sizeof(batteryLevel), sizeof(batteryLevel), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
GattCharacteristic *batteryServiceChars[] = {&batteryLevelChar };
GattService        batteryService(GattService::UUID_BATTERY_SERVICE, batteryServiceChars, sizeof(batteryServiceChars) / sizeof(GattCharacteristic *));

void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  ble.startAdvertising();
  Serial.println("Disconnected :-(");
}

void gattServerWriteCallBack(const GattWriteCallbackParams *params) {
  if (params->handle == servosChar.getValueAttribute().getHandle()) {
    Serial.print("Updating servos: ");
    memcpy(servoValues, params->data, params->len);
    for (uint16_t i = 0; i < params->len; i++) {
      servos[i].write(servoValues[i]);
      if (i > 0) {
        Serial.print(", ");
      }
      Serial.print(servoValues[i], HEX);
    }
    Serial.println("");
  }
}

void updateBatteryLevelCallback() {
  if (ble.getGapState().connected) {
    // TODO: convert voltage level to value in range [0..100]
    batteryLevel[0] = analogRead(BATTERY_LEVEL_PIN);
    ble.updateCharacteristicValue(batteryLevelChar.getValueAttribute().getHandle(), batteryLevel, sizeof(batteryLevel));
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Purple Eye Nano!");

  pinMode(A5, INPUT);
  servos[0].attach(D4);
  servos[1].attach(D5);
  servos[2].attach(D6);
  servos[3].attach(D7);

  ble.init();
  ble.onDisconnection(disconnectionCallBack);
  ble.onDataWritten(gattServerWriteCallBack);

  // Setup advertising payload, including Eddystone data
  ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
  ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t*)advertisedServices, sizeof(advertisedServices));
  ble.accumulateAdvertisingPayload(GapAdvertisingData::SERVICE_DATA, (uint8_t*)eddystoneData, sizeof(eddystoneData));
  ble.accumulateScanResponse(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
  ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);

  ble.addService(servosService);
  ble.addService(batteryService);
  ble.setDeviceName((const uint8_t *)DEVICE_NAME);
  ble.setTxPower(4);
  ble.setAdvertisingInterval(160); // (100 ms = 160 * 0.625ms.)
  ble.setAdvertisingTimeout(0);
  ble.startAdvertising();

  // Battery level task
  batteryTask.attach(updateBatteryLevelCallback, 1);

  Serial.println("Ready :)");
}

void loop() {
  ble.waitForEvent();
}
