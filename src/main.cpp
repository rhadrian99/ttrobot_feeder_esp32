#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Preferences.h>
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#include <TMCStepper.h>
#endif

#include "FeederWebApp.h"
#include "board_config.h"

#define FW_VERSION "1.0.7"

constexpr bool EnableActiveLevel = LOW;
constexpr uint32_t DefaultMotorSpeedStepsPerSecond = 400;
constexpr uint32_t DefaultMotorAccelerationStepsPerSecond2 = 400;
constexpr float DefaultGearRatio = 1.0f;
constexpr uint32_t DebounceMillis = 35;
constexpr uint32_t MinMotorSpeedStepsPerSecond = 1;
constexpr uint32_t MaxMotorSpeedStepsPerSecond = 20000;
constexpr uint32_t MinMotorAccelerationStepsPerSecond2 = 100;
constexpr uint32_t MaxMotorAccelerationStepsPerSecond2 = 16000;
constexpr uint32_t MinRotationPreset = 4;
constexpr uint32_t MaxRotationPreset = 7;
constexpr uint32_t DefaultRotationPreset = 5;
constexpr float MinGearRatio = 1.0f;
constexpr float MaxGearRatio = 5.0f;
constexpr uint32_t StallTimeoutMultiplier = 2;
constexpr uint32_t MinStallTimeoutMs = 3000;
constexpr uint32_t MinValidRotationPercent = 50;
constexpr float JamRecoveryDegrees = 15.0f;
constexpr uint8_t JamRecoveryCycles = 3;
constexpr uint8_t MaxJamAttempts = 2;
constexpr uint32_t DefaultMotorRunDurationMinutes = 20;

#if defined(CONFIG_IDF_TARGET_ESP32C3)
constexpr uint32_t TmcUartBaudRate = 115200;
constexpr uint8_t TmcUartAddress = 0;
constexpr float TmcRsenseOhms = 0.11f;
constexpr float TmcHoldCurrentMultiplier = 0.5f;

#if TMC_DRIVER_MODEL == TMC_DRIVER_MODEL_2208
using SelectedTmcDriver = TMC2208Stepper;
constexpr uint8_t TmcExpectedVersion = 0x20;
constexpr const char *TmcDriverName = "TMC2208";
#else
using SelectedTmcDriver = TMC2209Stepper;
constexpr uint8_t TmcExpectedVersion = 0x21;
constexpr const char *TmcDriverName = "TMC2209";
#endif
#endif

constexpr uint32_t DefaultTmcRunCurrentMilliamps = 800;

enum class JamRecoveryState : uint8_t { Idle, MoveForward, MoveBackward, Finishing };

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
Preferences preferences;

#if defined(CONFIG_IDF_TARGET_ESP32C3)
HardwareSerial tmcSerial(1);
#if TMC_DRIVER_MODEL == TMC_DRIVER_MODEL_2208
SelectedTmcDriver tmcDriver(&tmcSerial, TmcRsenseOhms);
#else
SelectedTmcDriver tmcDriver(&tmcSerial, TmcRsenseOhms, TmcUartAddress);
#endif
#endif

uint32_t motorSpeedStepsPerSecond = DefaultMotorSpeedStepsPerSecond;
uint32_t motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
uint32_t motorMicrosteps = kMicrostepsPerStep;
uint32_t tmcRunCurrentMilliamps = DefaultTmcRunCurrentMilliamps;
uint32_t rotationPreset = DefaultRotationPreset;
float gearRatio = DefaultGearRatio;
bool reverseRotation = false;
uint32_t motorRunDurationMinutes = DefaultMotorRunDurationMinutes;

bool motorRunning = false;
bool motorJammed = false;
bool motorJammedPermanent = false;
uint32_t motorStartMillis = 0;
uint32_t motorSessionStartMillis = 0;
bool motorSessionActive = false;
JamRecoveryState jamRecoveryState = JamRecoveryState::Idle;
uint8_t jamRecoveryCycle = 0;
uint8_t jamAttemptCount = 0;
bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
bool statusLedState = LOW;
uint32_t lastDebounceChangeMillis = 0;
bool lastHallState = false;
uint32_t rotationsCounter = 0;
uint32_t lastHallTransitionMillis = 0;
uint32_t lastRotationTimeMs = 0;
bool tmcDiagnosticPending = false;
uint32_t tmcDiagnosticAtMillis = 0;
bool tmcDriverConnected = false;

void setMotorEnabled(bool enabled);
void toggleMotor();
void applyMotorSettings();
void updateMotorSpeedFromPreset();
void loadMotorSettings();
void saveMotorSettings();
void writeStatusLed(bool on);
void updateRotationCounter();
void updateMotorRunTimer();
void configureTmcDriver();

FeederWebApp::Dependencies buildWebDependencies() {
  FeederWebApp::Dependencies dependencies;
  dependencies.motorRunning = &motorRunning;
  dependencies.motorJammed = &motorJammed;
  dependencies.motorJammedPermanent = &motorJammedPermanent;
  dependencies.rotationCounter = &rotationsCounter;
  dependencies.rotationPeriodMs = &lastRotationTimeMs;
  dependencies.rotationPreset = &rotationPreset;
  dependencies.motorSpeedStepsPerSecond = &motorSpeedStepsPerSecond;
  dependencies.motorAccelerationStepsPerSecond2 = &motorAccelerationStepsPerSecond2;
  dependencies.motorMicrosteps = &motorMicrosteps;
  dependencies.tmcRunCurrentMilliamps = &tmcRunCurrentMilliamps;
  dependencies.gearRatio = &gearRatio;
  dependencies.reverseRotation = &reverseRotation;
  dependencies.motorRunDurationMinutes = &motorRunDurationMinutes;
  dependencies.motorSessionStartMillis = &motorSessionStartMillis;
  dependencies.motorSessionActive = &motorSessionActive;
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  dependencies.tmcSettingsAvailable = true;
  dependencies.tmcDriverConnected = &tmcDriverConnected;
  dependencies.tmcDriverName = TmcDriverName;
#endif
  dependencies.firmwareVersion = FW_VERSION;
  dependencies.saveMotorSettings = &saveMotorSettings;
  dependencies.applyMotorSettings = &applyMotorSettings;
  dependencies.toggleMotor = &toggleMotor;
  dependencies.setMotorEnabled = &setMotorEnabled;
  return dependencies;
}

FeederWebApp webApp(buildWebDependencies());

void applyMotorSettings() {
  updateMotorSpeedFromPreset();
  configureTmcDriver();

  if (stepper == nullptr) {
    return;
  }

  stepper->setSpeedInHz(motorSpeedStepsPerSecond);
  stepper->setAcceleration(motorAccelerationStepsPerSecond2);
}

void updateMotorSpeedFromPreset() {
  const float stepsPerOutputRotation = static_cast<float>(kMotorStepsPerRevolution * motorMicrosteps) * gearRatio;
  const float computedSpeed = stepsPerOutputRotation / static_cast<float>(rotationPreset);
  motorSpeedStepsPerSecond = static_cast<uint32_t>(lroundf(computedSpeed));
  motorSpeedStepsPerSecond = constrain(motorSpeedStepsPerSecond, MinMotorSpeedStepsPerSecond, MaxMotorSpeedStepsPerSecond);
}

void loadMotorSettings() {
  preferences.end();
  const bool opened = preferences.begin("feeder", true);
  Serial.printf("NVS begin read => %s\n", opened ? "OK" : "FAIL");
  if (opened) {
    motorAccelerationStepsPerSecond2 = preferences.getUInt("accel", DefaultMotorAccelerationStepsPerSecond2);
    gearRatio = preferences.getFloat("ratio", DefaultGearRatio);
    reverseRotation = preferences.getBool("reverse", false);
    rotationPreset = preferences.getUInt("rotationPreset", DefaultRotationPreset);
    motorRunDurationMinutes = preferences.getUInt("runMinutes", DefaultMotorRunDurationMinutes);
  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    motorMicrosteps = preferences.getUInt("microsteps", kMicrostepsPerStep);
    tmcRunCurrentMilliamps = preferences.getUInt("runCurrent", DefaultTmcRunCurrentMilliamps);
  #endif
    preferences.end();

    rotationPreset = constrain(rotationPreset, MinRotationPreset, MaxRotationPreset);
    motorAccelerationStepsPerSecond2 = constrain(motorAccelerationStepsPerSecond2, MinMotorAccelerationStepsPerSecond2, MaxMotorAccelerationStepsPerSecond2);
    gearRatio = constrainFloat(gearRatio, MinGearRatio, MaxGearRatio);
  #if !ENABLE_DIRECTION_CONTROL
    reverseRotation = false;
  #endif
    if (motorRunDurationMinutes != 10 && motorRunDurationMinutes != 15 && motorRunDurationMinutes != 20) {
      motorRunDurationMinutes = DefaultMotorRunDurationMinutes;
    }
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    if (motorMicrosteps != 4 && motorMicrosteps != 8 && motorMicrosteps != 16) {
      motorMicrosteps = kMicrostepsPerStep;
    }
    if (tmcRunCurrentMilliamps != 600 && tmcRunCurrentMilliamps != 800 &&
        tmcRunCurrentMilliamps != 900 && tmcRunCurrentMilliamps != 1000) {
      tmcRunCurrentMilliamps = DefaultTmcRunCurrentMilliamps;
    }
#endif
  } else {
    Serial.println("NVS: niciun setare salvata, folosesc valorile implicite");
    motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
    gearRatio = DefaultGearRatio;
    reverseRotation = false;
    rotationPreset = DefaultRotationPreset;
    motorRunDurationMinutes = DefaultMotorRunDurationMinutes;
    motorMicrosteps = kMicrostepsPerStep;
    tmcRunCurrentMilliamps = DefaultTmcRunCurrentMilliamps;
  }

  updateMotorSpeedFromPreset();

  Serial.printf("NVS load: speed=%lu accel=%lu ratio=%.2f preset=%lu reverse=%s microsteps=%lu runCurrent=%lu\n",
                static_cast<unsigned long>(motorSpeedStepsPerSecond),
                static_cast<unsigned long>(motorAccelerationStepsPerSecond2),
                gearRatio,
                static_cast<unsigned long>(rotationPreset),
                reverseRotation ? "true" : "false",
                static_cast<unsigned long>(motorMicrosteps),
                static_cast<unsigned long>(tmcRunCurrentMilliamps));
}

void saveMotorSettings() {
  preferences.end();
  const bool opened = preferences.begin("feeder", false);
  Serial.printf("NVS begin write => %s\n", opened ? "OK" : "FAIL");
  if (!opened) {
    Serial.println("NVS: nu pot deschide namespace-ul feeder pentru salvare");
    return;
  }

  preferences.putUInt("version", 1);
  preferences.putUInt("accel", motorAccelerationStepsPerSecond2);
  preferences.putFloat("ratio", gearRatio);
  preferences.putBool("reverse", reverseRotation);
  preferences.putUInt("rotationPreset", rotationPreset);
  preferences.putUInt("runMinutes", motorRunDurationMinutes);
  preferences.putUInt("microsteps", motorMicrosteps);
  preferences.putUInt("runCurrent", tmcRunCurrentMilliamps);
  preferences.end();
  Serial.printf("NVS save: speed=%lu accel=%lu ratio=%.2f preset=%lu reverse=%s microsteps=%lu runCurrent=%lu\n",
                static_cast<unsigned long>(motorSpeedStepsPerSecond),
                static_cast<unsigned long>(motorAccelerationStepsPerSecond2),
                gearRatio,
                static_cast<unsigned long>(rotationPreset),
                reverseRotation ? "true" : "false",
                static_cast<unsigned long>(motorMicrosteps),
                static_cast<unsigned long>(tmcRunCurrentMilliamps));
}

void setMotorEnabled(bool enabled) {
  const uint8_t enableLevel = enabled ? EnableActiveLevel : !EnableActiveLevel;
  digitalWrite(Pins::Enable, enableLevel);

  if (stepper == nullptr) {
    return;
  }

  if (enabled) {
    stepper->enableOutputs();
  } else {
    stepper->disableOutputs();
  }
}

void stopMotor(bool jammed) {
  if (stepper != nullptr) {
    stepper->stopMove();
  }
  rotationsCounter = 0;
  lastHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  lastHallTransitionMillis = 0;
  lastRotationTimeMs = 0;
  setMotorEnabled(false);
  motorRunning = false;
  motorJammed = jammed;
  jamRecoveryState = JamRecoveryState::Idle;
  jamRecoveryCycle = 0;
}

void startMotor(bool resetJamAttempts = true) {
  motorJammed = false;
  if (resetJamAttempts) {
    motorJammedPermanent = false;
    jamAttemptCount = 0;
    motorSessionStartMillis = millis();
    motorSessionActive = true;
  }
  rotationsCounter = 0;
  lastHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  lastHallTransitionMillis = 0;
  lastRotationTimeMs = 0;
  motorStartMillis = millis();
  setMotorEnabled(true);
  if (reverseRotation) {
    stepper->runBackward();
  } else {
    stepper->runForward();
  }
  motorRunning = true;
}

int32_t computeWiggleSteps() {
  const float outputStepsPerRotation = static_cast<float>(kMotorStepsPerRevolution * motorMicrosteps) * gearRatio;
  return static_cast<int32_t>(lroundf(outputStepsPerRotation * (JamRecoveryDegrees / 360.0f)));
}

void beginJamRecovery() {
  stopMotor(true);
  jamAttemptCount++;

  if (jamAttemptCount >= MaxJamAttempts) {
    motorJammedPermanent = true;
    motorSessionActive = false;
    Serial.println("Motor blocat definitiv! A doua blocare consecutiva, necesita interventie manuala.");
    return;
  }

  jamRecoveryCycle = 0;
  jamRecoveryState = JamRecoveryState::MoveForward;
  setMotorEnabled(true);
}

void updateJamRecovery() {
  if (jamRecoveryState == JamRecoveryState::Idle || stepper == nullptr || stepper->isRunning()) {
    return;
  }

  const int32_t direction = reverseRotation ? -1 : 1;
  const int32_t wiggleSteps = computeWiggleSteps();

  switch (jamRecoveryState) {
    case JamRecoveryState::MoveForward:
      stepper->move(direction * wiggleSteps);
      jamRecoveryState = JamRecoveryState::MoveBackward;
      break;
    case JamRecoveryState::MoveBackward:
      stepper->move(-direction * wiggleSteps);
      jamRecoveryCycle++;
      jamRecoveryState = jamRecoveryCycle < JamRecoveryCycles ? JamRecoveryState::MoveForward : JamRecoveryState::Finishing;
      break;
    case JamRecoveryState::Finishing:
      jamRecoveryState = JamRecoveryState::Idle;
      Serial.println("Recuperare finalizata, motor repornit normal.");
      startMotor(false);
      break;
    default:
      break;
  }
}

void configureTmcDriver() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  const uint8_t version = static_cast<uint8_t>(tmcDriver.IOIN() >> 24);
  tmcDriverConnected = version == TmcExpectedVersion;
  if (!tmcDriverConnected) {
    Serial.printf("%s nu raspunde; setarea curentului nu a fost aplicata.\n", TmcDriverName);
    return;
  }

  tmcDriver.begin();
  tmcDriver.I_scale_analog(false);
  tmcDriver.rms_current(tmcRunCurrentMilliamps, TmcHoldCurrentMultiplier);
  tmcDriver.microsteps(motorMicrosteps);
  tmcDriver.intpol(true);
  tmcDriver.en_spreadCycle(false);
  tmcDriver.pwm_autoscale(true);

  Serial.printf("%s configurat: RUN=%u mA RMS HOLD=%u mA RMS R_SENSE=%.2f ohm\n",
                TmcDriverName,
                static_cast<unsigned int>(tmcRunCurrentMilliamps),
                static_cast<unsigned int>(tmcRunCurrentMilliamps * TmcHoldCurrentMultiplier),
                TmcRsenseOhms);
#endif
}

void printTmcSettings() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  const uint32_t ioin = tmcDriver.IOIN();
  const uint8_t version = static_cast<uint8_t>(ioin >> 24);

  Serial.printf("--- %s UART ---\n", TmcDriverName);
  Serial.printf("address=%u IOIN=0x%08lX version=0x%02X\n",
                TmcUartAddress,
                static_cast<unsigned long>(ioin),
                version);
  if (version != TmcExpectedVersion) {
    Serial.printf("%s fara raspuns valid; verifica PDN_UART, GND si adresa MS1/MS2.\n", TmcDriverName);
    Serial.println("--------------------");
    return;
  }

  const uint32_t gconf = tmcDriver.GCONF();
  const uint32_t chopconf = tmcDriver.CHOPCONF();
  const uint32_t iholdIrun = tmcDriver.IHOLD_IRUN();
  const uint32_t pwmconf = tmcDriver.PWMCONF();
  const uint32_t drvStatus = tmcDriver.DRV_STATUS();
  const uint8_t mres = static_cast<uint8_t>((chopconf >> 24) & 0x0F);
  const uint16_t microsteps = mres <= 8 ? static_cast<uint16_t>(256U >> mres) : 0;
  const uint8_t holdCurrentScale = static_cast<uint8_t>(iholdIrun & 0x1F);
  const uint8_t runCurrentScale = static_cast<uint8_t>((iholdIrun >> 8) & 0x1F);
  const uint16_t holdCurrentMilliamps = tmcDriver.cs2rms(holdCurrentScale);
  const uint16_t runCurrentMilliamps = tmcDriver.cs2rms(runCurrentScale);

  Serial.printf("GCONF=0x%08lX spreadCycle=%s pdnDisable=%s mstepRegSelect=%s\n",
                static_cast<unsigned long>(gconf),
                (gconf & (1UL << 2)) ? "ON" : "OFF",
                (gconf & (1UL << 6)) ? "ON" : "OFF",
                (gconf & (1UL << 7)) ? "ON" : "OFF");
  Serial.printf("CHOPCONF=0x%08lX microsteps=%u interpolate=%s\n",
                static_cast<unsigned long>(chopconf),
                microsteps,
                (chopconf & (1UL << 28)) ? "ON" : "OFF");
  Serial.printf("IHOLD_IRUN=0x%08lX IHOLD=%lu IRUN=%lu IHOLDDELAY=%lu\n",
                static_cast<unsigned long>(iholdIrun),
                static_cast<unsigned long>(iholdIrun & 0x1F),
                static_cast<unsigned long>((iholdIrun >> 8) & 0x1F),
                static_cast<unsigned long>((iholdIrun >> 16) & 0x0F));
  Serial.printf("PWMCONF=0x%08lX\n", static_cast<unsigned long>(pwmconf));
  Serial.printf("DRV_STATUS=0x%08lX ot=%s otpw=%s stealthChop=%s standstill=%s\n",
                static_cast<unsigned long>(drvStatus),
                tmcDriver.ot() ? "YES" : "NO",
                tmcDriver.otpw() ? "YES" : "NO",
                tmcDriver.stealth() ? "YES" : "NO",
                tmcDriver.stst() ? "YES" : "NO");
  Serial.printf("Faze: openA=%s openB=%s shortA=%s shortB=%s CS_ACTUAL=%lu\n",
                tmcDriver.ola() ? "YES" : "NO",
                tmcDriver.olb() ? "YES" : "NO",
                tmcDriver.s2ga() ? "YES" : "NO",
                tmcDriver.s2gb() ? "YES" : "NO",
                static_cast<unsigned long>(tmcDriver.cs_actual()));
  Serial.printf("IFCNT=%u\n", tmcDriver.IFCNT());
  Serial.println("Rezumat configuratie:");
  Serial.printf("  RUN: aproximativ %u mA RMS\n", runCurrentMilliamps);
  Serial.printf("  HOLD: aproximativ %u mA RMS\n", holdCurrentMilliamps);
  if (microsteps != 0) {
    Serial.printf("  Microstepping: 1/%u\n", microsteps);
  } else {
    Serial.println("  Microstepping: valoare necunoscuta");
  }
  Serial.printf("  Interpolare: %s\n", (chopconf & (1UL << 28)) ? "activa" : "inactiva");
  Serial.printf("  Mod functionare: %s\n", (gconf & (1UL << 2)) ? "SpreadCycle" : "StealthChop");
  Serial.println("--------------------");
#else
  Serial.println("Driverul TMC UART este configurat doar pentru ESP32-C3.");
#endif
}

void updateTmcDiagnostic() {
  if (tmcDiagnosticPending && static_cast<int32_t>(millis() - tmcDiagnosticAtMillis) >= 0) {
    tmcDiagnosticPending = false;
    printTmcSettings();
  }
}

void toggleMotor() {
  if (stepper == nullptr) {
    Serial.println("Eroare: motorul nu a fost initializat");
    motorRunning = false;
    setMotorEnabled(false);
    return;
  }

  if (jamRecoveryState != JamRecoveryState::Idle) {
    Serial.println("Recuperare in curs, asteapta finalizarea");
    return;
  }

  if (motorRunning) {
    tmcDiagnosticPending = false;
    stopMotor(false);
    motorSessionActive = false;
    Serial.println("Motor oprit");
  } else {
    startMotor();
    tmcDiagnosticAtMillis = millis() + 500;
    tmcDiagnosticPending = true;
    Serial.println("Motor pornit");
  }
}

void updateMotorRunTimer() {
  if (!motorSessionActive) {
    return;
  }

  const uint32_t runDurationMillis = motorRunDurationMinutes * 60UL * 1000UL;
  if (millis() - motorSessionStartMillis < runDurationMillis) {
    return;
  }

  motorSessionActive = false;
  stopMotor(false);
  Serial.printf("Motor oprit automat dupa %lu minute pentru protectie termica.\n",
                static_cast<unsigned long>(motorRunDurationMinutes));
}

void updateRotationCounter() {
  if (!motorRunning) {
    rotationsCounter = 0;
    lastHallTransitionMillis = 0;
    lastRotationTimeMs = 0;
    return;
  }

  // O rotație = o trecere a magnetului peste senzor Hall.
  const bool currentHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  if (currentHallState && !lastHallState) {
    const uint32_t now = millis();
    const uint32_t referenceMillis = lastHallTransitionMillis != 0 ? lastHallTransitionMillis : motorStartMillis;
    const uint32_t elapsedMillis = now - referenceMillis;
    const uint32_t minimumRotationMillis = rotationPreset * 1000UL * MinValidRotationPercent / 100UL;

    if (elapsedMillis >= minimumRotationMillis) {
      if (lastHallTransitionMillis != 0) {
        lastRotationTimeMs = elapsedMillis;
      }
      lastHallTransitionMillis = now;
      rotationsCounter++;
      // Rotatie reala confirmata dupa recuperare => mecanismul functioneaza, resetam contorul de blocari.
      jamAttemptCount = 0;
    } else {
      Serial.printf("Impuls Hall ignorat: %lu ms (minim %lu ms)\n",
                    static_cast<unsigned long>(elapsedMillis),
                    static_cast<unsigned long>(minimumRotationMillis));
    }
  }
  lastHallState = currentHallState;

  // Fara nicio tranzitie Hall in intervalul asteptat => magnetul nu mai trece, motor blocat.
  const uint32_t expectedRotationMs = rotationPreset * 1000UL;
  const uint32_t computedTimeoutMs = expectedRotationMs * StallTimeoutMultiplier;
  const uint32_t stallTimeoutMs = computedTimeoutMs > MinStallTimeoutMs ? computedTimeoutMs : MinStallTimeoutMs;
  const uint32_t referenceMillis = lastHallTransitionMillis != 0 ? lastHallTransitionMillis : motorStartMillis;
  if (millis() - referenceMillis > stallTimeoutMs) {
    Serial.println("Motor blocat! Incep secventa de recuperare.");
    beginJamRecovery();
  }
}

void updateButton() {
  const bool reading = digitalRead(Pins::Button);

  if (reading != lastButtonReading) {
    lastDebounceChangeMillis = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastDebounceChangeMillis) > DebounceMillis &&
      reading != debouncedButtonState) {
    debouncedButtonState = reading;

    if (debouncedButtonState == LOW) {
      toggleMotor();
    }
  }
}

void writeStatusLed(bool on) {
  digitalWrite(Pins::StatusLed, on ? StatusLedActiveLevel : !StatusLedActiveLevel);
}

void updateStatusLed() {
  const bool hallReading = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  statusLedState = !hallReading;
  writeStatusLed(!hallReading);
}

void setup() {
  Serial.begin(115200);
  loadMotorSettings();

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  tmcSerial.begin(TmcUartBaudRate, SERIAL_8N1, Pins::TmcUartRx, Pins::TmcUartTx);
  Serial.printf("%s UART: RX=GPIO%u TX=GPIO%u baud=%lu address=%u\n",
                TmcDriverName,
                Pins::TmcUartRx,
                Pins::TmcUartTx,
                static_cast<unsigned long>(TmcUartBaudRate),
                TmcUartAddress);
#endif

  pinMode(Pins::Step, OUTPUT);
  pinMode(Pins::Dir, OUTPUT);
  pinMode(Pins::Enable, OUTPUT);
  pinMode(Pins::Button, INPUT_PULLUP);
  pinMode(Pins::StatusLed, OUTPUT);
  pinMode(Pins::HallSensor, INPUT_PULLUP);

  digitalWrite(Pins::Step, LOW);
  statusLedState = false;
  writeStatusLed(false);
  setMotorEnabled(false);

  engine.init();
  stepper = engine.stepperConnectToPin(Pins::Step);
  if (stepper != nullptr) {
    stepper->setDirectionPin(Pins::Dir);
    stepper->setEnablePin(Pins::Enable, EnableActiveLevel == LOW);
    stepper->setAutoEnable(false);
    applyMotorSettings();
    setMotorEnabled(false);
  } else {
    Serial.println("Eroare: nu pot conecta pinul STEP la FastAccelStepper");
  }

  rotationsCounter = 0;
  lastHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  lastHallTransitionMillis = 0;
  lastRotationTimeMs = 0;

  webApp.begin();

  Serial.print(BoardName);
  Serial.println(" feeder gata. Apasa butonul pentru start/stop.");
}

void loop() {
  webApp.loop();
  updateButton();
  updateRotationCounter();
  updateJamRecovery();
  updateMotorRunTimer();
  updateStatusLed();
  updateTmcDiagnostic();
}
