#include <Arduino.h>
#include <DNSServer.h>
#include <FastAccelStepper.h>
#include <Preferences.h>
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
constexpr uint32_t DefaultMotorSpeedStepsPerSecond = 400;
constexpr uint32_t DefaultMotorAccelerationStepsPerSecond2 = 1000;
constexpr float DefaultGearRatio = 1.0f;
constexpr uint32_t DebounceMillis = 35;
constexpr uint32_t StatusLedBlinkMillis = 2000;
constexpr uint32_t WifiHealthCheckMillis = 5000;
constexpr byte DnsPort = 53;
constexpr uint32_t MinMotorSpeedStepsPerSecond = 1;
constexpr uint32_t MaxMotorSpeedStepsPerSecond = 20000;
constexpr uint32_t MinMotorAccelerationStepsPerSecond2 = 1;
constexpr uint32_t MaxMotorAccelerationStepsPerSecond2 = 50000;
constexpr float MinGearRatio = 0.01f;
constexpr float MaxGearRatio = 100.0f;

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
.settings{margin-top:14px;border:1px solid #314052;background:#111b25;border-radius:8px;padding:14px}
.settings h2{font-size:18px;color:#f9c74f;margin-bottom:10px;letter-spacing:0}
.grid{display:grid;gap:10px}
label{display:grid;gap:5px;color:#b8c5d1;font-size:12px}
input{width:100%;border:1px solid #314052;border-radius:8px;background:#0f1720;color:#f4f0e8;padding:11px;font-size:16px}
.switchRow{display:flex;align-items:center;justify-content:space-between;gap:12px;color:#b8c5d1;font-size:12px;margin-top:10px}
.switch{position:relative;display:inline-block;width:54px;height:30px;flex:0 0 auto}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;inset:0;background:#314052;border-radius:999px;transition:.2s}
.slider:before{content:"";position:absolute;width:24px;height:24px;left:3px;bottom:3px;background:#f4f0e8;border-radius:50%;transition:.2s}
.switch input:checked+.slider{background:#f94144}
.switch input:checked+.slider:before{transform:translateX(24px)}
#saveSettings{margin-top:12px;font-size:16px;padding:13px;background:#f9c74f;color:#101820}
#settingsBox[disabled]{opacity:.48}
#settingsMsg{min-height:18px;margin-top:8px;color:#9fb3c8;font-size:12px;text-align:center}
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
  <section class="settings">
    <h2>Setari motor</h2>
    <fieldset id="settingsBox">
      <div class="grid">
        <label>Acceleratie (pasi/s^2)
          <input id="accel" type="number" min="1" max="50000" step="1" value="1000">
        </label>
        <label>Viteza (pasi/s)
          <input id="speed" type="number" min="1" max="20000" step="1" value="400">
        </label>
        <label>Ratie reductor
          <input id="ratio" type="number" min="0.01" max="100" step="0.01" value="1">
        </label>
      </div>
      <div class="switchRow">
        <span>Schimba directia de rotatie</span>
        <label class="switch">
          <input id="reverse" type="checkbox">
          <span class="slider"></span>
        </label>
      </div>
      <button id="saveSettings" type="button" onclick="saveSettings()">SALVEAZA SETARILE</button>
    </fieldset>
    <div id="settingsMsg"></div>
  </section>
</main>
<script>
function toggleFeeder(){fetch('/toggle',{method:'POST'}).then(poll).catch(()=>{});}
function loadSettings(){
  fetch('/settings').then(r=>r.json()).then(s=>{
    document.getElementById('accel').value=s.acceleration;
    document.getElementById('speed').value=s.speed;
    document.getElementById('ratio').value=s.gearRatio;
    document.getElementById('reverse').checked=s.reverse;
  }).catch(()=>{});
}
function saveSettings(){
  if(!confirm('Salvezi noile setari ale motorului?'))return;
  const msg=document.getElementById('settingsMsg');
  const body=new URLSearchParams({
    acceleration:document.getElementById('accel').value,
    speed:document.getElementById('speed').value,
    gearRatio:document.getElementById('ratio').value,
    reverse:document.getElementById('reverse').checked?'1':'0'
  });
  fetch('/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})
    .then(r=>{if(!r.ok)throw new Error(r.status===409?'Opreste feederul inainte de modificari':'Eroare salvare');return r.json();})
    .then(s=>{msg.textContent='Setari salvate';document.getElementById('accel').value=s.acceleration;document.getElementById('speed').value=s.speed;document.getElementById('ratio').value=s.gearRatio;document.getElementById('reverse').checked=s.reverse;})
    .catch(e=>msg.textContent=e.message);
}
function poll(){
  fetch('/status').then(r=>r.json()).then(d=>{
    const state=document.getElementById('state');
    const btn=document.getElementById('toggleBtn');
    const settings=document.getElementById('settingsBox');
    state.textContent=d.running?'PORNIT':'OPRIT';
    state.className=d.running?'':'off';
    btn.textContent=d.running?'STOP':'START';
    btn.className=d.running?'off':'';
    settings.disabled=d.running;
    document.getElementById('clients').textContent=d.clients;
    document.getElementById('ip').textContent=d.ip;
  }).catch(()=>{});
}
setInterval(poll,1000);loadSettings();poll();
</script>
</body>
</html>
)rawliteral";

FastAccelStepperEngine engine;
FastAccelStepper *stepper = nullptr;
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

String uniqueSsid = "Feeder";

uint32_t motorSpeedStepsPerSecond = DefaultMotorSpeedStepsPerSecond;
uint32_t motorAccelerationStepsPerSecond2 = DefaultMotorAccelerationStepsPerSecond2;
float gearRatio = DefaultGearRatio;
bool reverseRotation = false;

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

void sendSettings() {
  char json[160];
  snprintf(
    json,
    sizeof(json),
    "{\"speed\":%lu,\"acceleration\":%lu,\"gearRatio\":%.2f,\"reverse\":%s}",
    static_cast<unsigned long>(motorSpeedStepsPerSecond),
    static_cast<unsigned long>(motorAccelerationStepsPerSecond2),
    gearRatio,
    reverseRotation ? "true" : "false"
  );
  server.send(200, "application/json", json);
}

void handleGetSettings() {
  sendSettings();
}

void handlePostSettings() {
  if (motorRunning) {
    server.send(409, "text/plain", "Feederul trebuie oprit inainte de modificarea setarilor");
    return;
  }

  if (!server.hasArg("speed") || !server.hasArg("acceleration") || !server.hasArg("gearRatio") || !server.hasArg("reverse")) {
    server.send(400, "text/plain", "Lipsesc setari");
    return;
  }

  motorSpeedStepsPerSecond = constrain(
    static_cast<uint32_t>(server.arg("speed").toInt()),
    MinMotorSpeedStepsPerSecond,
    MaxMotorSpeedStepsPerSecond
  );
  motorAccelerationStepsPerSecond2 = constrain(
    static_cast<uint32_t>(server.arg("acceleration").toInt()),
    MinMotorAccelerationStepsPerSecond2,
    MaxMotorAccelerationStepsPerSecond2
  );
  gearRatio = constrainFloat(server.arg("gearRatio").toFloat(), MinGearRatio, MaxGearRatio);
  reverseRotation = server.arg("reverse") == "1";

  saveMotorSettings();
  applyMotorSettings();
  sendSettings();
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
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/settings", HTTP_POST, handlePostSettings);
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
  loadMotorSettings();
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
    applyMotorSettings();
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
