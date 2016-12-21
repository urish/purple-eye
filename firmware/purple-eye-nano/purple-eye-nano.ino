/**
   Purple Eye Firmware ported to [BLE Nano](http://redbearlab.com/blenano/)
   Copyright (C) 2016, Uri Shaked
   License: MIT.
*/

extern "C" {
#include <pstorage.h>
#include <fstorage.h>
#include "softdevice_handler.h"
}

#include <BLE_API.h>
#include <Wire.h>
#include <LSM303.h>
#include <L3G.h>
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
static int8_t      servoOffsets[4]        = {0, 0, 0, 0};
GattCharacteristic servoOffsetsChar(0x5201, (uint8_t*)servoOffsets, sizeof(servoOffsets), sizeof(servoOffsets), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
GattCharacteristic *servosChars[] = {&servosChar, &servoOffsetsChar, };
GattService        servosService(0x5100, servosChars, sizeof(servosChars) / sizeof(GattCharacteristic *));

// Battery
Ticker             batteryTask;
static uint8_t     batteryLevel[1];
GattCharacteristic batteryLevelChar(GattCharacteristic::UUID_BATTERY_LEVEL_CHAR, batteryLevel, sizeof(batteryLevel), sizeof(batteryLevel), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
GattCharacteristic *batteryServiceChars[] = {&batteryLevelChar };
GattService        batteryService(GattService::UUID_BATTERY_SERVICE, batteryServiceChars, sizeof(batteryServiceChars) / sizeof(GattCharacteristic *));

// Accelerometer + Magnetometer + Gyro (IMU)
LSM303             imuDevice;
bool               imuAvailable;
L3G                gyro;
bool               gyroAvailable;
static uint16_t    imuData[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
GattCharacteristic imuChar(0xff09, (uint8_t*)imuData, sizeof(imuData), sizeof(imuData), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
GattCharacteristic *imuChars[] = {&imuChar };
GattService        imuService(0xff08, imuChars, sizeof(imuChars) / sizeof(GattCharacteristic *));

// Sound
byte               playerData[] = {0, 0, 0};
GattCharacteristic playerChar(0xff1a, (uint8_t*)playerData, sizeof(playerData), sizeof(playerData), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
GattCharacteristic *playerChars[] = {&playerChar };
GattService        playerService(0xff10, playerChars, sizeof(playerChars) / sizeof(GattCharacteristic *));

// Storage
bool               storageAvailable = false;
FS_SECTION_VARS_ADD(fs_config_t storageConfig) = { .cb = &storageCallback, .num_pages = 1, .page_order = 1 };

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
    rightLeg.write(servoValues[0] + servoOffsets[0]);
    rightFoot.write(servoValues[1] + servoOffsets[1]);
    leftFoot.write(servoValues[2] + servoOffsets[2]);
    leftLeg.write(servoValues[3] + servoOffsets[3]);
  }
}

static void sysEventHandler(uint32_t sys_evt) {
  // Delegate events to the fstorage module, as well as pstorage (used by the RBL stack)
  fs_sys_event_handler(sys_evt);
  pstorage_sys_event_handler(sys_evt);
}

static void storageCallback(uint8_t op_code, uint32_t result, uint32_t  const * p_data, fs_length_t length) {
  if (op_code == FS_OP_ERASE && result == NRF_SUCCESS) {
    fs_store(&storageConfig, storageConfig.p_start_addr, (uint32_t*)servoOffsets, 1); // 1 words = 4 bytes
  }
}

void loadServoOffsets() {
  memcpy(servoOffsets, storageConfig.p_start_addr, sizeof(servoOffsets));
}

void saveServoOffsets() {
  fs_erase(&storageConfig, storageConfig.p_start_addr, FS_PAGE_SIZE_WORDS);
}

void playerCommand(byte cmd, byte arg1, byte arg2) {
  byte data[10] = {0x7e, 0xff, 0x6, cmd, 0, arg1, arg2, 0, 0, 0xef};
  int16_t checksum = 0 - data[1] - data[2] - data[3] - data[4] - data[5] - data[6];
  data[7] = (checksum >> 8) & 0xff;
  data[8] = checksum & 0xff;
  Serial.write(data, sizeof(data));
}

void playerCommand(byte cmd, uint16_t arg) {
  playerCommand(cmd, arg >> 8, arg & 0xff);
}

void playSound(uint16_t fileNum, byte volume) {
  if (volume > 0) {
    playerCommand(0x6, 0, volume);
    delay(10);
  }
  playerCommand(0x3, fileNum);
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
  if (params->handle == servoOffsetsChar.getValueAttribute().getHandle()) {
    memcpy(servoOffsets, params->data, params->len);
    saveServoOffsets();
  }
  if ((params->handle == playerChar.getValueAttribute().getHandle()) && (params->len >= 2)) {
    uint16_t fileId = *(uint16_t*)params->data;
    uint8_t volume = params->len == 3 ? params->data[2] : 0;
    playSound(fileId, volume);
  }
}

void sensorsCallback() {
  if (ble.getGapState().connected) {
    // Battery Level
    float voltage = analogRead(BATTERY_LEVEL_PIN) * 3.3 / 512;
    batteryLevel[0] = max(0, min(100, (int)((voltage - 3.6) / 0.6 * 100)));
    ble.updateCharacteristicValue(batteryLevelChar.getValueAttribute().getHandle(), batteryLevel, sizeof(batteryLevel));

    // IMU
    if (imuAvailable) {
      imuDevice.read();
      memset(imuData, 0, sizeof(imuData));
      if (!imuDevice.timeoutOccurred()) {
        imuData[0] = imuDevice.a.x;
        imuData[1] = imuDevice.a.y;
        imuData[2] = imuDevice.a.z;
        imuData[3] = imuDevice.m.x;
        imuData[4] = imuDevice.m.y;
        imuData[5] = imuDevice.m.z;
      }
      if (gyroAvailable) {
        gyro.read();
        if (!gyro.timeoutOccurred()) {
          imuData[6] = gyro.g.x;
          imuData[7] = gyro.g.y;
          imuData[8] = gyro.g.z;
        }
      }
      ble.updateCharacteristicValue(imuChar.getValueAttribute().getHandle(), (uint8_t*)imuData, sizeof(imuData));
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Purple Eye Nano!");

  // Set up storage - to store servo offsets
  // Following two lines are a workaround for a bug - storageConfig is not initialized correctly for some reason.
  // So we set up the configuration here, and override the original one.
  fs_config_t storageConfig2 = { .cb = storageCallback, .num_pages = 1, .page_order = 0 };
  memcpy(&storageConfig, &storageConfig2, sizeof(storageConfig));
  if (fs_init() == NRF_SUCCESS) {
    storageAvailable = true;
    loadServoOffsets();
  }

  Wire.begin();
  imuAvailable = imuDevice.init();
  imuDevice.setTimeout(1);
  imuDevice.enableDefault();
  gyroAvailable = gyro.init();
  gyro.setTimeout(1);
  gyro.enableDefault();

  // LED consumes about 0.3ma, so we turn it off.
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 1);

  pinMode(BATTERY_LEVEL_PIN, INPUT);
  pinMode(RIGHT_LEG_PIN, OUTPUT);
  pinMode(RIGHT_FOOT_PIN, OUTPUT);
  pinMode(LEFT_FOOT_PIN, OUTPUT);
  pinMode(LEFT_LEG_PIN, OUTPUT);
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
  ble.addService(imuService);
  ble.addService(playerService);
  ble.setDeviceName((const uint8_t *)DEVICE_NAME);
  ble.setTxPower(4);
  ble.setAdvertisingInterval(160); // (100 ms = 160 * 0.625ms.)
  ble.setAdvertisingTimeout(0);
  ble.startAdvertising();

  // Battery level task
  batteryTask.attach(sensorsCallback, 0.1);

  // Override the system event handler - this is required for fstorage
  softdevice_sys_evt_handler_set(sysEventHandler);
}

void loop() {
  ble.waitForEvent();
}
