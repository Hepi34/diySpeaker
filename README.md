# diySpeaker
Contains the ESP32 and DSP programs used in my DIY Speaker project which I've built for my matura project.

[English](https://github.com/Hepi34/diySpeaker/edit/main/README.md) | [German](https://github.com/Hepi34/diySpeaker/edit/main/README_GER.md)

* [Main ESP Code](#Main-ESP-Code)
* [Second ESP Code](#Second-ESP-Code)
* [DSP Code](#DSP-Code)

--------

# Main ESP Code
The main ESP's code consists of two files: esp32main.ino and JKBMS_BLE.ino. The esp32main file contains all the code needed to run the main logic, including the screen, the rotary encoder and the DSP commands. The JKBMS_BLE file is  [code downloaded from the internet](https://github.com/SteveintheIoW/T-Display-S3-JK-BMS-BLE-to-Solis-CAN-Pylontech/tree/main) which I then modified and adapted to my needs. The download contained the reverse engeneering of the JK-BMS BLE protocol needed to connect the ESP to the BMS and to get its values via BLE. The JKBMS_BLE file now contains the necessary parts of the protocol needed for my specific project and BMS.
 
# Second ESP Code

# DSP Code
