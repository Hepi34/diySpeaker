#include <Wire.h>
#include <U8g2lib.h>
#include <Arduino.h>
#include <QuickEspNow.h>
#include <WiFi.h>
#include <string>
#include <Adafruit_MCP4728.h>
#include "BLEDevice.h"

Adafruit_MCP4728 mcp;

// OLED Display Setup
U8G2_SH1107_PIMORONI_128X128_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Rotary Encoder Pins
#define ENC_A 16
#define ENC_B 17
#define ENC_SW 21

// DSP MP Pins
#define MP6_PIN 10  // GPIO for MP6
#define MP7_PIN 11  // GPIO for MP7
#define MP9_PIN 12  // GPIO for MP9

#define BMS_BUTTON 15  //GPIO to measure ON/OFF switch

#define AMP_RELAY 13  //GPIO for Amp Relay
#define SUB_RELAY 14  //GPIO for Sub TX module relay

//Encoder rotation
volatile int encoderPos = 0;
volatile bool encoderChanged = false;

//Encoder button
bool buttonPressed = false;

//Encoder timeout
unsigned long lastEncoderMove = 0;
unsigned long lastButtonPress = 0;

//Encoder debounce
const int encoderDelay = 50;          // Debounce delay for encoder
const int buttonDebounceDelay = 400;  // Debounce delay for button press

// On / Off button pressed
bool mainButtonPressed = false;
int mainButtonTime = 0;
int mainButtonPressTime = 3000;  //  3s hold for Amp to mute (3s press is likely turning off)
bool turnOffSoon = false;        //After holding for 3s, unmute Amp if it is not off after another 4 seconds
int turnOffTimeout = 4000;
int turnOffTrigger = 0;
bool turnOffTriggered = false;

// Define variables to hold the current states
bool dspState[4] = { false, false, false, false };      // On/Off states for the 4 DSP options -> Dynamic bass, Harmonic Bass, Bypass DSP, Loudness Boost
bool prevdspState[4] = { false, false, false, false };  // On/Off states for the 4 previous DSP options
bool extSubState = false;                               // On/Off state for the External Subwoofer
bool auxInput = false;                                  // False for BT, True for AUX
int volume = 0;                                         // int 0 - 20 for analog volume via DAC
bool changeVolume = false;                              // Volume changing in progress
int eq[3] = { 20, 20, 20 };                             // Low, Med, High
bool eqState[3] = { false, false, false };              // Low, med, high change
bool prevExtSubState = false;
bool prevAuxInput = false;

bool SubRemoteTurnOff = true;
bool sentTurnOffCommand = false;

// Boot timeout
bool bootDone = false;

// DSP boot chime
int bootChimeDuration = 4000;
int bootChimePlayTime = 0;
bool bootChimePlayed = false;

// DSP low battery sound
int lowBatteryPlayTime = 0;
int lowBatteryDuration = 1500;
bool lowBatteryPlayed = false;

// BLE vars
int SOC_Percent = 0;
float Charge_Current = 0;
float Battery_Voltage = 0;
float Battery_Power = 0;
String discharge = "off";
String charge = "off";

bool lowBattWarn = true;
int lastLowBattPlayTime = 0;
int lowBattRepeatTime = 30000;  // 60s, half the value (30s) because every 2nd run turns the chime block off and doesnt sound

// Signal strength default
int signalStrength = 0;

// Timeout tracking
unsigned long lastActivityTime = 0;
const unsigned long timeoutDuration = 10000;  // 10 seconds timeout duration
const unsigned long espnowTimeout = 10000;
int lastBLEcheck = 0;
int BLEtimeout = 1000;

// DAC updates
bool volumeChanged = false;
bool eqChanged = false;

// ESPNOW Variables
int remoteBattery = 0;
float Remote_Voltage = 0;
float Remote_CCurrent = 0;
float Remote_Power = 0;
int remoteCharging = 0;

unsigned long lastRecvTime = 0;           // Last time data was received
const unsigned long timeout_recv = 5000;  // Timeout in milliseconds (5 seconds)

// FW Info
const char* fw_ver = "0.7.2";
const char* build_date = "22/06/2025";
const char* author = "Noah Joss";


// Menu Structure (Expandable)
struct MenuItem {
  const char* label;
  struct MenuItem* submenu;  // Submenu pointer
  int submenuSize;           // Number of items in the submenu
};

// Main Menu Items
MenuItem inputMenu[] = {
  { "Bluetooth", nullptr, 2 },
  { "AUX", nullptr, 2 },
  { "Back", nullptr, 0 },  // "Back" option
};

MenuItem volumeMenu[] = {
  { "Adjust", nullptr, 2 },  // Placeholder for volume control options
  { "Back", nullptr, 0 },    // "Back" option
};

// EQ and DSP Menus
MenuItem eqMenu[] = {
  { "Low", nullptr, 2 },   // 2 options in "Low"
  { "Mid", nullptr, 2 },   // 2 options in "Med"
  { "High", nullptr, 2 },  // 2 options in "High"
  { "Back", nullptr, 0 },  // "Back" option
};

MenuItem dspMenu[] = {
  { "Dyn. Bass", nullptr, 2 },    // On/Off
  { "Harm. Bass", nullptr, 2 },   // On/Off
  { "Loud. Boost", nullptr, 2 },  // On/Off
  { "Bypass DSP", nullptr, 2 },   // On/Off
  { "Back", nullptr, 0 },         // "Back" option
};

MenuItem extSubMenu[] = {
  { "On/Off", nullptr, 0 },      // On/Off
  { "Remote off", nullptr, 0 },  // Remote turn off On/Off
  { "Back", nullptr, 0 },        // "Back" option
};

MenuItem battMenu[] = {
  { "L. Batt. Warn.", nullptr, 0 },
  { "Main Volt.", nullptr, 0 },
  { "Sub Volt.", nullptr, 0 },
  { "M. Ch. Curr.", nullptr, 0 },
  { "Sub Ch. Curr.", nullptr, 0 },
  { "Main Pwr.", nullptr, 0 },
  { "Sub Pwr.", nullptr, 0 },
  { "Back", nullptr, 0 },
};

MenuItem infoMenu[] = {
  { "FW Version", nullptr, 0 },
  { "Build Date", nullptr, 0 },
  { "Author", nullptr, 0 },
  { "Back", nullptr, 0 },
};

// Main Menu
MenuItem mainMenu[] = {
  { "Volume", volumeMenu, 2 },    // 2 options in Volume menu
  { "Input", inputMenu, 3 },      // 3 options in Input menu
  { "EQ", eqMenu, 4 },            // 4 options in EQ menu
  { "DSP", dspMenu, 4 },          // 4 options in DSP menu
  { "Ext. Sub", extSubMenu, 2 },  // 2 options in Ext. Sub menu
  { "Battery", battMenu, 7 },
  { "FW Info", infoMenu, 4 },
};

int mainMenuSize = sizeof(mainMenu) / sizeof(mainMenu[0]);

int selectedIndex = 0;  //Currently selected option
int menuLevel = 0;      // Representing menu hierarchy (0 for main, 1 for submenu1, etc.)


// Rotary Encoder Interrupt Handler
void IRAM_ATTR readEncoder() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastEncoderMove > encoderDelay) {
    if (digitalRead(ENC_A) == digitalRead(ENC_B)) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    encoderChanged = true;
    lastEncoderMove = currentMillis;
    lastActivityTime = currentMillis;  // Reset inactivity timer
  }
}

// Button Interrupt Handler (with debounce)
void IRAM_ATTR readButton() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonPress > buttonDebounceDelay) {
    buttonPressed = true;
    lastButtonPress = currentMillis;
    lastActivityTime = currentMillis;  // Reset inactivity timer
  }
}

// Function to handle receiving data
void dataReceived(uint8_t* address, uint8_t* data, uint8_t len, signed int rssi, bool broadcast) {
  lastRecvTime = millis();  // Update last received time
  //Serial.printf("RSSI:" + rssi);

  String receivedData = String((char*)data, len);  // Convert received byte array to String

  // Split the string into components
  int comma1 = receivedData.indexOf(',');
  int comma2 = receivedData.indexOf(',', comma1 + 1);
  int comma3 = receivedData.indexOf(',', comma2 + 1);
  int comma4 = receivedData.indexOf(',', comma3 + 1);

  // Extract substrings for each value
  String voltageStr = receivedData.substring(0, comma1);
  String currentStr = receivedData.substring(comma1 + 1, comma2);
  String powerStr = receivedData.substring(comma2 + 1, comma3);
  String socStr = receivedData.substring(comma3 + 1, comma4);
  String chargingStr = receivedData.substring(comma4 + 1);

  // Convert to appropriate types
  Remote_Voltage = voltageStr.toFloat();
  Remote_CCurrent = currentStr.toFloat();
  Remote_Power = powerStr.toFloat();
  remoteBattery = socStr.toInt();
  remoteCharging = chargingStr.toInt();

  if (rssi == 0) {
    signalStrength = 0;  // No connection or poor signal, set to 0
  } else if (rssi > -60) {
    signalStrength = 4;  // Excellent signal
  } else if (rssi > -70) {
    signalStrength = 3;  // Good signal
  } else if (rssi > -80) {
    signalStrength = 2;  // Poor signal
  } else {
    signalStrength = 1;  // Very poor signal
  }
}

uint8_t peerAddress[] = { 0xb4, 0x3a, 0x45, 0xa9, 0x1f, 0xc0 };

void setupESPNOW() {
  // Initialize ESPNOW
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect(false);
  // Set up callback for receiving data
  quickEspNow.onDataRcvd(dataReceived);

  // Specific MAC address for ESP2 (94:a9:90:31:2c:5c) WRONG, FINE NOW

  quickEspNow.begin(6);  // If you are not connected to WiFi, channel should be specified
}


void setup() {
  Serial.begin(115200);

  Wire.begin(6, 7);  // Your ESP32-S3 I2C pins
  //Wire.setClock(400000);  // Use 400kHz for faster communication

  display.setDisplayRotation(U8G2_R1);  // Rotate 90 degrees clockwise

  display.begin();

  drawSetupText(70, "Booting", "Pin setup", true);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  pinMode(MP6_PIN, INPUT);  // MP6 (GPIO 10)
  pinMode(MP7_PIN, INPUT);  // MP7 (GPIO 11)
  pinMode(MP9_PIN, INPUT);  // MP9 (GPIO 12)

  pinMode(BMS_BUTTON, INPUT_PULLDOWN);

  pinMode(AMP_RELAY, OUTPUT);
  digitalWrite(AMP_RELAY, HIGH);

  pinMode(SUB_RELAY, OUTPUT);
  digitalWrite(SUB_RELAY, HIGH);

  drawSetupText(70, "Booting", "Interrupt setup", true);

  attachInterrupt(digitalPinToInterrupt(ENC_A), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_SW), readButton, FALLING);

  drawSetupText(70, "Booting", "BLE setup", true);

  setupBLE();

  delay(5000);

  drawSetupText(70, "Booting", "ESPnow setup", true);

  setupESPNOW();

  drawSetupText(70, "Booting", "DAC setup", true);

  if (!mcp.begin()) {
    Serial.println("Failed to find MCP4728 DAC");
    drawSetupText(70, "Booting", "Failed to find DAC", true);
    while (1)
      ;
  }

  Serial.println("MCP4728 ready");

  updateDAC();

  drawSetupText(70, "Booting", "DSP Correction", true);

  //?? Seems like the DSP misinterprets pin pulses or something, IDK
  delay(500);
  pulsePin(MP7_PIN, 1);

  drawSetupText(70, "Booting", "Boot chime", true);
  digitalWrite(AMP_RELAY, LOW);
  bootChimePlayTime = millis();
  delay(3000);
  pulsePin(MP6_PIN, 1);
  delay(500);
  pulsePin(MP6_PIN, 1);

  drawSetupText(70, "Booting", "Setup done", true);

  Serial.println("Setup done");
}

void loop() {
  unsigned long currentMillis = millis();
  if (!bootDone) {
    drawSetupText(70, "Booting", "Waiting for BMS", true);
    if (SOC_Percent != 0) {
      lastActivityTime = millis();
      updateDisplay();
      bootDone = true;
    }
    if (millis() > bootChimePlayTime + bootChimeDuration && !bootChimePlayed) {
      bootChimePlayed = true;
      digitalWrite(AMP_RELAY, HIGH);
    }
  }

  // If BMS buttons pressed...
  if (digitalRead(BMS_BUTTON) == HIGH) {
    // If the button was already pressed before (holding)
    if (mainButtonPressed) {
      // 2 seconds after the 1st press
      if (millis() > mainButtonTime + mainButtonPressTime) {
        //If the button is still pressed
        if (digitalRead(BMS_BUTTON) == HIGH) {
          // Turn off amp, turn off sub if applicable and wait for turn off
          digitalWrite(AMP_RELAY, HIGH);
          if (SubRemoteTurnOff) {
            String data = "Turn off";
            quickEspNow.send(peerAddress, (uint8_t*)data.c_str(), data.length());
            Serial.println("sent turn off comamnd");
            sentTurnOffCommand = true;
          }
          turnOffSoon = true;
        }
        // If no longer pressed reset
        else {
          mainButtonPressed = false;
        }
      }
    }
    // If the button is pressed the 1st time in a while.
    else {
      mainButtonPressed = true;
      mainButtonTime = millis();
    }
  }

  if (turnOffSoon) {
    if (!turnOffTriggered) {
      // Start waiting for turn off
      turnOffTrigger = millis();
      turnOffTriggered = true;
    } else {
      if (millis() > turnOffTimeout + turnOffTrigger) {
        //It didn't turn off if it reached this
        turnOffTriggered = false;
        turnOffSoon = false;
        mainButtonPressed = false;
        if (volume > 0) {
          digitalWrite(AMP_RELAY, LOW);
        }
      }
    }
  }

  // BMS BLE once per second
  if (millis() > lastBLEcheck + BLEtimeout) {
    bleLoop();
    lastBLEcheck = millis();
  }

  // Sub battery timeout
  if (millis() - lastRecvTime > espnowTimeout) {
    remoteBattery = 0;  // Reset battery value
    signalStrength = 0;
    Remote_Voltage = 0;
    Remote_CCurrent = 0;
    Remote_Power = 0;
    remoteCharging = 0;
  }

  if (lowBattWarn) {
    if (millis() - lastLowBattPlayTime > lowBattRepeatTime) {
      if ((SOC_Percent < 10 || Battery_Voltage < 21.0) && Charge_Current <= 0) {
        pulsePin(MP6_PIN, 2);
        lastLowBattPlayTime = millis();
      } else if ((remoteBattery < 10 || Remote_Voltage < 21.0) && Remote_CCurrent <= 0 && extSubState) {
        pulsePin(MP6_PIN, 2);
        lastLowBattPlayTime = millis();
      }
    }
  }

  // Clear display after timeout
  if (currentMillis - lastActivityTime > timeoutDuration) {
    // OLED Timeout
    if (bootDone) {
      display.clearBuffer();  // Clear display
      display.sendBuffer();   // Send empty buffer to turn off the display
    }
  } else {
    if (encoderChanged && bootDone) {
      encoderChanged = false;
      encoderPos = encoderPos > 0 ? 1 : -1;
      if (changeVolume) {
        // Ensure the volume stays within bounds
        if (volume >= 20 && encoderPos == 1) {
          volume = 20;
        } else if (volume <= 0 && encoderPos == -1) {
          volume = 0;
        } else {
          volume += encoderPos;  // Modify the volume by the encoder position (increment or decrement based on encoder direction)
        }
        volumeChanged = true;
        if (volume != 0) {
          digitalWrite(AMP_RELAY, LOW);
        } else {
          digitalWrite(AMP_RELAY, HIGH);
        }

      } else if (eqState[0]) {
        if (eq[0] >= 20 && encoderPos == 1) {
          eq[0] = 20;
        } else if (eq[0] <= 0 && encoderPos == -1) {
          eq[0] = 0;
        } else {
          eq[0] += encoderPos;  // Modify the volume by the encoder position (increment or decrement based on encoder direction)
        }
        eqChanged = true;

      } else if (eqState[1]) {
        if (eq[1] >= 20 && encoderPos == 1) {
          eq[1] = 20;
        } else if (eq[1] <= 0 && encoderPos == -1) {
          eq[1] = 0;
        } else {
          eq[1] += encoderPos;  // Modify the volume by the encoder position (increment or decrement based on encoder direction)
        }
        eqChanged = true;

      } else if (eqState[2]) {
        if (eq[2] >= 20 && encoderPos == 1) {
          eq[2] = 20;
        } else if (eq[2] <= 0 && encoderPos == -1) {
          eq[2] = 0;
        } else {
          eq[2] += encoderPos;  // Modify the volume by the encoder position (increment or decrement based on encoder direction)
        }
        eqChanged = true;

      } else {
        selectedIndex = (selectedIndex + encoderPos + getMenuSize()) % getMenuSize();
      }
      encoderPos = 0;
      updateDisplay();
    }

    if (buttonPressed && bootDone) {
      buttonPressed = false;

      // Handle the "Back" button for all submenus
      if ((menuLevel != 0) && strcmp(getCurrentMenu()[selectedIndex].label, "Back") == 0) {
        menuLevel = 0;      // Go back to the main menu
        selectedIndex = 0;  // Reset selected index to the first option in main menu
      }
      // Handle selection in the main menu
      else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "Volume") == 0) {
        menuLevel = 1;  // Enter Volume menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "Input") == 0) {
        menuLevel = 2;  // Enter Input menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "EQ") == 0) {
        menuLevel = 3;  // Enter EQ menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "DSP") == 0) {
        menuLevel = 4;  // Enter DSP menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "Ext. Sub") == 0) {
        menuLevel = 5;  // Enter Ext. Sub menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "Battery") == 0) {
        menuLevel = 6;  // Enter Ext. Sub menu
        selectedIndex = 0;
      } else if (menuLevel == 0 && strcmp(mainMenu[selectedIndex].label, "FW Info") == 0) {
        menuLevel = 7;  // Enter Ext. Sub menu
        selectedIndex = 0;
      } else if (menuLevel == 5 && strcmp(extSubMenu[selectedIndex].label, "On/Off") == 0) {
        extSubState = !extSubState;
        digitalWrite(SUB_RELAY, extSubState ? LOW : HIGH);
        updateDSPState();
      } else if (menuLevel == 5 && strcmp(extSubMenu[selectedIndex].label, "Remote off") == 0) {
        SubRemoteTurnOff = !SubRemoteTurnOff;
      } else if (menuLevel == 1 && strcmp(volumeMenu[selectedIndex].label, "Adjust") == 0) {
        changeVolume = !changeVolume;
      } else if (menuLevel == 2 && strcmp(inputMenu[selectedIndex].label, "Bluetooth") == 0) {
        auxInput = false;
        updateDSPState();
      } else if (menuLevel == 2 && strcmp(inputMenu[selectedIndex].label, "AUX") == 0) {
        auxInput = true;
        updateDSPState();
      } else if (menuLevel == 4 && strcmp(dspMenu[selectedIndex].label, "Dyn. Bass") == 0) {
        dspState[0] = !dspState[0];
        updateDSPState();
      } else if (menuLevel == 4 && strcmp(dspMenu[selectedIndex].label, "Harm. Bass") == 0) {
        dspState[1] = !dspState[1];
        updateDSPState();
      } else if (menuLevel == 4 && strcmp(dspMenu[selectedIndex].label, "Bypass DSP") == 0) {
        dspState[2] = !dspState[2];
        updateDSPState();
      } else if (menuLevel == 4 && strcmp(dspMenu[selectedIndex].label, "Loud. Boost") == 0) {
        dspState[3] = !dspState[3];
        updateDSPState();
      } else if (menuLevel == 3 && strcmp(eqMenu[selectedIndex].label, "Low") == 0) {
        eqState[0] = !eqState[0];
      } else if (menuLevel == 3 && strcmp(eqMenu[selectedIndex].label, "Mid") == 0) {
        eqState[1] = !eqState[1];
      } else if (menuLevel == 3 && strcmp(eqMenu[selectedIndex].label, "High") == 0) {
        eqState[2] = !eqState[2];
      } else if (menuLevel == 6 && strcmp(battMenu[selectedIndex].label, "L. Batt. Warn.") == 0) {
        lowBattWarn = !lowBattWarn;
      }
      updateDisplay();
    }

    if (volumeChanged || eqChanged) {
      updateDAC();
      volumeChanged = false;
      eqChanged = false;
    }
  }
}

void updateDSPState() {
  // For active-low control (better approach)
  //pinMode(10, dspState[0] ? OUTPUT : INPUT);
  //pinMode(11, dspState[1] ? OUTPUT : INPUT);
  //pinMode(12, dspState[2] ? OUTPUT : INPUT);

  if (dspState[0] != prevdspState[0]) {
    pulsePin(MP9_PIN, 1);
    prevdspState[0] = dspState[0];
  } else if (dspState[1] != prevdspState[1]) {
    pulsePin(MP9_PIN, 2);
    prevdspState[1] = dspState[1];
  } else if (dspState[2] != prevdspState[2]) {
    pulsePin(MP9_PIN, 3);
    prevdspState[2] = dspState[2];
  } else if (dspState[3] != prevdspState[3]) {
    pulsePin(MP7_PIN, 3);
    prevdspState[3] = dspState[3];
  }

  if (extSubState != prevExtSubState) {
    pulsePin(MP7_PIN, 2);
    prevExtSubState = extSubState;
  } else if (auxInput != prevAuxInput) {
    Serial.println("Aux MP PIN pulsed");
    pulsePin(MP7_PIN, 1);
    prevAuxInput = auxInput;
  }


  //digitalWrite(10, !dspState[0]);  // LOW when active, HIGH when inactive
  //digitalWrite(11, !dspState[1]);
  //digitalWrite(12, !dspState[2]);
}

void pulsePin(int pin, int pulsecount) {
  for (int i = 0; i < pulsecount; i++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(10);
    pinMode(pin, INPUT);
    delay(10);
  }
}

void updateDAC() {
  mcp.setChannelValue(MCP4728_CHANNEL_A, mapVolumeToDAC(volume));
  mcp.setChannelValue(MCP4728_CHANNEL_B, mapEQToDAC(eq[0]));
  mcp.setChannelValue(MCP4728_CHANNEL_C, mapEQToDAC(eq[1]));
  mcp.setChannelValue(MCP4728_CHANNEL_D, mapEQToDAC(eq[2]));
}

int mapVolumeToDAC(int volume) {
  return map(volume, 0, 20, 0, 4095);
}

int mapEQToDAC(int eqValue) {
  return map(eqValue, 0, 20, 0, 4095);
}

MenuItem* getCurrentMenu() {
  // Return the current menu based on the current menu level
  if (menuLevel == 0) {
    return mainMenu;
  } else if (menuLevel == 1) {
    return volumeMenu;
  } else if (menuLevel == 2) {
    return inputMenu;
  } else if (menuLevel == 3) {
    return eqMenu;
  } else if (menuLevel == 4) {
    return dspMenu;
  } else if (menuLevel == 5) {
    return extSubMenu;
  } else if (menuLevel == 6) {
    return battMenu;
  } else if (menuLevel == 7) {
    return infoMenu;
  }

  return nullptr;  // Default fallback if no valid menu level
}

int getMenuSize() {
  // Return the size of the current menu based on menuLevel
  if (menuLevel == 0) {
    return mainMenuSize;  // Main menu size (from mainMenu)
  } else if (menuLevel == 1) {
    return sizeof(volumeMenu) / sizeof(volumeMenu[0]);  // Volume menu size
  } else if (menuLevel == 2) {
    return sizeof(inputMenu) / sizeof(inputMenu[0]);  // Input menu size
  } else if (menuLevel == 3) {
    return sizeof(eqMenu) / sizeof(eqMenu[0]);  // EQ menu size
  } else if (menuLevel == 4) {
    return sizeof(dspMenu) / sizeof(dspMenu[0]);  // DSP menu size
  } else if (menuLevel == 5) {
    return sizeof(extSubMenu) / sizeof(extSubMenu[0]);  // Ext. Sub menu size
  } else if (menuLevel == 6) {
    return sizeof(battMenu) / sizeof(battMenu[0]);  // Batt menu size
  } else if (menuLevel == 7) {
    return sizeof(infoMenu) / sizeof(infoMenu[0]);  // Info menu size
  }
  return 0;  // Default fallback if no valid level
}

void drawMenuLevel(int y) {
  display.setFont(u8g2_font_ncenB08_tr);  // Smaller font for the menu level text

  const char* menuLevelText = "";

  // Set the text based on the menu level
  if (menuLevel == 0) {
    menuLevelText = "Main Menu";
  } else if (menuLevel == 1) {
    menuLevelText = "Volume Menu";
  } else if (menuLevel == 2) {
    menuLevelText = "Input Menu";
  } else if (menuLevel == 3) {
    menuLevelText = "EQ Menu";
  } else if (menuLevel == 4) {
    menuLevelText = "DSP Menu";
  } else if (menuLevel == 5) {
    menuLevelText = "Ext. Sub Menu";
  } else if (menuLevel == 6) {
    menuLevelText = "Battery Menu";
  } else if (menuLevel == 7) {
    menuLevelText = "FW Info";
  }

  // Get the width of the menu level text to center it
  int textWidth = display.getStrWidth(menuLevelText);
  int textX = (128 - textWidth) / 2;  // Calculate the X position to center the text

  // Draw the centered menu level text
  display.drawStr(textX, y, menuLevelText);
}

void drawMenuDescription(int y) {
  display.setFont(u8g2_font_ncenB08_tr);  // Smaller font for descriptions

  const char* descriptionText = "";

  // Draw the description based on the menu level and the selected item
  if (menuLevel == 0) {
    descriptionText = "";  // No description for main menu
  } else if (menuLevel == 2) {
    if (selectedIndex == 0 || selectedIndex == 1) {
      descriptionText = auxInput ? "Current: AUX" : "Current: Bluetooth";
    }
  } else if (menuLevel == 3) {
    if (selectedIndex == 0) {
      static char descriptionBuffer[50];  // Use a buffer for sprintf
      sprintf(descriptionBuffer, "Current: %d", eq[0]);
      descriptionText = descriptionBuffer;  // Point to the formatted text
    } else if (selectedIndex == 1) {
      static char descriptionBuffer[50];  // Use a buffer for sprintf
      sprintf(descriptionBuffer, "Current: %d", eq[1]);
      descriptionText = descriptionBuffer;  // Point to the formatted text
    } else if (selectedIndex == 2) {
      static char descriptionBuffer[50];  // Use a buffer for sprintf
      sprintf(descriptionBuffer, "Current: %d", eq[2]);
      descriptionText = descriptionBuffer;  // Point to the formatted text
    }
  } else if (menuLevel == 1) {
    if (selectedIndex == 0) {
      static char descriptionBuffer[50];  // Use a buffer for sprintf
      sprintf(descriptionBuffer, "Current: %d", volume);
      descriptionText = descriptionBuffer;  // Point to the formatted text
    }
  } else if (menuLevel == 4) {
    // DSP menu - show current state based on selected index
    if (selectedIndex == 0) {
      descriptionText = dspState[0] ? "Current: On" : "Current: Off";
    } else if (selectedIndex == 1) {
      descriptionText = dspState[1] ? "Current: On" : "Current: Off";
    } else if (selectedIndex == 2) {
      descriptionText = dspState[3] ? "Current: On" : "Current: Off";
    } else if (selectedIndex == 3) {
      descriptionText = dspState[2] ? "Current: On" : "Current: Off";
    }
  } else if (menuLevel == 5) {
    // External Sub menu - show current state based on selected index
    if (selectedIndex == 0) {
      descriptionText = extSubState ? "Current: On" : "Current: Off";
    } else if (selectedIndex == 1) {
      descriptionText = SubRemoteTurnOff ? "Current: On" : "Current: Off";
    }
  } else if (menuLevel == 6) {
    if (strcmp(getCurrentMenu()[selectedIndex].label, "Back") != 0) {
      static char descriptionBuffer[50];  // Use a buffer for sprintf
      float var1 = 0;
      switch (selectedIndex) {
        case 0:
          descriptionText = lowBattWarn ? "Current: On" : "Current: Off";
          break;
        case 1:
          var1 = Battery_Voltage;
          sprintf(descriptionBuffer, "Current: %.2f V", var1);
          descriptionText = descriptionBuffer;  // Point to the formatted text
          break;
        case 2:
          var1 = Remote_Voltage;
          sprintf(descriptionBuffer, "Current: %.2f V", var1);
          descriptionText = descriptionBuffer;  // Point to the formatted text
          break;
        case 3:
          var1 = Charge_Current;
          sprintf(descriptionBuffer, "Current: %.2f A", var1);
          descriptionText = descriptionBuffer;  // Point to the formatted text
          break;
        case 4:
          var1 = Remote_CCurrent;
          sprintf(descriptionBuffer, "Current: %.2f A", var1);
          descriptionText = descriptionBuffer;  // Point to the formatted text

          break;
        case 5:
          var1 = Battery_Power;
          if (Charge_Current <= 0) {
            sprintf(descriptionBuffer, "Current: -%.2f W", var1);
          } else {
            sprintf(descriptionBuffer, "Current: %.2f W", var1);
          }
          descriptionText = descriptionBuffer;  // Point to the formatted text

        case 6:
          var1 = Remote_Power;
          if (Remote_CCurrent <= 0) {
            sprintf(descriptionBuffer, "Current: -%.2f W", var1);
          } else {
            sprintf(descriptionBuffer, "Current: %.2f W", var1);
          }
          descriptionText = descriptionBuffer;  // Point to the formatted text

        default:
          var1 = -1;
          break;
      }
    }
  } else if (menuLevel == 7) {
    // External Sub menu - show current state based on selected index
    if (selectedIndex == 0) {
      descriptionText = fw_ver;
    } else if (selectedIndex == 1) {
      descriptionText = build_date;
    } else if (selectedIndex == 2) {
      descriptionText = author;
    }
  }

  // Get the width of the description text to center it
  int textWidth = display.getStrWidth(descriptionText);
  int textX = (128 - textWidth) / 2;  // Calculate the X position to center the text

  // Draw the centered description text below the menu
  display.drawStr(textX, y, descriptionText);
}

void drawBattery(int x, int y, int percent, bool showLightning) {
  int width = 20;
  int height = 10;
  display.setFont(u8g2_font_ncenB08_tr);  // Smaller font for battery percentage
  display.drawFrame(x, y, width, height);
  display.drawBox(x + width, y + 3, 2, 4);  // Battery nub
  int fillWidth = (width - 2) * percent / 100;
  display.drawBox(x + 1, y + 1, fillWidth, height - 2);
  char percentStr[5];
  sprintf(percentStr, "%d%%", percent);
  display.drawStr(x + width + 5, y + 8, percentStr);  // Battery percentage text
  if (showLightning) {
    int boltX = x + width / 2 - 2;
    int boltY = y + 2;

    display.setDrawColor(2);  // XOR mode

    // Bolt shape: Zig-zag style, 5 pixels tall
    display.drawLine(boltX + 1, boltY, boltX + 3, boltY + 2);
    display.drawLine(boltX + 3, boltY + 2, boltX + 2, boltY + 2);
    display.drawLine(boltX + 2, boltY + 2, boltX + 4, boltY + 5);

    display.setDrawColor(1);  // Restore normal mode
  }
}

void drawSignalBars(int x, int y, int signalStrength) {
  int barWidth = 3;
  int baseBarHeight = 7;
  int maxHeight = 14;  // Maximum height for the strongest signal bar

  // Draw 4 bars, representing signal strength
  for (int i = 0; i < 4; i++) {
    // Calculate the height of the bar based on signal strength
    int currentHeight = baseBarHeight + (i * 2);  // Increase height as i increases

    if (i < signalStrength) {
      // Draw filled bar
      display.drawBox(x + i * (barWidth + 2), y - currentHeight, barWidth, currentHeight);
    } else {
      // Draw empty bar (outline)
      display.drawFrame(x + i * (barWidth + 2), y - currentHeight, barWidth, currentHeight);
    }
  }
  if (signalStrength == 0) {
    display.drawStr(x - 10, y, "X");  // Draw the "X" character
  }
}

void drawSetupText(int y, const char* bigTxt, const char* txt, bool clear) {
  if (clear) {
    display.clearBuffer();  // Clear display
  }

  display.setFont(u8g2_font_ncenB14_tr);
  int textWidth = display.getStrWidth(bigTxt);
  int centerX = (128 - textWidth) / 2;  // 128 is the width of the OLED screen
  display.drawStr(centerX, y, bigTxt);

  display.setFont(u8g2_font_ncenB08_tr);  // Smaller font for descriptions
  int textWidth2 = display.getStrWidth(txt);
  int centerX2 = (128 - textWidth2) / 2;  // 128 is the width of the OLED screen
  display.drawStr(centerX2, y + 20, txt);

  display.sendBuffer();  // Update the OLED screen with the new content
}

void updateDisplay() {
  display.clearBuffer();

  // Draw battery indicators
  drawBattery(5, 5, SOC_Percent, Charge_Current > 0.1);
  drawBattery(80, 5, remoteBattery, Remote_CCurrent > 0.1);  // Adjusted X to prevent it from going off screen

  // Draw signal bars (below battery)
  drawSignalBars(110, 30, signalStrength);  // Positioned below the battery icons

  drawMenuLevel(70);  // Adjust the vertical position as necessary

  // Draw menu with larger text
  display.setFont(u8g2_font_ncenB14_tr);  // Larger font for menu text

  const char* menuText = "";

  // Determine the correct menu to display based on menuLevel
  if (menuLevel == 0) {
    menuText = mainMenu[selectedIndex].label;
  } else if (menuLevel == 1) {
    menuText = volumeMenu[selectedIndex].label;
  } else if (menuLevel == 2) {
    menuText = inputMenu[selectedIndex].label;
  } else if (menuLevel == 3) {
    menuText = eqMenu[selectedIndex].label;
  } else if (menuLevel == 4) {
    menuText = dspMenu[selectedIndex].label;
  } else if (menuLevel == 5) {
    menuText = extSubMenu[selectedIndex].label;
  } else if (menuLevel == 6) {
    menuText = battMenu[selectedIndex].label;
  } else if (menuLevel == 7) {
    menuText = infoMenu[selectedIndex].label;
  }

  // Calculate the width of the text to center it
  int textWidth = display.getStrWidth(menuText);
  int centerX = (128 - textWidth) / 2;  // 128 is the width of the OLED screen

  // Draw centered menu text
  display.drawStr(centerX, 90, menuText);

  drawMenuDescription(110);


  display.sendBuffer();  // Update the OLED screen with the new content
}
