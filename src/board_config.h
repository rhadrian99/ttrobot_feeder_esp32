#pragma once

#include <Arduino.h>

// Configuratie pini si LED per placa.
// Se selecteaza automat in functie de tinta de compilare (WROOM vs C3).

// 0 fixeaza sensul normal si ascunde controlul din pagina de setari.
#define ENABLE_DIRECTION_CONTROL 0

#if defined(CONFIG_IDF_TARGET_ESP32C3)

namespace Pins {
constexpr uint8_t Step = 7;
constexpr uint8_t Dir = 6;
constexpr uint8_t Enable = 10;
constexpr uint8_t Button = 5;
constexpr uint8_t StatusLed = 8;
constexpr uint8_t HallSensor = 4;
constexpr uint8_t TmcUartRx = 0;
constexpr uint8_t TmcUartTx = 1;
}  // namespace Pins

// LED onboard pe C3 supermini este activ pe LOW.
constexpr uint8_t StatusLedActiveLevel = LOW;
constexpr uint8_t HallSensorActiveLevel = LOW;
constexpr const char *BoardName = "ESP32-C3 supermini";

// Marker de identitate firmware pentru validarea OTA (per placa).
constexpr const char *FirmwareIdentityPrefix = "TTROBOT_FEEDER_C3_FW:";
constexpr uint32_t kMicrostepsPerStep = 4;

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
constexpr uint8_t HallSensor = 4;
}  // namespace Pins

// LED onboard pe WROOM este activ pe HIGH.
constexpr uint8_t StatusLedActiveLevel = HIGH;
constexpr uint8_t HallSensorActiveLevel = LOW;
constexpr const char *BoardName = "ESP32-WROOM";

// Marker de identitate firmware pentru validarea OTA (per placa).
// Pastrat neschimbat pentru compatibilitate OTA cu firmware-ul existent.
constexpr const char *FirmwareIdentityPrefix = "TTROBOT_FEEDER_ESP32_FW:";
constexpr uint32_t kMicrostepsPerStep = 8;

#endif

// Constante motor/driver comune ambelor placi.
constexpr uint32_t kMotorStepsPerRevolution = 200;

inline float constrainFloat(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}
