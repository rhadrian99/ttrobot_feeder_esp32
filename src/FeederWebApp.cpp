#include "FeederWebApp.h"

#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

const IPAddress FeederWebApp::kApIp(192, 168, 4, 1);
const IPAddress FeederWebApp::kApSubnet(255, 255, 255, 0);

namespace {
float constrainFloat(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

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
.meta .wide{grid-column:1/-1}
.settings{margin-top:14px;border:1px solid #314052;background:#111b25;border-radius:8px;padding:14px}
.settings h2{font-size:18px;color:#f9c74f;margin-bottom:10px;letter-spacing:0}
.grid{display:grid;gap:10px}
label{display:grid;gap:5px;color:#b8c5d1;font-size:12px}
input{width:100%;border:1px solid #314052;border-radius:8px;background:#0f1720;color:#f4f0e8;padding:11px;font-size:16px}
input[type=file]{font-size:13px;color:#b8c5d1}
.switchRow{display:flex;align-items:center;justify-content:space-between;gap:12px;color:#b8c5d1;font-size:12px;margin-top:10px}
.switch{position:relative;display:inline-block;width:54px;height:30px;flex:0 0 auto}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;inset:0;background:#314052;border-radius:999px;transition:.2s}
.slider:before{content:"";position:absolute;width:24px;height:24px;left:3px;bottom:3px;background:#f4f0e8;border-radius:50%;transition:.2s}
.switch input:checked+.slider{background:#f94144}
.switch input:checked+.slider:before{transform:translateX(24px)}
#saveSettings{margin-top:12px;font-size:16px;padding:13px;background:#f9c74f;color:#101820}
#updateFirmware{margin-top:12px;font-size:16px;padding:13px;background:#f3722c;color:#101820}
#settingsBox[disabled]{opacity:.48}
#firmwareBox[disabled]{opacity:.48}
#settingsMsg,#firmwareMsg{min-height:18px;margin-top:8px;color:#9fb3c8;font-size:12px;text-align:center}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.62);display:none;place-items:center;padding:18px;z-index:5}
.modal.open{display:grid}
.modalBox{width:min(360px,100%);border:1px solid #46576a;background:#172330;border-radius:8px;padding:18px;box-shadow:0 18px 45px rgba(0,0,0,.45)}
.modalBox h2{font-size:19px;color:#f9c74f;margin-bottom:8px;letter-spacing:0}
.modalBox p{color:#b8c5d1;font-size:14px;line-height:1.35;margin-bottom:14px}
.modalActions{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.modalActions button{font-size:15px;padding:12px}
#cancelSave{background:#314052;color:#f4f0e8}
#cancelUpdate{background:#314052;color:#f4f0e8}
#confirmSave{background:#f9c74f;color:#101820}
#confirmUpdate{background:#f3722c;color:#101820}
</style>
</head>
<body>
<main class="panel">
  <h1 id="appTitle">ESP32 Feeder</h1>
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
      <button id="saveSettings" type="button" onclick="askSaveSettings()">SALVEAZA SETARILE</button>
    </fieldset>
    <div id="settingsMsg"></div>
  </section>
  <section class="settings">
    <h2>Update firmware</h2>
    <fieldset id="firmwareBox">
      <label>Fisier firmware (.bin)
        <input id="firmwareFile" type="file" accept=".bin,application/octet-stream">
      </label>
      <button id="updateFirmware" type="button" onclick="askFirmwareUpdate()">UPDATE FIRMWARE</button>
    </fieldset>
    <div id="firmwareMsg"></div>
  </section>
</main>
<div id="saveModal" class="modal">
  <div class="modalBox">
    <h2>Confirmare salvare</h2>
    <p>Salvezi noile setari ale motorului in memoria flash?</p>
    <div class="modalActions">
      <button id="cancelSave" type="button" onclick="closeSaveModal()">ANULEAZA</button>
      <button id="confirmSave" type="button" onclick="confirmSaveSettings()">SALVEAZA</button>
    </div>
  </div>
</div>
<div id="updateModal" class="modal">
  <div class="modalBox">
    <h2>Confirmare update</h2>
    <p>Vrei sa incarci si sa flash-uiesti firmware-ul selectat? ESP32-ul va reporni dupa update.</p>
    <div class="modalActions">
      <button id="cancelUpdate" type="button" onclick="closeUpdateModal()">ANULEAZA</button>
      <button id="confirmUpdate" type="button" onclick="confirmFirmwareUpdate()">UPDATE</button>
    </div>
  </div>
</div>
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
function askSaveSettings(){document.getElementById('saveModal').className='modal open';}
function closeSaveModal(){document.getElementById('saveModal').className='modal';}
function confirmSaveSettings(){closeSaveModal();saveSettings();}
function askFirmwareUpdate(){
  const file=document.getElementById('firmwareFile').files[0];
  const msg=document.getElementById('firmwareMsg');
  if(!file){msg.textContent='Alege un fisier .bin';return;}
  document.getElementById('updateModal').className='modal open';
}
function closeUpdateModal(){document.getElementById('updateModal').className='modal';}
function confirmFirmwareUpdate(){closeUpdateModal();uploadFirmware();}
function uploadFirmware(){
  const file=document.getElementById('firmwareFile').files[0];
  const msg=document.getElementById('firmwareMsg');
  if(!file){msg.textContent='Alege un fisier .bin';return;}
  const body=new FormData();
  body.append('firmware',file,file.name);
  msg.textContent='Se incarca firmware-ul...';
  fetch('/update',{method:'POST',body})
    .then(r=>{if(!r.ok)return r.text().then(t=>{throw new Error(t||'Update esuat');});return r.text();})
    .then(t=>{msg.textContent=t;})
    .catch(e=>{msg.textContent=e.message;});
}
function saveSettings(){
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
    document.getElementById('firmwareBox').disabled=d.running;
    document.getElementById('clients').textContent=d.clients;
    document.getElementById('ip').textContent=d.ip;
    document.getElementById('appTitle').textContent='ESP32 Feeder v.'+d.version;
  }).catch(()=>{});
}
setInterval(poll,1000);loadSettings();poll();
</script>
</body>
</html>
)rawliteral";

const char *formatIp(char *buffer, size_t bufferSize, IPAddress ip) {
  snprintf(buffer, bufferSize, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return buffer;
}
}  // namespace

FeederWebApp::FeederWebApp(const Dependencies &dependencies) : deps_(dependencies) {}

void FeederWebApp::begin() {
  generateUniqueSsid();
  ensureAccessPoint(true);
  setupWebServer();
}

void FeederWebApp::loop() {
  dnsServer_.processNextRequest();
  server_.handleClient();
  updateRestartState();

  const uint32_t now = millis();
  if (now - lastWifiCheckMillis_ >= kWifiHealthCheckMillis) {
    lastWifiCheckMillis_ = now;
    if (WiFi.softAPIP()[0] == 0 || !wifiApReady_) {
      Serial.println("[WIFI] AP indisponibil, repornesc reteaua");
      ensureAccessPoint(true);
    }
  }
}

void FeederWebApp::generateUniqueSsid() {
  const uint64_t chipId = ESP.getEfuseMac();
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "Feeder_%04X", static_cast<uint16_t>(chipId >> 32));
  uniqueSsid_ = String(buffer);
}

bool FeederWebApp::ensureAccessPoint(bool forceRestart) {
  if (forceRestart) {
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    delay(250);
  }

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(kApIp, kApIp, kApSubnet);
  delay(100);

  for (int attempt = 1; attempt <= 3; attempt++) {
    if (WiFi.softAP(uniqueSsid_.c_str(), kWifiPassword, 1, 0, 4)) {
      delay(250);
      IPAddress ip = WiFi.softAPIP();
      if (ip[0] != 0) {
        char ipBuffer[16];
        formatIp(ipBuffer, sizeof(ipBuffer), ip);
        dnsServer_.start(kDnsPort, "*", ip);
        wifiApReady_ = true;
        Serial.printf("[WIFI] AP gata: %s  IP: %s  parola: %s\n", uniqueSsid_.c_str(), ipBuffer, kWifiPassword);
        return true;
      }
    }

    Serial.printf("[WIFI] Reincercare AP %d/3\n", attempt);
    WiFi.softAPdisconnect(true);
    delay(250);
  }

  wifiApReady_ = false;
  Serial.println("[WIFI] Eroare la pornirea Access Point-ului");
  return false;
}

void FeederWebApp::setupWebServer() {
  server_.on("/", HTTP_GET, [this]() { onRoot(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/generate_204", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/gen_204", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/fwlink", HTTP_GET, [this]() { onCaptivePortal(); });
  server_.on("/status", HTTP_GET, [this]() { onStatus(); });
  server_.on("/settings", HTTP_GET, [this]() { onGetSettings(); });
  server_.on("/settings", HTTP_POST, [this]() { onPostSettings(); });
  server_.on("/toggle", HTTP_POST, [this]() { onToggle(); });
  server_.on("/start", HTTP_POST, [this]() { onStart(); });
  server_.on("/stop", HTTP_POST, [this]() { onStop(); });
  server_.on("/update", HTTP_POST, [this]() { onFirmwareUpdateResult(); }, [this]() { onFirmwareUpdateUpload(); });
  server_.onNotFound([this]() { onNotFound(); });
  server_.begin();
}

void FeederWebApp::onRoot() {
  server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server_.send(200, "text/html", IndexHtml);
}

void FeederWebApp::onCaptivePortal() {
  server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server_.sendHeader("Location", "http://192.168.4.1/", true);
  server_.send(302, "text/plain", "Redirecting to ESP32 Feeder");
}

void FeederWebApp::onStatus() {
  char ipBuffer[16];
  formatIp(ipBuffer, sizeof(ipBuffer), WiFi.softAPIP());

  char json[220];
  snprintf(
    json,
    sizeof(json),
    "{\"running\":%s,\"clients\":%d,\"ip\":\"%s\",\"version\":\"%s\"}",
    *deps_.motorRunning ? "true" : "false",
    WiFi.softAPgetStationNum(),
    ipBuffer,
    deps_.firmwareVersion
  );
  server_.send(200, "application/json", json);
}

void FeederWebApp::sendSettings() {
  char json[160];
  snprintf(
    json,
    sizeof(json),
    "{\"speed\":%lu,\"acceleration\":%lu,\"gearRatio\":%.2f,\"reverse\":%s}",
    static_cast<unsigned long>(*deps_.motorSpeedStepsPerSecond),
    static_cast<unsigned long>(*deps_.motorAccelerationStepsPerSecond2),
    *deps_.gearRatio,
    *deps_.reverseRotation ? "true" : "false"
  );
  server_.send(200, "application/json", json);
}

void FeederWebApp::onGetSettings() {
  sendSettings();
}

void FeederWebApp::onPostSettings() {
  if (*deps_.motorRunning) {
    server_.send(409, "text/plain", "Feederul trebuie oprit inainte de modificarea setarilor");
    return;
  }

  if (!server_.hasArg("speed") || !server_.hasArg("acceleration") || !server_.hasArg("gearRatio") || !server_.hasArg("reverse")) {
    server_.send(400, "text/plain", "Lipsesc setari");
    return;
  }

  *deps_.motorSpeedStepsPerSecond = constrain(
    static_cast<uint32_t>(server_.arg("speed").toInt()),
    kMinMotorSpeedStepsPerSecond,
    kMaxMotorSpeedStepsPerSecond
  );
  *deps_.motorAccelerationStepsPerSecond2 = constrain(
    static_cast<uint32_t>(server_.arg("acceleration").toInt()),
    kMinMotorAccelerationStepsPerSecond2,
    kMaxMotorAccelerationStepsPerSecond2
  );
  *deps_.gearRatio = constrainFloat(server_.arg("gearRatio").toFloat(), kMinGearRatio, kMaxGearRatio);
  *deps_.reverseRotation = server_.arg("reverse") == "1";

  if (deps_.saveMotorSettings != nullptr) {
    deps_.saveMotorSettings();
  }
  if (deps_.applyMotorSettings != nullptr) {
    deps_.applyMotorSettings();
  }
  sendSettings();
}

void FeederWebApp::onToggle() {
  if (deps_.toggleMotor != nullptr) {
    deps_.toggleMotor();
  }
  server_.send(200, "text/plain", "OK");
}

void FeederWebApp::onStart() {
  if (!*deps_.motorRunning && deps_.toggleMotor != nullptr) {
    deps_.toggleMotor();
  }
  server_.send(200, "text/plain", "OK");
}

void FeederWebApp::onStop() {
  if (*deps_.motorRunning && deps_.toggleMotor != nullptr) {
    deps_.toggleMotor();
  }
  server_.send(200, "text/plain", "OK");
}

void FeederWebApp::failFirmwareUpdate(const char *message) {
  firmwareUpdateFailed_ = true;
  firmwareUpdateError_ = message;
  if (firmwareUpdateWriteStarted_) {
    Update.abort();
  }
}

void FeederWebApp::releaseFirmwareProbeBuffer() {
  if (firmwareProbeBuffer_ != nullptr) {
    free(firmwareProbeBuffer_);
    firmwareProbeBuffer_ = nullptr;
  }
  firmwareProbeSize_ = 0;
}

bool FeederWebApp::bufferContains(const uint8_t *buffer, size_t bufferSize, const char *needle) const {
  const size_t needleSize = strlen(needle);
  if (needleSize == 0 || bufferSize < needleSize) {
    return false;
  }

  for (size_t offset = 0; offset <= bufferSize - needleSize; offset++) {
    if (memcmp(buffer + offset, needle, needleSize) == 0) {
      return true;
    }
  }

  return false;
}

bool FeederWebApp::beginValidatedFirmwareUpdate() {
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    failFirmwareUpdate("Nu pot porni scrierea OTA");
    Update.printError(Serial);
    return false;
  }

  firmwareUpdateWriteStarted_ = true;
  if (Update.write(firmwareProbeBuffer_, firmwareProbeSize_) != firmwareProbeSize_) {
    failFirmwareUpdate("Eroare la scrierea firmware-ului");
    Update.printError(Serial);
    return false;
  }

  firmwareUpdateValidated_ = true;
  releaseFirmwareProbeBuffer();
  return true;
}

void FeederWebApp::probeFirmwareChunk(const uint8_t *buffer, size_t size) {
  if (firmwareUpdateFailed_ || firmwareUpdateValidated_) {
    return;
  }

  if (firmwareProbeBuffer_ == nullptr || firmwareProbeSize_ + size > kFirmwareValidationMaxBytes) {
    failFirmwareUpdate("Firmware incompatibil: marker proiect lipsa");
    return;
  }

  memcpy(firmwareProbeBuffer_ + firmwareProbeSize_, buffer, size);
  firmwareProbeSize_ += size;

  if (bufferContains(firmwareProbeBuffer_, firmwareProbeSize_, kFirmwareIdentityPrefix)) {
    beginValidatedFirmwareUpdate();
  }
}

void FeederWebApp::onFirmwareUpdateUpload() {
  HTTPUpload &upload = server_.upload();

  if (*deps_.motorRunning) {
    failFirmwareUpdate("Opreste feederul inainte de update firmware");
    return;
  }

  if (upload.status == UPLOAD_FILE_START) {
    firmwareUpdateStarted_ = true;
    firmwareUpdateFailed_ = false;
    firmwareUpdateValidated_ = false;
    firmwareUpdateWriteStarted_ = false;
    firmwareUpdateError_ = "";
    releaseFirmwareProbeBuffer();
    firmwareProbeBuffer_ = static_cast<uint8_t *>(malloc(kFirmwareValidationMaxBytes));
    if (deps_.setMotorEnabled != nullptr) {
      deps_.setMotorEnabled(false);
    }
    Serial.printf("[OTA] Start update: %s\n", upload.filename.c_str());

    if (firmwareProbeBuffer_ == nullptr) {
      failFirmwareUpdate("Memorie insuficienta pentru validarea firmware-ului");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!firmwareUpdateFailed_ && !firmwareUpdateValidated_) {
      probeFirmwareChunk(upload.buf, upload.currentSize);
    } else if (!firmwareUpdateFailed_ && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      failFirmwareUpdate("Eroare la scrierea firmware-ului");
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!firmwareUpdateFailed_ && !firmwareUpdateValidated_) {
      failFirmwareUpdate("Firmware incompatibil: marker proiect lipsa");
    }

    if (!firmwareUpdateFailed_ && Update.end(true)) {
      Serial.printf("[OTA] Update complet: %u bytes\n", upload.totalSize);
    } else if (!firmwareUpdateFailed_) {
      failFirmwareUpdate("Update firmware esuat");
      Update.printError(Serial);
    }
    releaseFirmwareProbeBuffer();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    failFirmwareUpdate("Update firmware anulat");
    releaseFirmwareProbeBuffer();
    Serial.println("[OTA] Update anulat");
  }
}

void FeederWebApp::onFirmwareUpdateResult() {
  if (*deps_.motorRunning) {
    server_.send(409, "text/plain", "Opreste feederul inainte de update firmware");
    return;
  }

  if (firmwareUpdateFailed_ || Update.hasError()) {
    server_.send(500, "text/plain", firmwareUpdateError_.length() ? firmwareUpdateError_.c_str() : "Update firmware esuat");
    return;
  }

  if (!firmwareUpdateStarted_) {
    server_.send(400, "text/plain", "Lipseste fisierul firmware");
    return;
  }

  restartPending_ = true;
  restartAtMillis_ = millis() + 800;
  server_.send(200, "text/plain", "Firmware incarcat. ESP32 reporneste...");
}

void FeederWebApp::onNotFound() {
  onCaptivePortal();
}

void FeederWebApp::updateRestartState() {
  if (restartPending_ && millis() >= restartAtMillis_) {
    Serial.println("[OTA] Restart dupa update firmware");
    ESP.restart();
  }
}
