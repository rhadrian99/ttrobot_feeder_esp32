#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "board_config.h"

class FeederWebApp {
 public:
  struct Dependencies {
    bool *motorRunning = nullptr;
    uint32_t *rotationCounter = nullptr;
    uint32_t *rotationPeriodMs = nullptr;
    uint32_t *motorSpeedStepsPerSecond = nullptr;
    uint32_t *motorAccelerationStepsPerSecond2 = nullptr;
    float *gearRatio = nullptr;
    bool *reverseRotation = nullptr;
    const char *firmwareVersion = nullptr;
    void (*saveMotorSettings)() = nullptr;
    void (*applyMotorSettings)() = nullptr;
    void (*toggleMotor)() = nullptr;
    void (*setMotorEnabled)(bool enabled) = nullptr;
  };

  explicit FeederWebApp(const Dependencies &dependencies);

  void begin();
  void loop();

 private:
  static constexpr uint8_t kDnsPort = 53;
  static constexpr uint32_t kStatusLedBlinkMillis = 2000;
  static constexpr uint32_t kWifiHealthCheckMillis = 5000;
  static constexpr uint32_t kFirmwareValidationMaxBytes = 32768;
  static constexpr uint32_t kMinMotorSpeedStepsPerSecond = 1;
  static constexpr uint32_t kMaxMotorSpeedStepsPerSecond = 20000;
  static constexpr uint32_t kMinMotorAccelerationStepsPerSecond2 = 1;
  static constexpr uint32_t kMaxMotorAccelerationStepsPerSecond2 = 50000;
  static constexpr float kMinGearRatio = 0.01f;
  static constexpr float kMaxGearRatio = 100.0f;
  static constexpr const char *kWifiPassword = "feeder1234";
  static const IPAddress kApIp;
  static const IPAddress kApSubnet;
  static constexpr const char *kFirmwareIdentityPrefix = FirmwareIdentityPrefix;

  Dependencies deps_;
  String uniqueSsid_ = "Feeder";
  WebServer server_;
  DNSServer dnsServer_;
  bool wifiApReady_ = false;
  bool restartPending_ = false;
  bool firmwareUpdateStarted_ = false;
  bool firmwareUpdateFailed_ = false;
  bool firmwareUpdateValidated_ = false;
  bool firmwareUpdateWriteStarted_ = false;
  uint32_t lastStatusLedToggleMillis_ = 0;
  uint32_t lastWifiCheckMillis_ = 0;
  uint32_t restartAtMillis_ = 0;
  uint8_t *firmwareProbeBuffer_ = nullptr;
  size_t firmwareProbeSize_ = 0;
  String firmwareUpdateError_;

  void setupWebServer();
  bool ensureAccessPoint(bool forceRestart = false);
  void generateUniqueSsid();
  void onRoot();
  void onSettingsPage();
  void onCaptivePortal();
  void onStatus();
  void onGetSettings();
  void onPostSettings();
  void onToggle();
  void onStart();
  void onStop();
  void onFirmwareUpdateUpload();
  void onFirmwareUpdateResult();
  void onNotFound();
  void sendSettings();
  void failFirmwareUpdate(const char *message);
  void releaseFirmwareProbeBuffer();
  bool beginValidatedFirmwareUpdate();
  void probeFirmwareChunk(const uint8_t *buffer, size_t size);
  bool bufferContains(const uint8_t *buffer, size_t bufferSize, const char *needle) const;
  void updateRestartState();
};
