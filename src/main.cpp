#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

#include "FeederWebApp.h"
#include "board_config.h"

#define FW_VERSION "1.0.4"

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

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
Preferences preferences;

uint32_t motorSpeedStepsPerSecond = DefaultMotorSpeedStepsPerSecond;
uint32_t motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
uint32_t rotationPreset = DefaultRotationPreset;
float gearRatio = DefaultGearRatio;
bool reverseRotation = false;

bool motorRunning = false;
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

FeederWebApp::Dependencies buildWebDependencies() {
  FeederWebApp::Dependencies dependencies;
  dependencies.motorRunning = &motorRunning;
  dependencies.rotationCounter = &rotationsCounter;
  dependencies.rotationPeriodMs = &lastRotationTimeMs;
  dependencies.rotationPreset = &rotationPreset;
  dependencies.motorSpeedStepsPerSecond = &motorSpeedStepsPerSecond;
  dependencies.motorAccelerationStepsPerSecond2 = &motorAccelerationStepsPerSecond2;
  dependencies.gearRatio = &gearRatio;
  dependencies.reverseRotation = &reverseRotation;
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
    preferences.end();

    rotationPreset = constrain(rotationPreset, MinRotationPreset, MaxRotationPreset);
    motorAccelerationStepsPerSecond2 = constrain(motorAccelerationStepsPerSecond2, MinMotorAccelerationStepsPerSecond2, MaxMotorAccelerationStepsPerSecond2);
    gearRatio = constrainFloat(gearRatio, MinGearRatio, MaxGearRatio);
  } else {
    Serial.println("NVS: niciun setare salvata, folosesc valorile implicite");
    motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
    gearRatio = DefaultGearRatio;
    reverseRotation = false;
    rotationPreset = DefaultRotationPreset;
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

void toggleMotor() {
  motorRunning = !motorRunning;

  if (stepper == nullptr) {
    Serial.println("Eroare: motorul nu a fost initializat");
    motorRunning = false;
    setMotorEnabled(false);
    return;
  }

  if (motorRunning) {
    rotationsCounter = 0;
    lastHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
    lastHallTransitionMillis = 0;
    lastRotationTimeMs = 0;
    setMotorEnabled(true);
    if (reverseRotation) {
      stepper->runBackward();
    } else {
      stepper->runForward();
    }
  } else {
    stepper->stopMove();
    rotationsCounter = 0;
    lastHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
    lastHallTransitionMillis = 0;
    lastRotationTimeMs = 0;
    setMotorEnabled(false);
  }

  Serial.println(motorRunning ? "Motor pornit" : "Motor oprit");
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
  }
  lastHallState = currentHallState;
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
  updateStatusLed();
}
