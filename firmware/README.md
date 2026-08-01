# Feeder Firmware

The ESP32-S3 firmware identifies each pet, checks whether feeding is allowed, dispenses food, weighs
the bowl, and keeps commands and feeding records in sync with Firebase.

## Build and upload

```bash
cp feeder/secrets.h.example feeder/secrets.h
scripts/setup.sh
scripts/build.sh
scripts/upload.sh
scripts/monitor.sh
```

Fill `secrets.h` with 2.4 GHz Wi-Fi, Firebase device credentials, the database and storage
hosts, and the LAN address of the Vision Host.

## Hardware and setup

The feeder uses a Waveshare ESP32-S3 N16R8. The Arducam Mega 5MP SPI camera uses CS on GPIO 10 and
SPI on GPIO 11–13, the NAU7802 uses I²C on GPIO 8/9, and the ULN2003 stepper driver uses GPIO 39–42.

Use the `@pf` serial console to set up and test each part: zero the scale, move the drum to its
starting position, check the camera and Vision Host connection, and enable the feeder. Run
`system.hello` to see the available setup and test commands.
