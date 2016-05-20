/*********************************************************************
  This is an example for our nRF51822 based Bluefruit LE modules

  Pick one up today in the adafruit shop!

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  MIT license, check LICENSE for more information
  All text above, and the splash screen below must be included in
  any redistribution
*********************************************************************/

#include <Arduino.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Servo.h"

#define BLUEFRUIT_SPI_CS               8
#define BLUEFRUIT_SPI_IRQ              7
#define BLUEFRUIT_SPI_RST              4

#define RIGHT_LEG_PIN 5
#define RIGHT_FOOT_PIN 11
#define LEFT_FOOT_PIN 9
#define LEFT_LEG_PIN 10

const uint8_t BLE_ADVERTISE_PACKET[] = {
  // Advertise The Servo Service
  0x02,
  0x01,
  0x06,
  0x05,
  0x02,
  0x00, 0x51, // Service ID
  0x0a,
  0x18,

  // Eddystone URL Beacon. See https://www.mkompf.com/tech/eddystoneurl.html
  0x03,  // Length of Service List
  0x03,  // Param: Service List
  0xAA, 0xFE,  // Eddystone ID
  0x11,  // Length of Service Data
  0x16,  // Service Data
  0xAA, 0xFE, // Eddystone ID
  0x10,  // Frame type: URL
  0xF8, // Power
  0x02, // http://
  'b',
  'i',
  't',
  '.',
  'd',
  'o',
  '/',
  'p',
  'r',
  'p',
  'l',
};

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

Servo rightFoot, rightLeg, leftFoot, leftLeg;
int rightFootValue = 0, rightLegValue = 0, leftFootValue = 0, leftLegValue = 0;

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void updateServos() {
  if (leftFootValue == 0 && leftLegValue == 0 && rightFootValue == 0 && rightFootValue == 0) {
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
    rightFoot.write(rightFootValue + 15);
    rightLeg.write(rightLegValue);
    leftFoot.write(leftFootValue - 10);
    leftLeg.write(leftLegValue);
  }
}

int32_t eyebotServiceId;
int32_t eyebotServosCharId;

void setup(void)
{
  boolean success;

  updateServos();

  Serial.begin(115200);
  Serial.println("Start!");

  randomSeed(micros());

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(true) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  /* Perform a factory reset to make sure everything is in a known state */
  Serial.println(F("Performing a factory reset: "));
  if (! ble.factoryReset() ) {
    error(F("Couldn't factory reset"));
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  /* Change the device name to make it easier to find */
  Serial.println("Setting device name");

  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=PurpleEye")) ) {
    error(F("Could not set device name?"));
  }

  /* Add the Serve Service definition */
  /* Service ID should be 1 */
  Serial.println(F("Adding the service definition (UUID = 0x5100): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x5100"), &eyebotServiceId);
  if (! success) {
    error(F("Could not add service"));
  }

  /* Chars ID for Servo position should be 1 */
  Serial.println(F("Adding the characteristic (UUID = 0x5200): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x5200, PROPERTIES=0x0A, MIN_LEN=4, MAX_LEN=4, VALUE=00-00-00-00"), &eyebotServosCharId);
  if (! success) {
    error(F("Could not add HRM characteristic"));
  }

  /* Add the Servo Service and Eddystone Beacon to the advertising data */
  Serial.println(F("Adding Servo Service UUID to the advertising payload: "));

  // Encode advertise packet as hex string
  char bleAdvStr[sizeof(BLE_ADVERTISE_PACKET) * 3];
  for (int i = 0; i < sizeof(BLE_ADVERTISE_PACKET); i++) {
    sprintf(&bleAdvStr[i * 3], "%02x-", BLE_ADVERTISE_PACKET[i]);
  };
  bleAdvStr[sizeof(bleAdvStr) - 1] = 0;
  char cmdBuf[256];
  snprintf(cmdBuf, sizeof(cmdBuf), "AT+GAPSETADVDATA=%s", bleAdvStr);
  ble.sendCommandCheckOK( cmdBuf );

  /* Reset the device for the new service setting changes to take effect */
  Serial.print(F("Performing a SW reset (service changes require a reset): "));
  ble.reset();

  Serial.println();
}

void loop(void)
{
  /* Command is sent when \n (\r) or println is called */
  /* AT+GATTCHAR=CharacteristicID,value */
  ble.print("AT+GATTCHAR=");
  ble.println(eyebotServosCharId);

  /* Check if command executed OK */
  char buf[100] = {0};
  ble.readline(buf, sizeof(buf));
  sscanf(buf, "%02x-%02x-%02x-%02x", &rightLegValue, &rightFootValue, &leftFootValue, &leftLegValue);
  updateServos();
}

