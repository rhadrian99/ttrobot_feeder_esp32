#include <Arduino.h>
#include <DNSServer.h>
#include <FastAccelStepper.h>
#include <WebServer.h>
#include <WiFi.h>

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

constexpr bool EnableActiveLevel = LOW;
constexpr uint32_t MotorSpeedStepsPerSecond = 800;
constexpr uint32_t MotorAccelerationStepsPerSecond2 = 400;
constexpr uint32_t DebounceMillis = 35;
constexpr uint32_t StatusLedBlinkMillis = 2000;
constexpr uint32_t WifiHealthCheckMillis = 5000;
constexpr byte DnsPort = 53;

const char WifiPassword[] = "feeder1234";
const IPAddress ApIp(192, 168, 4, 1);
const IPAddress ApSubnet(255, 255, 255, 0);

const char IndexHtml[] = R"rawliteral(
<!DOCTYPE html>
<html lang="ro">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Feeder</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{min-height:100vh;font-family:Verdana,Geneva,sans-serif;background:#101820;color:#f4f0e8;display:grid;place-items:center;padding:18px}
.panel{width:min(430px,100%);border:1px solid #314052;background:#172330;border-radius:8px;padding:20px;box-shadow:0 18px 45px rgba(0,0,0,.35)}
h1{font-size:26px;text-align:center;margin-bottom:8px;color:#f9c74f;letter-spacing:0}
.sub{text-align:center;color:#9fb3c8;font-size:13px;margin-bottom:18px}
.status{border:1px solid #314052;background:#0f1720;border-radius:8px;padding:14px;margin-bottom:14px;text-align:center}
.label{font-size:12px;color:#8ea4ba;margin-bottom:6px;text-transform:uppercase}
#state{font-size:34px;font-weight:700;color:#90be6d}
#state.off{color:#f94144}
button{width:100%;border:0;border-radius:8px;padding:17px;font-size:22px;font-weight:700;color:#101820;background:#90be6d;cursor:pointer;touch-action:manipulation}
button.off{background:#f94144;color:#fff}
.meta{margin-top:14px;display:grid;grid-template-columns:1fr 1fr;gap:10px;color:#b8c5d1;font-size:12px}
.meta div{border:1px solid #314052;border-radius:8px;padding:10px;background:#111b25}
</style>
</head>
<body>
<main class="panel">
  <h1>ESP32 Feeder</h1>
  <div class="sub">Access Point local: http://192.168.4.1</div>
  <section class="status">
    <div class="label">Stare feeder</div>
    <div id="state" class="off">OPRIT</div>
  </section>
  <button id="toggleBtn" onclick="toggleFeeder()">START</button>
  <div class="meta">
    <div>Clienti WiFi<br><strong id="clients">0</strong></div>
    <div>IP<br><strong id="ip">192.168.4.1</strong></div>
  </div>
</main>
<script>
function toggleFeeder(){fetch('/toggle',{method:'POST'}).then(poll).catch(()=>{});}
function poll(){
  fetch('/status').then(r=>r.json()).then(d=>{
    const state=document.getElementById('state');
    const btn=document.getElementById('toggleBtn');
    state.textContent=d.running?'PORNIT':'OPRIT';
    state.className=d.running?'':'off';
    btn.textContent=d.running?'STOP':'START';
    btn.className=d.running?'off':'';
    document.getElementById('clients').textContent=d.clients;
    document.getElementById('ip').textContent=d.ip;
  }).catch(()=>{});
}
setInterval(poll,1000);poll();
</script>
</body>
</html>
)rawliteral";

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
WebServer server(80);
DNSServer dnsServer;

String uniqueSsid = "Feeder";

bool motorRunning = false;
bool disableMotorWhenStopped = false;
bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
bool statusLedState = LOW;
bool wifiApReady = false;
uint32_t lastDebounceChangeMillis = 0;
uint32_t lastStatusLedToggleMillis = 0;
uint32_t lastWifiCheckMillis = 0;

void toggleMotor();

void formatIpAddress(char *buffer, size_t bufferSize, IPAddress ip) {
  snprintf(buffer, bufferSize, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void generateUniqueSsid() {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "Feeder_%04X", static_cast<uint16_t>(chipId >> 32));
  uniqueSsid = String(buffer);
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[WIFI] Client conectat. Total: %d\n", WiFi.softAPgetStationNum());
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("[WIFI] Client deconectat. Total: %d\n", WiFi.softAPgetStationNum());
      break;
    default:
      break;
  }
}

bool ensureAccessPoint(bool forceRestart = false) {
  if (forceRestart) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    delay(250);
  }

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(ApIp, ApIp, ApSubnet);
  delay(100);

  for (int attempt = 1; attempt <= 3; attempt++) {
    if (WiFi.softAP(uniqueSsid.c_str(), WifiPassword, 1, 0, 4)) {
      delay(250);
      IPAddress ip = WiFi.softAPIP();
      if (ip[0] != 0) {
        char ipBuffer[16];
        formatIpAddress(ipBuffer, sizeof(ipBuffer), ip);
        dnsServer.start(DnsPort, "*", ip);
        wifiApReady = true;
        Serial.printf("[WIFI] AP gata: %s  IP: %s  parola: %s\n", uniqueSsid.c_str(), ipBuffer, WifiPassword);
        return true;
      }
    }

    Serial.printf("[WIFI] Reincercare AP %d/3\n", attempt);
    WiFi.softAPdisconnect(true);
    delay(250);
  }

  wifiApReady = false;
  Serial.println("[WIFI] Eroare la pornirea Access Point-ului");
  return false;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html", IndexHtml);
}

void handleCaptivePortal() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "Redirecting to ESP32 Feeder");
}

void handleStatus() {
  char ipBuffer[16];
  formatIpAddress(ipBuffer, sizeof(ipBuffer), WiFi.softAPIP());

  char json[128];
  snprintf(
    json,
    sizeof(json),
    "{\"running\":%s,\"clients\":%d,\"ip\":\"%s\"}",
    motorRunning ? "true" : "false",
    WiFi.softAPgetStationNum(),
    ipBuffer
  );
  server.send(200, "application/json", json);
}

void handleToggle() {
  toggleMotor();
  server.send(200, "text/plain", "OK");
}

void handleStart() {
  if (!motorRunning) {
    toggleMotor();
  }
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  if (motorRunning) {
    toggleMotor();
  }
  server.send(200, "text/plain", "OK");
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  server.on("/generate_204", HTTP_GET, handleCaptivePortal);
  server.on("/gen_204", HTTP_GET, handleCaptivePortal);
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortal);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
  server.on("/fwlink", HTTP_GET, handleCaptivePortal);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/start", HTTP_POST, handleStart);
  server.on("/stop", HTTP_POST, handleStop);
  server.onNotFound(handleCaptivePortal);
  server.begin();
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
    stepper->runForward();
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

void updateStatusLed() {
  if (!motorRunning) {
    if (statusLedState != LOW) {
      statusLedState = LOW;
      digitalWrite(Pins::StatusLed, statusLedState);
    }
    return;
  }

  const uint32_t now = millis();

  if (now - lastStatusLedToggleMillis >= StatusLedBlinkMillis) {
    lastStatusLedToggleMillis = now;
    statusLedState = !statusLedState;
    digitalWrite(Pins::StatusLed, statusLedState);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.onEvent(onWiFiEvent);
  generateUniqueSsid();

  pinMode(Pins::Step, OUTPUT);
  pinMode(Pins::Dir, OUTPUT);
  pinMode(Pins::Enable, OUTPUT);
  pinMode(Pins::Button, INPUT_PULLUP);
  pinMode(Pins::StatusLed, OUTPUT);

  digitalWrite(Pins::Step, LOW);
  digitalWrite(Pins::StatusLed, statusLedState);
  setMotorEnabled(false);

  engine.init();
  stepper = engine.stepperConnectToPin(Pins::Step);
  if (stepper != nullptr) {
    stepper->setDirectionPin(Pins::Dir);
    stepper->setEnablePin(Pins::Enable, EnableActiveLevel == LOW);
    stepper->setAutoEnable(false);
    stepper->setSpeedInHz(MotorSpeedStepsPerSecond);
    stepper->setAcceleration(MotorAccelerationStepsPerSecond2);
    setMotorEnabled(false);
  } else {
    Serial.println("Eroare: nu pot conecta pinul STEP la FastAccelStepper");
  }

  ensureAccessPoint(true);
  setupWebServer();

  Serial.println("ESP32-WROOM feeder gata. Apasa butonul pentru start/stop.");
  Serial.printf("Interfata web: conecteaza-te la WiFi '%s' cu parola '%s', apoi deschide http://192.168.4.1\n", uniqueSsid.c_str(), WifiPassword);
}

void loop() {
  const uint32_t now = millis();

  dnsServer.processNextRequest();
  server.handleClient();

  if (now - lastWifiCheckMillis >= WifiHealthCheckMillis) {
    lastWifiCheckMillis = now;
    if (WiFi.softAPIP()[0] == 0 || !wifiApReady) {
      Serial.println("[WIFI] AP indisponibil, repornesc reteaua");
      ensureAccessPoint(true);
    }
  }

  updateButton();
  updateMotorEnable();
  updateStatusLed();
}
