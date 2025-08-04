# diySpeaker
Enthält die ESP32- und DSP-Programmdateien, die ich in meinem selbst gebauten Lautsprecher verwendet habe, welchen ich als Maturaarbeit gebaut habe.

[Englisch](https://github.com/Hepi34/diySpeaker/edit/main/README.md) | [Deutsch](https://github.com/Hepi34/diySpeaker/blob/main/README_GER.md)

* [Haupt-ESP-Code](#Haupt-ESP-Code)
* [Zweit-ESP-Code](#Zweit-ESP-Code)
* [DSP-Code](#DSP-Code)

--------

# Haupt-ESP-Code
Der Code des Haupt-ESPs besteht aus zwei Dateien: esp32main.ino und JKBMS_BLE.ino. Die esp32main-Datei enthält all den Code, welcher für die Hauptlogik gebraucht wird. Darunter auch Logik für den Bildschirm, den Rotary Encoder und für die DSP-Befehle. Die JKBMS_BLE-Datei ist [vom Internet heruntergeladener Code](https://github.com/SteveintheIoW/T-Display-S3-JK-BMS-BLE-to-Solis-CAN-Pylontech/tree/main), welchen ich für meine eigenen Bedürfnisse angepasst und modifiziert habe. Der heruntergeladene Code enthält das reverse-engeneerte JKBMS-BLE-Protokoll, welches gebruacht wird, um den ESP via BLE mit dem BMS zu verbinden und Werte auszulesen. Die JKBMS_BLE-Datei enthält nun nur noch Code welcher für mein BMS und mein spezifisches Projekt gebraucht werden.

# Zweit-ESP-Code
Genau wie der Code vom Haupt-ESP besteht der Code des zweiten ESPs aus zwei Dateien: esp32secondQuick.ino und JKBMS_BLE.ino. Die esp32secondQuick-Datei enthält all den Code, welchen für den kabellosen Subwoofer gebraucht wird. "Quick" wurde zum Namen der Datei hinzugefügt, als ich von der normalen ESPnow-Bibliothek zu der QuickESPNow-Bibliothek gewechselt habe, um die implementierung einfacher zu gestalten. Die JKBMS_BLE-Datei ist dieselbe wie die, die ich im Hauptlautsprecher verwendet habe.

# DSP-Code
Das DSP-Programm ist in einer App namens [SigmaStudio](https://www.analog.com/en/resources/evaluation-hardware-and-software/embedded-development-software/ss_sigst_02.html#software-overview) programmiert, welche gebraucht wird, um DSP-Chips von Analog Devices zu programmieren. Die DSP-Projektdatei heisst 1452_cs42448_48.dspproj und enthält alle Logik welche für den DSP in meinem Lautpsprecher benötigt wird. Das Projekt enthält mehrere sogenannte Subboards, welche man unten in Sigmastudio sehen kann.
