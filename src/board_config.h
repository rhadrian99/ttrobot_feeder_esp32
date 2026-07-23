#pragma once

#include <Arduino.h>

// Configuratie pini si LED per placa.
// Se selecteaza automat in functie de tinta de compilare (WROOM vs C3).

#if defined(CONFIG_IDF_TARGET_ESP32C3)

namespace Pins {
constexpr uint8_t Step = 6;
constexpr uint8_t Dir = 7;
constexpr uint8_t Enable = 10;
constexpr uint8_t Button = 5;
constexpr uint8_t StatusLed = 8;
}  // namespace Pins

// LED onboard pe C3 supermini este activ pe LOW.
constexpr uint8_t StatusLedActiveLevel = LOW;
constexpr const char *BoardName = "ESP32-C3 supermini";

// Marker de identitate firmware pentru validarea OTA (per placa).
constexpr const char *FirmwareIdentityPrefix = "TTROBOT_FEEDER_C3_FW:";

#else

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

namespace Pins {
constexpr uint8_t Step = 25;
constexpr uint8_t Dir = 26;
constexpr uint8_t Enable = 27;
constexpr uint8_t Button = 14;
constexpr uint8_t StatusLed = LED_BUILTIN;
}  // namespace Pins

// LED onboard pe WROOM este activ pe HIGH.
constexpr uint8_t StatusLedActiveLevel = HIGH;
constexpr const char *BoardName = "ESP32-WROOM";

// Marker de identitate firmware pentru validarea OTA (per placa).
// Pastrat neschimbat pentru compatibilitate OTA cu firmware-ul existent.
constexpr const char *FirmwareIdentityPrefix = "TTROBOT_FEEDER_ESP32_FW:";

#endif
