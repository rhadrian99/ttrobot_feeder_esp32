#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

#include "FeederWebApp.h"
#include "board_config.h"

#define FW_VERSION "1.0.6"

constexpr bool EnableActiveLevel = LOW;
constexpr uint32_t DefaultMotorSpeedStepsPerSecond = 400;
constexpr uint32_t DefaultMotorAccelerationStepsPerSecond2 = 1000;
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
constexpr float JamRecoveryDegrees = 15.0f;
constexpr uint8_t JamRecoveryCycles = 3;
constexpr uint8_t MaxJamAttempts = 2;
constexpr uint32_t DefaultMotorRunDurationMinutes = 20;

enum class JamRecoveryState : uint8_t { Idle, MoveForward, MoveBackward, Finishing };

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
Preferences preferences;

uint32_t motorSpeedStepsPerSecond = DefaultMotorSpeedStepsPerSecond;
uint32_t motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
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

void setMotorEnabled(bool enabled);
void toggleMotor();
void applyMotorSettings();
void loadMotorSettings();
void saveMotorSettings();
void writeStatusLed(bool on);
void updateRotationCounter();
void updateMotorRunTimer();

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
  dependencies.gearRatio = &gearRatio;
  dependencies.reverseRotation = &reverseRotation;
  dependencies.motorRunDurationMinutes = &motorRunDurationMinutes;
  dependencies.motorSessionStartMillis = &motorSessionStartMillis;
  dependencies.motorSessionActive = &motorSessionActive;
  dependencies.firmwareVersion = FW_VERSION;
  dependencies.saveMotorSettings = &saveMotorSettings;
  dependencies.applyMotorSettings = &applyMotorSettings;
  dependencies.toggleMotor = &toggleMotor;
  dependencies.setMotorEnabled = &setMotorEnabled;
  return dependencies;
}

FeederWebApp webApp(buildWebDependencies());

void applyMotorSettings() {
  if (stepper == nullptr) {
    return;
  }

  stepper->setSpeedInHz(motorSpeedStepsPerSecond);
  stepper->setAcceleration(motorAccelerationStepsPerSecond2);
}

void updateMotorSpeedFromPreset() {
  const float stepsPerOutputRotation = static_cast<float>(kMotorStepsPerRevolution * kMicrostepsPerStep) * gearRatio;
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
    preferences.end();

    rotationPreset = constrain(rotationPreset, MinRotationPreset, MaxRotationPreset);
    motorAccelerationStepsPerSecond2 = constrain(motorAccelerationStepsPerSecond2, MinMotorAccelerationStepsPerSecond2, MaxMotorAccelerationStepsPerSecond2);
    gearRatio = constrainFloat(gearRatio, MinGearRatio, MaxGearRatio);
    if (motorRunDurationMinutes != 10 && motorRunDurationMinutes != 15 && motorRunDurationMinutes != 20) {
      motorRunDurationMinutes = DefaultMotorRunDurationMinutes;
    }
  } else {
    Serial.println("NVS: niciun setare salvata, folosesc valorile implicite");
    motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
    gearRatio = DefaultGearRatio;
    reverseRotation = false;
    rotationPreset = DefaultRotationPreset;
    motorRunDurationMinutes = DefaultMotorRunDurationMinutes;
  }

  updateMotorSpeedFromPreset();

  Serial.printf("NVS load: speed=%lu accel=%lu ratio=%.2f preset=%lu reverse=%s\n",
                static_cast<unsigned long>(motorSpeedStepsPerSecond),
                static_cast<unsigned long>(motorAccelerationStepsPerSecond2),
                gearRatio,
                static_cast<unsigned long>(rotationPreset),
                reverseRotation ? "true" : "false");
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
  preferences.end();
  Serial.printf("NVS save: speed=%lu accel=%lu ratio=%.2f preset=%lu reverse=%s\n",
                static_cast<unsigned long>(motorSpeedStepsPerSecond),
                static_cast<unsigned long>(motorAccelerationStepsPerSecond2),
                gearRatio,
                static_cast<unsigned long>(rotationPreset),
                reverseRotation ? "true" : "false");
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
  const float outputStepsPerRotation = static_cast<float>(kMotorStepsPerRevolution * kMicrostepsPerStep) * gearRatio;
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
    stopMotor(false);
    motorSessionActive = false;
    Serial.println("Motor oprit");
  } else {
    startMotor();
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
    if (lastHallTransitionMillis != 0) {
      lastRotationTimeMs = now - lastHallTransitionMillis;
    }
    lastHallTransitionMillis = now;
    rotationsCounter++;
    // Rotatie reala confirmata dupa recuperare => mecanismul functioneaza, resetam contorul de blocari.
    jamAttemptCount = 0;
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
}
