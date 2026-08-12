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
constexpr uint32_t MinMotorAccelerationStepsPerSecond2 = 1;
constexpr uint32_t MaxMotorAccelerationStepsPerSecond2 = 50000;
constexpr float MinGearRatio = 0.01f;
constexpr float MaxGearRatio = 100.0f;

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
Preferences preferences;

uint32_t motorSpeedStepsPerSecond = DefaultMotorSpeedStepsPerSecond;
uint32_t motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
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

void loadMotorSettings() {
  preferences.begin("feeder", true);
  motorSpeedStepsPerSecond = preferences.getUInt("speed", DefaultMotorSpeedStepsPerSecond);
  motorAccelerationStepsPerSecond2 = preferences.getUInt("accel", DefaultMotorAccelerationStepsPerSecond2);
  gearRatio = preferences.getFloat("ratio", DefaultGearRatio);
  reverseRotation = preferences.getBool("reverse", false);
  preferences.end();

  motorSpeedStepsPerSecond = constrain(motorSpeedStepsPerSecond, MinMotorSpeedStepsPerSecond, MaxMotorSpeedStepsPerSecond);
  motorAccelerationStepsPerSecond2 = constrain(motorAccelerationStepsPerSecond2, MinMotorAccelerationStepsPerSecond2, MaxMotorAccelerationStepsPerSecond2);
  gearRatio = constrainFloat(gearRatio, MinGearRatio, MaxGearRatio);
}

void saveMotorSettings() {
  preferences.begin("feeder", false);
  preferences.putUInt("speed", motorSpeedStepsPerSecond);
  preferences.putUInt("accel", motorAccelerationStepsPerSecond2);
  preferences.putFloat("ratio", gearRatio);
  preferences.putBool("reverse", reverseRotation);
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
    setMotorEnabled(false);
    disableMotorWhenStopped = false;
  }

  Serial.println(motorRunning ? "Motor pornit" : "Motor oprit");
}

void updateRotationCounter() {
  if (!motorRunning) {
    rotationsCounter = 0;
    return;
  }

  // O rotație = o trecere a magnetului peste senzor Hall.
  const bool currentHallState = digitalRead(Pins::HallSensor) == HallSensorActiveLevel;
  if (currentHallState && !lastHallState) {
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
