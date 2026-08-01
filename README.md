# PetVision

PetVision is a multi-pet feeder that identifies the animal at the bowl, checks its schedule and
daily allowance, and dispenses the correct food. The ESP32 handles feeding decisions and safety
checks; a local GPU service supplies pet embeddings, Firebase connects the feeder to the companion
app.

- [`firmware/`](firmware): ESP32-S3 feeder control and setup
- [`vision-host/`](vision-host): local detection and embedding service
- [`app/`](app): installable React companion app
- [`cloud/`](cloud): Firebase rules, hosting, and account setup
- [`stl/`](stl): printable models for the dispenser and load-cell assembly

## Run

```bash
npm install
cp .firebaserc.example .firebaserc
cp app/.env.example app/.env
npm run dev
```

See each component README to set up Firebase, start the Vision Host, and build the firmware.

## Hardware

- Waveshare ESP32-S3 N16R8
- Arducam Mega 5MP SPI
- NAU7802 load-cell amplifier
- 2 kg load cell
- 28BYJ-48 stepper motor
- ULN2003 stepper driver

MIT licensed. See [LICENSE](LICENSE).
