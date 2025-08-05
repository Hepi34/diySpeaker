# diySpeaker
Contains the ESP32 and DSP programs used in my DIY Speaker project which I've built for my matura project.

[English](https://github.com/Hepi34/diySpeaker/edit/main/README.md) | [German](https://github.com/Hepi34/diySpeaker/blob/main/README_GER.md)

* [Main ESP Code](#Main-ESP-Code)
* [Second ESP Code](#Second-ESP-Code)
* [DSP Code](#DSP-Code)

--------

# Main ESP Code
The main ESP's code consists of two files: esp32main.ino and JKBMS_BLE.ino. The esp32main file contains all the code needed to run the main logic, including the screen, the rotary encoder and the DSP commands. The JKBMS_BLE file contains [code downloaded from the internet](https://github.com/SteveintheIoW/T-Display-S3-JK-BMS-BLE-to-Solis-CAN-Pylontech/tree/main) which I then modified and adapted to my needs. The download contained the reverse engineering of the JK-BMS BLE protocol needed to connect the ESP to the BMS and to get its values via BLE. The JKBMS_BLE file now contains the necessary parts of the protocol needed for my specific project and BMS.
 
# Second ESP Code
Just like the code for the main ESP, the second ESP's code consists of two files: esp32secondQuick.ino and JKBMS_BLE.ino. The esp32secondQuick file contains all the logic needed to run the wireless subwoofer. Quick was added to the name when I switched from the regular ESPnow library to the QuickESPnow library for easier implementation. The JKBMS_BLE file is the same as the one used in the main speaker. 

# DSP Code
The DSP's code is programmed with an application called [SigmaStudio](https://www.analog.com/en/resources/evaluation-hardware-and-software/embedded-development-software/ss_sigst_02.html#software-overview), used to program DSP chips from Analog Devices. The DSP's project file is called 1452_cs42448_48.dspproj and it contains all the logic needed for the DSP in my speaker. The project contains multiple subpages, which can be seen at the bottom of the SigmaStudio program.
