#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

#include "FeederWebApp.h"
#include "board_config.h"

#define FW_VERSION "1.0.3"

constexpr bool EnableActiveLevel = LOW;
constexpr uint32_t DefaultMotorSpeedStepsPerSecond = 400;
constexpr uint32_t DefaultMotorAccelerationStepsPerSecond2 = 1000;
constexpr float DefaultGearRatio = 1.0f;
constexpr uint32_t DebounceMillis = 35;
constexpr uint32_t StatusLedBlinkMillis = 2000;
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

void setMotorEnabled(bool enabled);
void toggleMotor();
void applyMotorSettings();
void loadMotorSettings();
void saveMotorSettings();
void writeStatusLed(bool on);
float constrainFloat(float value, float minimum, float maximum);

FeederWebApp::Dependencies buildWebDependencies() {
  FeederWebApp::Dependencies dependencies;
  dependencies.motorRunning = &motorRunning;
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
  if (stepper == nullptr) {
    digitalWrite(Pins::Enable, enabled ? EnableActiveLevel : !EnableActiveLevel);
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
    setMotorEnabled(true);
    if (reverseRotation) {
      stepper->runBackward();
    } else {
      stepper->runForward();
    }
  } else {
    stepper->stopMove();
    disableMotorWhenStopped = true;
  }

  Serial.println(motorRunning ? "Motor pornit" : "Motor oprit");
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
  if (!motorRunning) {
    if (statusLedState) {
      statusLedState = false;
      writeStatusLed(false);
    }
    return;
  }

  const uint32_t now = millis();

  if (now - lastStatusLedToggleMillis >= StatusLedBlinkMillis) {
    lastStatusLedToggleMillis = now;
    statusLedState = !statusLedState;
    writeStatusLed(statusLedState);
  }
}

void setup() {
  Serial.begin(115200);
  loadMotorSettings();

  pinMode(Pins::Step, OUTPUT);
  pinMode(Pins::Dir, OUTPUT);
  pinMode(Pins::Enable, OUTPUT);
  pinMode(Pins::Button, INPUT_PULLUP);
  pinMode(Pins::StatusLed, OUTPUT);

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

  webApp.begin();

  Serial.print(BoardName);
  Serial.println(" feeder gata. Apasa butonul pentru start/stop.");
}

void loop() {
  webApp.loop();
  updateButton();
  updateMotorEnable();
  updateStatusLed();
}
