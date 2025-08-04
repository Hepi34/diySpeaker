#include <WiFi.h>
#include <QuickEspNow.h>
#include "BLEDevice.h"
#include "Arduino.h"

// The MAC address of the first ESP32 (Receiver)
uint8_t peerMac[] = { 0x94, 0xa9, 0x90, 0x31, 0x2c, 0x5c };  // Replace with actual MAC address of the receiver

// Dummy data variables
int batteryLevel = 0;  // Dummy battery level (0-100)

int lastESPsend = 0;
int lastBLEcheck = 0;

int SOC_Percent = 0;
float Charge_Current = 0;
float Battery_Voltage = 0;
float Battery_Power = 0;
String discharge = "off";
String charge = "off";

int charging = 0;

#define BMS_BUTTON 15  //GPIO to measure ON/OFF switch

#define POWER_RELAY 17  //GPIO for Power SW Relay

#define AMP_RELAY 18  //GPIO for Amp Relay

// Constants
const unsigned long mainButtonPressTime = 2500;  // 3s long press
const unsigned long doublePressInterval = 1500;  // 2s window for double press

// State variables
bool mainButtonPressed = false;
unsigned long mainButtonTime = 0;

bool ampMuted = true;

unsigned long lastPressTime = 0;
int pressCount = 0;

bool turnOffSoon = false;  //After holding for 3s, unmute Amp if it is not off after another 4 seconds
int turnOffTimeout = 4000;
int turnOffTrigger = 0;
bool turnOffTriggered = false;

int remoteTurnOffStart = 0;

bool receivedTurnOffCommand = false;

void dataReceived(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {

  String receivedData = String((char*)data, len);  // Convert received byte array to String

  Serial.println(receivedData);

  if (receivedData.equalsIgnoreCase("turn off") && !receivedTurnOffCommand) {
    digitalWrite(POWER_RELAY, HIGH);

    remoteTurnOffStart = millis();

    receivedTurnOffCommand = true;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(AMP_RELAY, OUTPUT);
  digitalWrite(AMP_RELAY, HIGH);

  pinMode(POWER_RELAY, OUTPUT);
  digitalWrite(POWER_RELAY, LOW);


  pinMode(BMS_BUTTON, INPUT_PULLDOWN);

  // Initialize WiFi in station mode (no connection to a network)
  WiFi.mode(WIFI_MODE_STA);

  WiFi.disconnect(false);
  quickEspNow.onDataRcvd(dataReceived);
  quickEspNow.begin(6);  // If you are not connected to WiFi, channel should be specified

  Serial.println("QuickESPNow sender setup complete");

  setupBLE();
  Serial.println("BLE setup done");
}

void loop() {
  // Prepare the data to send (battery level and signal strength)

  if (digitalRead(BMS_BUTTON) == HIGH) {
    // Button is currently being held
    if (!mainButtonPressed) {
      mainButtonPressed = true;
      mainButtonTime = millis();
    } else {
      // Check for long press
      if (millis() - mainButtonTime >= mainButtonPressTime) {
        if (!ampMuted) {
          digitalWrite(AMP_RELAY, HIGH);  // Mute the amp
          ampMuted = true;
          turnOffSoon = true;
        }
      }
    }
  } else {
    // Button is released
    if (mainButtonPressed) {
      // Check if it was a short press
      if (millis() - mainButtonTime < mainButtonPressTime) {
        // Handle double press
        if (millis() - lastPressTime < doublePressInterval) {
          pressCount++;

          if (pressCount == 2) {
            ampMuted = !ampMuted;
            digitalWrite(AMP_RELAY, ampMuted ? HIGH : LOW);
            pressCount = 0;
          }
        } else {
          // First short press
          pressCount = 1;
        }

        lastPressTime = millis();
      }

      mainButtonPressed = false;
    }

    if (pressCount == 1 && (millis() - lastPressTime) > doublePressInterval) {
      pressCount = 0;
    }
  }


  if (turnOffSoon) {
    if (!turnOffTriggered) {
      // Start waiting for turn off
      turnOffTrigger = millis();
      turnOffTriggered = true;
    } else {
      if (millis() >= turnOffTimeout + turnOffTrigger) {
        //It didn't turn off if it reached this
        turnOffTriggered = false;
        turnOffSoon = false;
        mainButtonPressed = false;
        if (ampMuted) {
          digitalWrite(AMP_RELAY, LOW);
          ampMuted = false;
        }
      }
    }
  }

  if (millis() == remoteTurnOffStart + 4990) {
    digitalWrite(POWER_RELAY, LOW);
  }


  if (millis() > lastBLEcheck + 1000) {
    bleLoop();
    lastBLEcheck = millis();
  }

  if (millis() > lastESPsend + 2000) {

    // Send the data to the first ESP32
    if (discharge == "off") {
      charging = 1;
    } else {
      charging = 0;
    }

    String data = String(Battery_Voltage) + "," + String(Charge_Current) + "," + String(Battery_Power) + "," + String(SOC_Percent) + "," + String(charging);
    quickEspNow.send(peerMac, (uint8_t*)data.c_str(), data.length());
    Serial.println(data);
    Serial.println("Sent Data");
    lastESPsend = millis();
  }
}
