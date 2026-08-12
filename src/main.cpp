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
constexpr uint32_t StatusLedBlinkMillis = 250;
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
bool disableMotorWhenStopped = false;
bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
bool statusLedState = LOW;
uint32_t lastDebounceChangeMillis = 0;
uint32_t lastStatusLedToggleMillis = 0;
bool hallActive = false;
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
float constrainFloat(float value, float minimum, float maximum);

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

float constrainFloat(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

void applyMotorSettings() {
  if (stepper == nullptr) {
    return;
  }

  stepper->setSpeedInHz(motorSpeedStepsPerSecond);
  stepper->setAcceleration(motorAccelerationStepsPerSecond2);
}

void updateMotorSpeedFromPreset() {
  const float stepsPerOutputRotation = 200.0f * 8.0f * gearRatio;
  const float computedSpeed = stepsPerOutputRotation / static_cast<float>(rotationPreset);
  motorSpeedStepsPerSecond = static_cast<uint32_t>(lroundf(computedSpeed));
  motorSpeedStepsPerSecond = constrain(motorSpeedStepsPerSecond, MinMotorSpeedStepsPerSecond, MaxMotorSpeedStepsPerSecond);
}

void loadMotorSettings() {
  preferences.begin("feeder", true);
  motorSpeedStepsPerSecond = preferences.getUInt("speed", DefaultMotorSpeedStepsPerSecond);
  motorAccelerationStepsPerSecond2 = preferences.getUInt("accel", DefaultMotorAccelerationStepsPerSecond2);
  gearRatio = preferences.getFloat("ratio", DefaultGearRatio);
  reverseRotation = preferences.getBool("reverse", false);
  rotationPreset = preferences.getUInt("rotationPreset", DefaultRotationPreset);
  preferences.end();

  rotationPreset = constrain(rotationPreset, MinRotationPreset, MaxRotationPreset);
  motorSpeedStepsPerSecond = constrain(motorSpeedStepsPerSecond, MinMotorSpeedStepsPerSecond, MaxMotorSpeedStepsPerSecond);
  motorAccelerationStepsPerSecond2 = constrain(motorAccelerationStepsPerSecond2, MinMotorAccelerationStepsPerSecond2, MaxMotorAccelerationStepsPerSecond2);
  gearRatio = constrainFloat(gearRatio, MinGearRatio, MaxGearRatio);

  if (rotationPreset >= MinRotationPreset && rotationPreset <= MaxRotationPreset) {
    updateMotorSpeedFromPreset();
  }
}

void saveMotorSettings() {
  preferences.begin("feeder", false);
  preferences.putUInt("speed", motorSpeedStepsPerSecond);
  preferences.putUInt("accel", motorAccelerationStepsPerSecond2);
  preferences.putFloat("ratio", gearRatio);
  preferences.putBool("reverse", reverseRotation);
  preferences.putUInt("rotationPreset", rotationPreset);
  preferences.end();
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
    disableMotorWhenStopped = false;
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
    disableMotorWhenStopped = false;
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

void updateMotorEnable() {
  if (stepper != nullptr && disableMotorWhenStopped && !stepper->isRunning()) {
    setMotorEnabled(false);
    disableMotorWhenStopped = false;
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
  hallActive = hallReading;

  if (!hallReading) {
    statusLedState = true;
    writeStatusLed(true);
    lastStatusLedToggleMillis = millis();
    return;
  }

  statusLedState = false;
  writeStatusLed(false);
  lastStatusLedToggleMillis = millis();
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
  updateMotorEnable();
  updateRotationCounter();
  updateStatusLed();
}
