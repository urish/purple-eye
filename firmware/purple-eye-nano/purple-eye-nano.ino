/**
   Purple Eye Firmware ported to [BLE Nano](http://redbearlab.com/blenano/)
   Copyright (C) 2016, Uri Shaked
   License: MIT.
*/

#include <BLE_API.h>
#include "nrf51servo.h"

#define DEVICE_NAME       "PurpleEye"
#define RIGHT_LEG_PIN     D4
#define RIGHT_FOOT_PIN    D5
#define LEFT_FOOT_PIN     D6
#define LEFT_LEG_PIN      D7
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
Servo              leftFoot, rightFoot, leftLeg, rightLeg;
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

void updateServos() {
  if (servoValues[0] == 0 && servoValues[1] == 0 && servoValues[2] == 0 && servoValues[3] == 0) {
    rightLeg.detach();
    rightFoot.detach();
    leftFoot.detach();
    leftLeg.detach();
  } else {
    if (!leftFoot.attached()) {
      rightLeg.attach(RIGHT_LEG_PIN);
      rightFoot.attach(RIGHT_FOOT_PIN);
      leftFoot.attach(LEFT_FOOT_PIN);
      leftLeg.attach(LEFT_LEG_PIN);
    }
    rightLeg.write(servoValues[0]);
    rightFoot.write(servoValues[1] + 8);
    leftFoot.write(servoValues[2] - 10);
    leftLeg.write(servoValues[3]);
  }
}

void disconnectionCallBack(const Gap::DisconnectionCallbackParams_t *params) {
  ble.startAdvertising();
  Serial.println("Disconnected :-(");
}

void gattServerWriteCallBack(const GattWriteCallbackParams *params) {
  if (params->handle == servosChar.getValueAttribute().getHandle()) {
    memcpy(servoValues, params->data, params->len);
    updateServos();
  }
}

void updateBatteryLevelCallback() {
  if (ble.getGapState().connected) {
    float voltage = analogRead(BATTERY_LEVEL_PIN) * 3.3 / 512;
    batteryLevel[0] = max(0, min(100, (int)((voltage - 3.6) / 0.6 * 100)));
    ble.updateCharacteristicValue(batteryLevelChar.getValueAttribute().getHandle(), batteryLevel, sizeof(batteryLevel));
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Purple Eye Nano!");

  // LED consumes about 0.3ma, so we turn it off.
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 1);

  pinMode(BATTERY_LEVEL_PIN, INPUT);
  updateServos();

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

  Serial.println("Ready :-)");
}

void loop() {
  ble.waitForEvent();
}
