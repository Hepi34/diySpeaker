/* 
based on code from https://www.akkudoktor.net/forum/open-source-software-projekte/jkbms-auslesen-ueber-ble-bluetooth-oder-rs485-adapter-mittels-eps-iobroker/paged/49/
Big thanks to Scotty89 and others for their incredible brains and sharing 
CAN Code updated for ESP32 S3 versions. (CAN routines deprocated in S3,  renamamed TWAI. Should now work on ESP32, and S3 variants
DO NOT INSTALL ESP32 BLE Library by Neil Kolban! - if its in your Documents/arduino/Libraries folder , remove it!
The ESP32 BLE Lib is now installed and maintained as part of the ESP32 by Espressif boards manager package
Added MTU 517 in connection.ino to stop S3 crashing! , CAN works but displays need sorting! Can delays be reduced ?
added charging reduction at Hi dV or high SOC. Main Display sorted, Needs cell display and CAN Display?
Fixed charge algorithm, Cell volts array changed to int, Added Cell 1-8 display
Added Cell screen for upto 16 cells & CAN screen
Changed scan detect to BLE address works quicker and device name is completely un reliable in this and nimble libs
Added touch display changing, timeout, Charge limiting gave inverter errors?? reviewed lifepoe charge volts
Adding Reading of Alarm flag bits - wrong byte positions!
Over & Under temp warnings added. Configured and Working on PCB.  
Display now sleeps, added Cells 9-16 display, Added Display activetimeout (Pixel burn in saver, new cell screen, Graphics on BLE scan and boot screen)
TO DO .... Support & Test with JKBMS V11 Hardware!?
*/
//DONT MAKE COMMENT BLOCKS TOO BIG - Arduino IDE can throw odd compilation errors!!
/*
Release R2.0
Last updated 28-05-2024
Written by Steve Tearle
Before using:-
Specify your number of Cells ...usually 8 15 or 16
Set the BLE address of your JKBMS 
Specify your required Charge V and I specs and limits
Set if you want CAN active true false
*/



#define SW_Version "R2.0"

bool debug_flg = true;
bool debug_flg_full_log = false;

//########### Hardware Settings / Options  #########

//BMS-BLE Settings
const char* Geraetename = "JK-BD4A8S4P";     //JK-B2A24S20P JK-B2A24S15P
const char* BLE_ADDR = "98:da:20:06:ec:db";  //This is the BLE MAC address of your specific JK BMS - find in serial monitor or use BLE app on phone

//Set your BMS Hardware Version - Data parsing is different wrong or erratic values given if not correct
#define BMS_Version 11  //  is your BMS Hardware /Firmware Ver 10 or 11? - see B.T App 'About'

// Set your number of cells
#define cell_count 7

//########### End of "User" Settings #######################################################################

static BLEUUID serviceUUID("ffe0");  // The remote service we wish to connect to.
static BLEUUID charUUID("ffe1");     //ffe1 // The characteristic of the remote service we are interested in.
BLEClient* pClient;
BLEScan* pBLEScan;
byte getdeviceInfo[20] = { 0xaa, 0x55, 0x90, 0xeb, 0x97, 0x00, 0xdf, 0x52, 0x88, 0x67, 0x9d, 0x0a, 0x09, 0x6b, 0x9a, 0xf6, 0x70, 0x9a, 0x17, 0xfd };  // Device Infos
byte getInfo[20] = { 0xaa, 0x55, 0x90, 0xeb, 0x96, 0x00, 0x79, 0x62, 0x96, 0xed, 0xe3, 0xd0, 0x82, 0xa1, 0x9b, 0x5b, 0x3c, 0x9c, 0x4b, 0x5d };

unsigned long sendingtime = 0;
unsigned long bleScantime = 0;

unsigned long newdatalasttime = 0;
unsigned long ble_connection_time = 0;

unsigned long LastLoopTime = 0;
int LoopCount = 0;
bool Interval_First_Loop_Pass = false;

byte receivedBytes_main[320];
int frame = 0;
bool received_start = false;
bool received_start_frame = false;
bool received_complete = false;
bool new_data = false;
byte BLE_Scan_counter = 0;

//BMS Values
int cellVoltage[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  //make this bigger if you have more than 16 cells
int Cell_MaxV = 0;                                                         //this is int,  /1000.0 to get Volts
int Cell_MinV = 0;

//float cellVoltage[32];
float balanceWireResist[32];
float Average_Cell_Voltage = 0;
float Delta_Cell_Voltage = 0;
int Current_Balancer;
//float Battery_Voltage = 0;
//float Battery_Power = 0;
//float Charge_Current = 0;
float Battery_T1 = 0;
float Battery_T2 = 0;

float MOS_Temp = 0;
//int SOC_Percent = 0;
float Capacity_Remain = 0;
float Nominal_Capacity = 0;
String Cycle_Count = "";
float Capacity_Cycle = 0;
uint32_t Uptime;
uint8_t sec, mi, hr, days;
float Balance_Curr = 0;
//String charge = "off";
//String discharge = "off";
bool LowTemp = false;  //Various Alarm / Error  flags ....
bool HighTemp = false;
bool CellOverVoltage = false;
bool CellUnderVoltage = false;

static bool doConnect = false;
static bool ble_connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

unsigned long Alive_packet_counter = 0;

//--------------------------------------------------------


static void notifyCallback (BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  char data[320];
  String topic;
  if (debug_flg_full_log) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");

    for (int i = 0; i < length; i++) {
      Serial.print(pData[i], HEX);
      Serial.print(", ");
    }
    Serial.println("");
  }
  if (pData[0] == 0x55 && pData[1] == 0xAA && pData[2] == 0xEB && pData[3] == 0x90 && pData[4] == 0x02) {
    //Serial.printf("Found Frame Start...");
    received_start = true;
    received_start_frame = true;
    received_complete = false;
    frame = 0;
    for (int i = 0; i < length; i++) {
      receivedBytes_main[frame] = pData[i];
      frame++;
    }
  }

  if (received_start && !received_start_frame && !received_complete) {
    //     Serial.println("Daten erweitert !");


    for (int i = 0; i < length; i++) {
      receivedBytes_main[frame] = pData[i];
      frame++;
    }
    if (frame == 300) {
      //Serial.println("...and Frame Complete");
      received_complete = true;
      received_start = false;
      new_data = true;
      BLE_Scan_counter = 0;
      char data[20];

      if (debug_flg_full_log) {
        for (int i = 0; i <= frame; i++) {
          sprintf(data, "Frame:%d 0x%02X ", i, receivedBytes_main[i]);
          Serial.print(data);
        }
      }
    }
    if ((frame > 300)) {

      Serial.println("Fehlerhafte Daten !!");
      frame = 0;
      received_start = false;
      new_data = false;
    }
  }
  //Serial.print("frame: ");
  //Serial.println(frame);
  received_start_frame = false;
}

class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    ble_connected = false;
    //pclient->disconnect();
    Serial.println("BLE-Disconnect");
    delay(200);
    Serial.println("BLE was Disconnected ... and no BLE reconnection possible, Reboot ESP...");
    ESP.restart();
  }
};

//=================================================================================================
/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  /**
       Called for each advertising BLE server.
    */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    delay(100);



    // We have found our BMS address & device
    // Lets see if it contains the service we are looking for. BLE_ADDR Serial.println(advertisedDevice.getAddress().toString());
    // if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID) && advertisedDevice.getName() == Geraetename) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID) && (advertisedDevice.getAddress() == BLEAddress(BLE_ADDR))) {

      delay(500);
      //DisplayBMSFound();

      pBLEScan->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      pBLEScan->clearResults();

    }  // Found our server
  }    // onResult
};     // MyAdvertisedDeviceCallbacks

//************* SETUP FOLLOWS *************************************************

void setupBLE() {
  //BLE Setup
  BLEDevice::init("");
  pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(260);  //was 1349 ?!
  pBLEScan->setWindow(220);    //was 449  ?!
  pBLEScan->setActiveScan(true);
}

//***************** MAIN LOOP **************************************************
void bleLoop() {
  //Manage BLE
  if (doConnect == true) {
    if (!connectToBLEServer()) {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");

      delay(500);
      ble_connected = false;
      doConnect = false;
    }
  }

  if (ble_connected) {

    if (received_complete) {
      //      for (int i = 0; i < 319; i++)  {
      //        Serial.print(receivedBytes_main[i],HEX);
      //       Serial.print(", ");
      //      }
      //      Serial.println("");
      if (new_data) {
        ParseData();
        Get_Max_Min_Cell_Volts();
        newdatalasttime = millis();
      }
    }

    // BLE Get Device Data Trigger ...
    if (((millis() - sendingtime) > 500) && sendingtime != 0) {  // millis() >  sendingtime + sendingtimer aktive_sending &&
      sendingtime = 0;
      Serial.println("Sending Info Request");
      //aktive_sending = false;
      pRemoteCharacteristic->writeValue(getInfo, 20);
    }
  }

  //BLE nicht verbunden
  if ((!ble_connected && !doConnect && (millis() - bleScantime) > 4000)) {  //time was 15000, trying 4000

    Serial.println("BLE  ESP.restart();-> Reconnecting " + String(BLE_Scan_counter));
    bleScantime = millis();
    pBLEScan->start(3, false);  //this is a blocking call ...code carries on after 3 secs of scanning.....
    BLE_Scan_counter++;
  }

  // BLE verbidnugn ist da aber es kommen seit X Sekunden keine neuen Daten !
  if (!doConnect && ble_connected && (millis() >= (newdatalasttime + 60000)) && newdatalasttime != 0) {
    ble_connected = false;
    delay(200);
    newdatalasttime = millis();
    pClient->disconnect();
  }

  //checker das nach max 5 Minuten und keiner BLE Verbidung neu gestartet wird...
  if (BLE_Scan_counter > 20) {
    delay(200);
    Serial.println("BLE isn´t receiving new Data form BMS... and no BLE reconnection possible, Reboot ESP...");
  }
}


bool connectToBLEServer() {

  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  pClient->setClientCallbacks(new MyClientCallback());
  delay(500);  // hope it helps against ->  lld_pdu_get_tx_flush_nb HCI packet count mismatch (0, 1)
  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  delay(500);                  // hope it helps against ->  lld_pdu_get_tx_flush_nb HCI packet count mismatch (0, 1)
  Serial.println(" - Connected to server");

  //Added next line VITAL TO STOP ESP32 S3 / LILYGO T-DISPLAY S3 Crashing at this point!
  pClient->setMTU(517);  //set client to request maximum MTU from server (default is 23 otherwise)


  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notify the characteristic");
  }

  // Sending getdevice info
  pRemoteCharacteristic->writeValue(getdeviceInfo, 20);
  sendingtime = millis();
  Serial.println("Sending device Info");

  ble_connected = true;
  doConnect = false;
  Serial.println("We are now connected to the BLE Server.");

  return true;
}

//FYI...this  EspHome code outlines data frame protocol https://github.com/syssi/esphome-jk-bms/blob/main/components/jk_bms_ble/jk_bms_ble.cpp

void ParseData() {

  uint8_t j, i;
  if (BMS_Version == 11) {
    //Cell voltages  Zellspannungen
    for (j = 0, i = 7; i < 70; j++, i += 2) {
      cellVoltage[j] = (((int)receivedBytes_main[i] << 8 | receivedBytes_main[i - 1]));
      //cellVoltage[j] = cellVoltage[j] + 0.0005;       // so that it rounds "correctly", to 3 decimal places 0.0005 usw.
      //cellVoltage[j] = (int)(cellVoltage[j] * 1000);  // Here the float *100 is calculated and cast to int, so all other decimal places are omitted
      //cellVoltage[j] = cellVoltage[j] / 1000;
    }

    // +20 byte Zelle 15 - 24
    // +4 Byte unbekannt

    Average_Cell_Voltage = (((int)receivedBytes_main[75] << 8 | receivedBytes_main[74]) * 0.001);
    Delta_Cell_Voltage = (((int)receivedBytes_main[77] << 8 | receivedBytes_main[76]) * 0.001);
    Current_Balancer = receivedBytes_main[78];

    // +48 byte Resistance_Cell1
    for (j = 0, i = 81; i < (81 + 48); j++, i += 2) {
      balanceWireResist[j] = (((int)receivedBytes_main[i] << 8 | receivedBytes_main[i - 1]));
    }


    Battery_Voltage = (((int)receivedBytes_main[153] << 24 | receivedBytes_main[152] << 16 | receivedBytes_main[151] << 8 | receivedBytes_main[150]) * 0.001);
    Battery_Power = (((int)receivedBytes_main[157] << 24 | (int)receivedBytes_main[156] << 16 | (int)receivedBytes_main[155] << 8 | (int)receivedBytes_main[154]) * 0.001);
    Charge_Current = (((int)receivedBytes_main[161] << 24 | receivedBytes_main[160] << 16 | receivedBytes_main[159] << 8 | receivedBytes_main[158]) * 0.001);


    if (receivedBytes_main[163] == 0xFF) {
      Battery_T1 = ((0xFF << 24 | 0xFF << 16 | (int)receivedBytes_main[163] << 8 | (int)receivedBytes_main[162]) * 0.1);
    } else {
      Battery_T1 = (((int)receivedBytes_main[163] << 8 | (int)receivedBytes_main[162]) * 0.1);
    }


    if (receivedBytes_main[133] == 0xFF) {
      Battery_T2 = ((0xFF << 24 | 0xFF << 16 | (int)receivedBytes_main[165] << 8 | (int)receivedBytes_main[164]) * 0.1);
    } else {
      Battery_T2 = (((int)receivedBytes_main[165] << 8 | (int)receivedBytes_main[164]) * 0.1);
    }

    if (receivedBytes_main[145] == 0xFF) {
      MOS_Temp = ((0xFF << 24 | 0xFF << 16 | (int)receivedBytes_main[15] << 8 | (int)receivedBytes_main[144]) * 0.1);
    } else {
      MOS_Temp = (((int)receivedBytes_main[145] << 8 | (int)receivedBytes_main[144]) * 0.1);
    }


    if ((receivedBytes_main[171] & 0xF0) == 0x0) {
      Balance_Curr = (((int)receivedBytes_main[171] << 8 | receivedBytes_main[170]) * 0.001);
    } else if ((receivedBytes_main[171] & 0xF0) == 0xF0) {
      Balance_Curr = ((((int)receivedBytes_main[171] & 0x0F) << 8 | receivedBytes_main[170]) * -0.001) + 2.000;
    }
    // +2 byte unbekant 4
    SOC_Percent = ((int)receivedBytes_main[173]);
    Capacity_Remain = (((int)receivedBytes_main[177] << 24 | receivedBytes_main[176] << 16 | receivedBytes_main[175] << 8 | receivedBytes_main[174]) * 0.001);
    Nominal_Capacity = (((int)receivedBytes_main[181] << 24 | receivedBytes_main[180] << 16 | receivedBytes_main[179] << 8 | receivedBytes_main[178]) * 0.001);
    Cycle_Count = receivedBytes_main[185] + receivedBytes_main[184] + receivedBytes_main[183] + receivedBytes_main[182];


    // +6 byte unbekant 5
    Capacity_Cycle = (((int)receivedBytes_main[193] << 8 | receivedBytes_main[192]) * 0.001);
    Uptime = (((int)receivedBytes_main[196] << 16 | receivedBytes_main[195] << 8 | receivedBytes_main[194]));
    sec = Uptime % 60;
    Uptime /= 60;
    mi = Uptime % 60;
    Uptime /= 60;
    hr = Uptime % 24;
    days = Uptime /= 24;

    // +1 byte unknown  unbekannt
    if (receivedBytes_main[198] > 0) {
      charge = "on";
    } else if (receivedBytes_main[198] == 0) {
      charge = "off";
    }
    if (receivedBytes_main[199] > 0) {
      discharge = "on";
    } else if (receivedBytes_main[199] == 0) {
      discharge = "off";
    }
  }
}
//==================================================================================================
void Get_Max_Min_Cell_Volts(void) {

  Cell_MaxV = cellVoltage[0];
  Cell_MinV = cellVoltage[0];

  for (int i = 0; i < cell_count; i++) {
    Cell_MaxV = max(cellVoltage[i], Cell_MaxV);
    Cell_MinV = min(cellVoltage[i], Cell_MinV);
  }
  // Serial.print("The maximum value is: "); Serial.println(Cell_MaxV);
  // Serial.print("The minimum value is: "); Serial.println(Cell_MinV);
}
