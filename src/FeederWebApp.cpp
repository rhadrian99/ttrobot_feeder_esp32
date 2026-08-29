#include "FeederWebApp.h"

#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

const IPAddress FeederWebApp::kApIp(192, 168, 4, 1);
const IPAddress FeederWebApp::kApSubnet(255, 255, 255, 0);

namespace {

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
.runTimer{border:1px solid #314052;background:#111b25;border-radius:8px;padding:12px;margin-bottom:14px}
.timerRow{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:8px;color:#8ea4ba;font-size:12px;text-transform:uppercase}
#timerText{color:#f4f0e8;font-size:18px;font-weight:700;font-variant-numeric:tabular-nums}
.progressTrack{height:12px;background:#0b1219;border:1px solid #314052;border-radius:6px;overflow:hidden}
#timerProgress{width:0;height:100%;background:#90be6d;transition:width .4s linear,background-color .2s}
#timerProgress.warning{background:#f9c74f}
#timerProgress.critical{background:#f94144}
button{width:100%;border:0;border-radius:8px;padding:17px;font-size:22px;font-weight:700;color:#101820;background:#90be6d;cursor:pointer;touch-action:manipulation;margin-top:10px}
button.off{background:#f94144;color:#fff}
button.secondary{background:#5a7c99;color:#f4f0e8;font-size:16px}
button:disabled{opacity:.48;cursor:not-allowed}
.meta{margin-top:14px;display:grid;grid-template-columns:1fr 1fr;gap:10px;color:#b8c5d1;font-size:12px}
.meta div{border:1px solid #314052;border-radius:8px;padding:10px;background:#111b25}
.meta .wide{grid-column:1/-1}
.actions{margin-top:20px;border:1px solid #314052;background:#111b25;border-radius:8px;padding:14px}
.actions button{margin-top:0;background:#5a7c99;color:#f4f0e8;font-size:16px}
.alert{margin-top:14px;border:2px solid #f94144;background:#3d1616;border-radius:8px;padding:14px;color:#ff6b6b;font-weight:700;text-align:center;font-size:14px;display:none}
.alert.show{display:block}
.alert-icon{font-size:24px;margin-bottom:8px}
</style>
</head>
<body>
<main class="panel">
  <h1 id="appTitle">ESP32 Feeder</h1>
  <div class="sub">Access Point local: <span style="color:white;">192.168.4.1</span></div>
  <section class="status">
    <div class="label">Stare feeder</div>
    <div id="state" class="off">OPRIT</div>
  </section>
  <section class="runTimer">
    <div class="timerRow">
      <span>Timp pana la oprire</span>
      <strong id="timerText">--:--</strong>
    </div>
    <div id="timerTrack" class="progressTrack" role="progressbar" aria-label="Timp ramas" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0">
      <div id="timerProgress"></div>
    </div>
  </section>
  <button id="toggleBtn" onclick="toggleFeeder()">START</button>
  <div id="jamAlert" class="alert">
    <div class="alert-icon">⚠️</div>
    <span id="jamAlertText">Motor blocat! Verifica mecanismul si porneste din nou.</span>
  </div>
  <div class="meta">
    <div>Clienti WiFi<br><strong id="clients">0</strong></div>
    <div>IP<br><strong id="ip">192.168.4.1</strong></div>
    <div>Rotatii<br><strong id="rotations">0</strong></div>
    <div>Perioada<br><strong id="rotPeriod">0 ms</strong></div>
  </div>
  <section class="actions">
    <button class="secondary" onclick="window.location.href='/settings-page'">SETARI</button>
  </section>
</main>
<script>
const AudioContext=window.AudioContext||window.webkitAudioContext;
let audioCtx=null;
function initAudio(){if(!audioCtx){audioCtx=new AudioContext();}}
function playClick(){
  initAudio();
  if(!audioCtx)return;
  const t=audioCtx.currentTime;
  const osc=audioCtx.createOscillator();
  const gain=audioCtx.createGain();
  osc.connect(gain);
  gain.connect(audioCtx.destination);
  osc.frequency.value=800;
  osc.type='sine';
  gain.gain.setValueAtTime(0.1,t);
  gain.gain.exponentialRampToValueAtTime(0.01,t+0.1);
  osc.start(t);
  osc.stop(t+0.1);
}
function playSuccess(){
  initAudio();
  if(!audioCtx)return;
  const t=audioCtx.currentTime;
  for(let i=0;i<2;i++){
    const osc=audioCtx.createOscillator();
    const gain=audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.frequency.value=900+i*200;
    osc.type='sine';
    const start=t+i*0.15;
    gain.gain.setValueAtTime(0.1,start);
    gain.gain.exponentialRampToValueAtTime(0.01,start+0.1);
    osc.start(start);
    osc.stop(start+0.1);
  }
}
function playError(){
  initAudio();
  if(!audioCtx)return;
  const t=audioCtx.currentTime;
  const osc=audioCtx.createOscillator();
  const gain=audioCtx.createGain();
  osc.connect(gain);
  gain.connect(audioCtx.destination);
  osc.frequency.value=300;
  osc.type='sine';
  gain.gain.setValueAtTime(0.2,t);
  gain.gain.exponentialRampToValueAtTime(0.01,t+0.3);
  osc.start(t);
  osc.stop(t+0.3);
}
function toggleFeeder(){playClick();fetch('/toggle',{method:'POST'}).then(poll).catch(()=>{});}
function formatRemainingTime(totalSeconds){
  const seconds=Math.max(0,Number(totalSeconds)||0);
  const minutes=Math.floor(seconds/60);
  return String(minutes).padStart(2,'0')+':'+String(seconds%60).padStart(2,'0');
}
function poll(){
  fetch('/status').then(r=>r.json()).then(d=>{
    const state=document.getElementById('state');
    const btn=document.getElementById('toggleBtn');
    const settingsBtn=document.querySelector('.actions button');
    state.textContent=d.running?'PORNIT':'OPRIT';
    state.className=d.running?'':'off';
    btn.textContent=d.running?'STOP':'START';
    btn.className=d.running?'off':'';
    settingsBtn.disabled=d.running;
    document.getElementById('clients').textContent=d.clients;
    document.getElementById('ip').textContent=d.ip;
    document.getElementById('rotations').textContent=Number(d.rotations||0);
    document.getElementById('rotPeriod').textContent=d.rotationPeriodMs!=null?String(d.rotationPeriodMs)+' ms':'0 ms';
    const timerActive=Boolean(d.timerActive);
    const totalSeconds=Number(d.runDurationSeconds||0);
    const remainingSeconds=Number(d.runRemainingSeconds||0);
    const progressPercent=timerActive&&totalSeconds>0?Math.max(0,Math.min(100,remainingSeconds/totalSeconds*100)):0;
    const timerProgress=document.getElementById('timerProgress');
    document.getElementById('timerText').textContent=timerActive?formatRemainingTime(remainingSeconds):'--:--';
    timerProgress.style.width=progressPercent+'%';
    timerProgress.className=progressPercent<=20?'critical':progressPercent<=50?'warning':'';
    document.getElementById('timerTrack').setAttribute('aria-valuenow',String(Math.round(progressPercent)));
    document.getElementById('appTitle').textContent='ESP32 Feeder v.'+d.version;
    document.getElementById('jamAlert').className=d.jammed?'alert show':'alert';
    document.getElementById('jamAlertText').textContent=d.jammedPermanent
      ? 'Motor blocat definitiv dupa 2 incercari! Verifica mecanismul si porneste manual.'
      : 'Motor blocat, se incearca deblocarea automata...';
  }).catch(()=>{});
}
document.addEventListener('DOMContentLoaded',()=>{
  const settingsBtn=document.querySelector('.actions button');
  if(settingsBtn){
    settingsBtn.addEventListener('click',playClick);
  }
});
setInterval(poll,1000);poll();
</script>
</body>
</html>
)rawliteral";

const char SettingsHtml[] = R"rawliteral(
<!DOCTYPE html>
<html lang="ro">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Feeder - Setari</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{min-height:100vh;font-family:Verdana,Geneva,sans-serif;background:#101820;color:#f4f0e8;display:grid;place-items:center;padding:18px}
.panel{width:min(430px,100%);border:1px solid #314052;background:#172330;border-radius:8px;padding:20px;box-shadow:0 18px 45px rgba(0,0,0,.35)}
h1{font-size:26px;text-align:center;margin-bottom:8px;color:#f9c74f;letter-spacing:0}
.sub{text-align:center;color:#ffffff;font-size:15px;margin-bottom:18px}
button{width:100%;border:0;border-radius:8px;padding:17px;font-size:22px;font-weight:700;color:#101820;background:#90be6d;cursor:pointer;touch-action:manipulation}
button.back{background:#5a7c99;color:#f4f0e8;font-size:16px;margin-bottom:14px}
.settings{background:transparent;border:0;border-radius:0;padding:0;margin-bottom:14px}
.settings h2{font-size:18px;color:#f9c74f;margin-bottom:10px;letter-spacing:0}
.firmwareSettings{border-top:1px solid #314052;margin-top:24px;padding-top:22px}
.firmwareSettings h2{color:#f3722c}
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
#saveSettings{margin-top:32px;font-size:16px;padding:13px;background:#f9c74f;color:#101820}
#updateFirmware{margin-top:12px;font-size:16px;padding:13px;background:#f3722c;color:#101820}
#settingsBox[disabled]{opacity:.48}
#firmwareBox[disabled]{opacity:.48}
fieldset{border:0;padding:0;margin:0;min-inline-size:0}
.presetBlock{margin-top:14px;border:0;border-radius:8px;background:transparent;padding:0}
.presetLabel{font-size:12px;color:#b8c5d1;margin-bottom:10px;letter-spacing:.04em;text-transform:uppercase}
.presetGrid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.microstepGrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.presetButton,.accelButton,.microstepButton,.currentButton{width:100%;padding:12px 8px;border:1px solid #314052;border-radius:8px;background:#111b25;color:#f4f0e8;font-size:18px;font-weight:700;cursor:pointer}
.presetButton.selected,.accelButton.selected,.microstepButton.selected,.currentButton.selected{background:#f9c74f;color:#101820;border-color:#f9c74f}
.timerGrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.timerButton{width:100%;padding:12px 8px;border:1px solid #314052;border-radius:8px;background:#111b25;color:#f4f0e8;font-size:16px;font-weight:700;cursor:pointer}
.timerButton.selected{background:#f9c74f;color:#101820;border-color:#f9c74f}
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
  <h1>ESP32 Feeder - Setari</h1>
  <button class="back" onclick="window.location.href='/'">← INAPOI</button>
  <section class="settings">
    <h2>Setari motor</h2>
    <fieldset id="settingsBox">
      <div class="grid">
        <label>Ratie reductor
          <input id="ratio" type="number" min="1" max="5" step="0.05" value="1">
        </label>
      </div>
      <div id="reverseControl" class="switchRow" style="display:none">
        <span>Schimba directia de rotatie</span>
        <label class="switch">
          <input id="reverse" type="checkbox">
          <span class="slider"></span>
        </label>
      </div>
      <div class="presetBlock">
        <div class="presetLabel">Viteza rotație (sec/rotație)</div>
        <div class="presetGrid">
          <button type="button" class="presetButton" data-preset="4">4s</button>
          <button type="button" class="presetButton selected" data-preset="5">5s</button>
          <button type="button" class="presetButton" data-preset="6">6s</button>
          <button type="button" class="presetButton" data-preset="7">7s</button>
        </div>
      </div>
      <div class="presetBlock">
        <div class="presetLabel">Oprire automata</div>
        <div class="timerGrid">
          <button type="button" class="timerButton" data-minutes="10">10 min</button>
          <button type="button" class="timerButton" data-minutes="15">15 min</button>
          <button type="button" class="timerButton selected" data-minutes="20">20 min</button>
        </div>
      </div>
      <div class="presetBlock">
        <div class="presetLabel">Acceleratie (pasi/s^2)</div>
        <div class="presetGrid">
          <button type="button" class="accelButton" data-acceleration="200">200</button>
          <button type="button" class="accelButton selected" data-acceleration="400">400</button>
          <button type="button" class="accelButton" data-acceleration="600">600</button>
          <button type="button" class="accelButton" data-acceleration="800">800</button>
        </div>
      </div>
      <div id="tmcSettings">
        <div class="presetBlock">
          <div class="presetLabel">Driver UART configurat</div>
          <strong id="tmcDriverName">TMC</strong>
          <div id="tmcDriverStatus">Verificare UART...</div>
        </div>
        <div class="presetBlock">
          <div class="presetLabel">Microstepping</div>
          <div class="microstepGrid">
            <button type="button" class="microstepButton selected" data-microsteps="4">1/4</button>
            <button type="button" class="microstepButton" data-microsteps="8">1/8</button>
            <button type="button" class="microstepButton" data-microsteps="16">1/16</button>
          </div>
        </div>
        <div class="presetBlock">
          <div class="presetLabel">Curent RUN (mA RMS)</div>
          <div class="presetGrid">
            <button type="button" class="currentButton" data-current="600">600</button>
            <button type="button" class="currentButton selected" data-current="800">800</button>
            <button type="button" class="currentButton" data-current="900">900</button>
            <button type="button" class="currentButton" data-current="1000">1000</button>
          </div>
        </div>
      </div>
      <button id="saveSettings" type="button" onclick="askSaveSettings()">SALVEAZA SETARILE</button>
    </fieldset>
    <div id="settingsMsg"></div>
  </section>
  <section class="settings firmwareSettings">
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
    <h2 id="saveModalTitle">Confirmare salvare</h2>
    <p id="saveModalText">Salvezi noile setari ale motorului in memoria flash?</p>
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
const AudioContext=window.AudioContext||window.webkitAudioContext;
let audioCtx=null;
function initAudio(){if(!audioCtx){audioCtx=new AudioContext();}}
function playClick(){
  initAudio();
  if(!audioCtx)return;
  const t=audioCtx.currentTime;
  const osc=audioCtx.createOscillator();
  const gain=audioCtx.createGain();
  osc.connect(gain);
  gain.connect(audioCtx.destination);
  osc.frequency.value=800;
  osc.type='sine';
  gain.gain.setValueAtTime(0.1,t);
  gain.gain.exponentialRampToValueAtTime(0.01,t+0.1);
  osc.start(t);
  osc.stop(t+0.1);
}
function playSuccess(){
  initAudio();
  if(!audioCtx)return;
  const t=audioCtx.currentTime;
  for(let i=0;i<2;i++){
    const osc=audioCtx.createOscillator();
    const gain=audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.frequency.value=900+i*200;
    osc.type='sine';
    const start=t+i*0.15;
    gain.gain.setValueAtTime(0.1,start);
    gain.gain.exponentialRampToValueAtTime(0.01,start+0.1);
    osc.start(start);
    osc.stop(start+0.1);
  }
}
function getSettingsValidationIssues(){
  const issues=[];
  const ratioValue = Number(document.getElementById('ratio').value);
  if(Number.isNaN(ratioValue) || ratioValue < 1 || ratioValue > 5){
    issues.push({field:'ratio', label:'Ratie reductor', min:1, max:5});
  }
  return issues;
}
function focusFirstInvalidField(){
  const issues = getSettingsValidationIssues();
  if(issues.length === 0){return;}
  const field = document.getElementById(issues[0].field);
  if(field){field.focus();field.select();}
}
function showSettingsValidationModal(issues){
  const modal=document.getElementById('saveModal');
  const title=document.getElementById('saveModalTitle');
  const text=document.getElementById('saveModalText');
  const cancel=document.getElementById('cancelSave');
  const confirm=document.getElementById('confirmSave');
  title.textContent='Validare setari';
  text.innerHTML = issues.map(issue =>
    '<strong>' + issue.label + '</strong>: intre ' + issue.min + ' si ' + issue.max + '<br>'
  ).join('');
  cancel.style.display='none';
  confirm.textContent='OK';
  confirm.onclick = () => {
    closeSaveModal();
    focusFirstInvalidField();
  };
  modal.className='modal open';
}
function clampValue(value, min, max){
  const numeric = Number(value);
  if(!Number.isFinite(numeric)) return min;
  return Math.min(Math.max(numeric, min), max);
}
function clampRatioValue(value){
  const clamped = clampValue(value, 1, 5);
  return Number(clamped.toFixed(2));
}
function askSaveSettings(){
  const issues = getSettingsValidationIssues();
  if(issues.length > 0){
    playClick();
    showSettingsValidationModal(issues);
    return;
  }
  playClick();
  const modal=document.getElementById('saveModal');
  const title=document.getElementById('saveModalTitle');
  const text=document.getElementById('saveModalText');
  const cancel=document.getElementById('cancelSave');
  const confirm=document.getElementById('confirmSave');
  title.textContent='Confirmare salvare';
  text.textContent='Salvezi noile setari ale motorului in memoria flash?';
  cancel.style.display='block';
  confirm.textContent='SALVEAZA';
  confirm.onclick = confirmSaveSettings;
  modal.className='modal open';
}
function closeSaveModal(){
  const modal=document.getElementById('saveModal');
  const cancel=document.getElementById('cancelSave');
  const confirm=document.getElementById('confirmSave');
  modal.className='modal';
  cancel.style.display='block';
  confirm.textContent='SALVEAZA';
  confirm.onclick = confirmSaveSettings;
}
function confirmSaveSettings(){
  const issues = getSettingsValidationIssues();
  if(issues.length > 0){
    showSettingsValidationModal(issues);
    return;
  }
  closeSaveModal();
  saveSettings();
}
function setPresetButtonSelection(value){
  const preset = Number(value || 5);
  document.querySelectorAll('.presetButton').forEach(btn => {
    btn.classList.toggle('selected', Number(btn.dataset.preset) === preset);
  });
}
function setAccelerationButtonSelection(value){
  const options = [200, 400, 600, 800];
  const requested = Number(value || 400);
  const acceleration = options.reduce((closest, option) =>
    Math.abs(option - requested) < Math.abs(closest - requested) ? option : closest
  );
  document.querySelectorAll('.accelButton').forEach(btn => {
    btn.classList.toggle('selected', Number(btn.dataset.acceleration) === acceleration);
  });
}
function setMicrostepButtonSelection(value){
  const microsteps = [4, 8, 16].includes(Number(value)) ? Number(value) : 4;
  document.querySelectorAll('.microstepButton').forEach(btn => {
    btn.classList.toggle('selected', Number(btn.dataset.microsteps) === microsteps);
  });
}
function setCurrentButtonSelection(value){
  const current = [600, 800, 900, 1000].includes(Number(value)) ? Number(value) : 800;
  document.querySelectorAll('.currentButton').forEach(btn => {
    btn.classList.toggle('selected', Number(btn.dataset.current) === current);
  });
}
function setTimerButtonSelection(value){
  const minutes = Number(value || 20);
  document.querySelectorAll('.timerButton').forEach(btn => {
    btn.classList.toggle('selected', Number(btn.dataset.minutes) === minutes);
  });
}
function loadSettings(){
  fetch('/settings').then(r=>r.json()).then(s=>{
    const preset = Number(s.rotationPreset ?? 5);
    const clampedPreset = Math.max(4, Math.min(7, preset));

    document.getElementById('ratio').value=s.gearRatio;
    const reverseElement = document.getElementById('reverse');
    if(reverseElement) reverseElement.checked=s.reverse;

    // Control visibility of direction control based on server flag
    const reverseControl = document.getElementById('reverseControl');
    if(reverseControl) {
      reverseControl.style.display = s.showDirectionControl !== false ? 'flex' : 'none';
    }

    setAccelerationButtonSelection(s.acceleration);
    setMicrostepButtonSelection(s.microsteps);
    setCurrentButtonSelection(s.runCurrent);
    document.getElementById('tmcDriverName').textContent=s.tmcDriverName||'TMC';
    document.getElementById('tmcDriverStatus').textContent=s.tmcDriverConnected?'Comunicare UART: OK':'Comunicare UART: FARA RASPUNS';
    document.getElementById('tmcSettings').style.display=s.tmcSettingsAvailable?'block':'none';
    setPresetButtonSelection(clampedPreset);
    setTimerButtonSelection(s.runDurationMinutes ?? 20);
    updateFeederStatus();
  }).catch(()=>{});
}
function askFirmwareUpdate(){
  const file=document.getElementById('firmwareFile').files[0];
  const msg=document.getElementById('firmwareMsg');
  if(!file){msg.textContent='Alege un fisier .bin';playClick();return;}
  playClick();
  document.getElementById('updateModal').className='modal open';
}
function closeUpdateModal(){document.getElementById('updateModal').className='modal';}
function confirmFirmwareUpdate(){closeUpdateModal();uploadFirmware();}
function uploadFirmware(){
  const file=document.getElementById('firmwareFile').files[0];
  const msg=document.getElementById('firmwareMsg');
  if(!file){msg.textContent='Alege un fisier .bin';return;}
  playClick();
  const body=new FormData();
  body.append('firmware',file,file.name);
  msg.textContent='Se incarca firmware-ul...';
  fetch('/update',{method:'POST',body})
    .then(r=>{if(!r.ok)return r.text().then(t=>{throw new Error(t||'Update esuat');});return r.text();})
    .then(t=>{msg.textContent=t;playSuccess();})
    .catch(e=>{msg.textContent=e.message;});
}
function saveSettings(){
  const msg=document.getElementById('settingsMsg');
  const ratioField = document.getElementById('ratio');
  const selectedAcceleration = document.querySelector('.accelButton.selected')?.dataset.acceleration || 400;
  const selectedMicrosteps = document.querySelector('.microstepButton.selected')?.dataset.microsteps || 4;
  const selectedRunCurrent = document.querySelector('.currentButton.selected')?.dataset.current || 800;
  const selectedPreset = document.querySelector('.presetButton.selected')?.dataset.preset || 5;
  const selectedRunDuration = document.querySelector('.timerButton.selected')?.dataset.minutes || 20;
  const validatedRatio = clampRatioValue(ratioField.value);
  ratioField.value = validatedRatio;
  const reverseElement = document.getElementById('reverse');
  const reverseValue = reverseElement ? (reverseElement.checked ? '1' : '0') : '0';
  const body=new URLSearchParams({
    acceleration:selectedAcceleration,
    microsteps:selectedMicrosteps,
    runCurrent:selectedRunCurrent,
    rotationPreset:selectedPreset,
    runDurationMinutes:selectedRunDuration,
    gearRatio:validatedRatio,
    reverse:reverseValue
  });
  fetch('/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})
    .then(r=>{if(!r.ok)throw new Error(r.status===409?'Opreste feederul inainte de modificari':'Eroare salvare');return r.json();})
    .then(s=>{msg.textContent='Setari salvate';playSuccess();
      const preset = Number(s.rotationPreset ?? 5);
      const clampedPreset = Math.max(4, Math.min(7, preset));
      document.getElementById('ratio').value=clampRatioValue(s.gearRatio);
      const reverseElem = document.getElementById('reverse');
      if(reverseElem) reverseElem.checked=s.reverse;
      setAccelerationButtonSelection(s.acceleration);
      setMicrostepButtonSelection(s.microsteps);
      setCurrentButtonSelection(s.runCurrent);
      setPresetButtonSelection(clampedPreset);
      setTimerButtonSelection(s.runDurationMinutes ?? 20);})
    .catch(e=>{msg.textContent=e.message;});
}
function updateFeederStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    const settings=document.getElementById('settingsBox');
    settings.disabled=d.running;
    document.getElementById('firmwareBox').disabled=d.running;
  }).catch(()=>{});
}
document.addEventListener('DOMContentLoaded',()=>{
  const backBtn=document.querySelector('.back');
  if(backBtn){
    backBtn.addEventListener('click',playClick);
  }
  document.querySelectorAll('.presetButton').forEach(btn => {
    btn.addEventListener('click', () => {
      const preset = Number(btn.dataset.preset || 5);
      setPresetButtonSelection(preset);
    });
  });
  document.querySelectorAll('.accelButton').forEach(btn => {
    btn.addEventListener('click', () => {
      setAccelerationButtonSelection(btn.dataset.acceleration);
    });
  });
  document.querySelectorAll('.microstepButton').forEach(btn => {
    btn.addEventListener('click', () => {
      setMicrostepButtonSelection(btn.dataset.microsteps);
    });
  });
  document.querySelectorAll('.currentButton').forEach(btn => {
    btn.addEventListener('click', () => {
      setCurrentButtonSelection(btn.dataset.current);
    });
  });
  document.querySelectorAll('.timerButton').forEach(btn => {
    btn.addEventListener('click', () => {
      setTimerButtonSelection(btn.dataset.minutes);
    });
  });
});
loadSettings();
setInterval(updateFeederStatus,1000);
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
  server_.on("/settings-page", HTTP_GET, [this]() { onSettingsPage(); });
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

void FeederWebApp::onSettingsPage() {
  server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server_.send(200, "text/html", SettingsHtml);
}

void FeederWebApp::onCaptivePortal() {
  server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server_.sendHeader("Location", "http://192.168.4.1/", true);
  server_.send(302, "text/plain", "Redirecting to ESP32 Feeder");
}

void FeederWebApp::onStatus() {
  char ipBuffer[16];
  formatIp(ipBuffer, sizeof(ipBuffer), WiFi.softAPIP());

  const bool timerActive = deps_.motorSessionActive != nullptr && *deps_.motorSessionActive;
  const uint32_t runDurationSeconds = deps_.motorRunDurationMinutes != nullptr
    ? *deps_.motorRunDurationMinutes * 60UL
    : 20UL * 60UL;
  uint32_t runRemainingSeconds = 0;
  if (timerActive && deps_.motorSessionStartMillis != nullptr) {
    const uint32_t elapsedMillis = millis() - *deps_.motorSessionStartMillis;
    const uint32_t durationMillis = runDurationSeconds * 1000UL;
    runRemainingSeconds = elapsedMillis < durationMillis
      ? (durationMillis - elapsedMillis + 999UL) / 1000UL
      : 0;
  }

  char json[470];
  snprintf(
    json,
    sizeof(json),
    "{\"running\":%s,\"jammed\":%s,\"jammedPermanent\":%s,\"clients\":%d,\"ip\":\"%s\",\"version\":\"%s\",\"rotations\":%lu,\"rotationPeriodMs\":%lu,\"timerActive\":%s,\"runDurationSeconds\":%lu,\"runRemainingSeconds\":%lu}",
    *deps_.motorRunning ? "true" : "false",
    (deps_.motorJammed != nullptr && *deps_.motorJammed) ? "true" : "false",
    (deps_.motorJammedPermanent != nullptr && *deps_.motorJammedPermanent) ? "true" : "false",
    WiFi.softAPgetStationNum(),
    ipBuffer,
    deps_.firmwareVersion,
    deps_.rotationCounter != nullptr ? static_cast<unsigned long>(*deps_.rotationCounter) : 0UL,
    deps_.rotationPeriodMs != nullptr ? static_cast<unsigned long>(*deps_.rotationPeriodMs) : 0UL,
    timerActive ? "true" : "false",
    static_cast<unsigned long>(runDurationSeconds),
    static_cast<unsigned long>(runRemainingSeconds)
  );
  server_.send(200, "application/json", json);
}

void FeederWebApp::sendSettings() {
  const uint32_t microsteps = deps_.motorMicrosteps != nullptr ? *deps_.motorMicrosteps : kMicrostepsPerStep;
  const uint32_t runCurrent = deps_.tmcRunCurrentMilliamps != nullptr ? *deps_.tmcRunCurrentMilliamps : 800u;
  const float outputStepsPerRotation = static_cast<float>(kMotorStepsPerRevolution * microsteps) * (*deps_.gearRatio);
  const float secondsPerRotation = *deps_.motorSpeedStepsPerSecond > 0 ? (outputStepsPerRotation / static_cast<float>(*deps_.motorSpeedStepsPerSecond)) : 0.0f;
  const uint32_t presetValue = deps_.rotationPreset != nullptr
    ? *deps_.rotationPreset
    : (secondsPerRotation > 0.0f ? static_cast<uint32_t>(lroundf(outputStepsPerRotation / secondsPerRotation)) : 5u);
  char json[430];
  snprintf(
    json,
    sizeof(json),
    "{\"speed\":%lu,\"acceleration\":%lu,\"gearRatio\":%.2f,\"reverse\":%s,\"rotationPreset\":%lu,\"secondsPerRotation\":%.2f,\"runDurationMinutes\":%lu,\"microsteps\":%lu,\"runCurrent\":%lu,\"tmcSettingsAvailable\":%s,\"tmcDriverName\":\"%s\",\"tmcDriverConnected\":%s,\"showDirectionControl\":%s}",
    static_cast<unsigned long>(*deps_.motorSpeedStepsPerSecond),
    static_cast<unsigned long>(*deps_.motorAccelerationStepsPerSecond2),
    *deps_.gearRatio,
    *deps_.reverseRotation ? "true" : "false",
    static_cast<unsigned long>(presetValue),
    secondsPerRotation,
    deps_.motorRunDurationMinutes != nullptr ? static_cast<unsigned long>(*deps_.motorRunDurationMinutes) : 20UL,
    static_cast<unsigned long>(microsteps),
    static_cast<unsigned long>(runCurrent),
    deps_.tmcSettingsAvailable ? "true" : "false",
    deps_.tmcDriverName != nullptr ? deps_.tmcDriverName : "",
    (deps_.tmcDriverConnected != nullptr && *deps_.tmcDriverConnected) ? "true" : "false",
    ENABLE_DIRECTION_CONTROL ? "true" : "false"
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

  if (!server_.hasArg("acceleration") || !server_.hasArg("gearRatio") || !server_.hasArg("reverse") ||
      (deps_.tmcSettingsAvailable && (!server_.hasArg("microsteps") || !server_.hasArg("runCurrent")))) {
    server_.send(400, "text/plain", "Lipsesc setari");
    return;
  }

  const uint32_t requestedPreset = server_.hasArg("rotationPreset")
    ? constrain(static_cast<uint32_t>(server_.arg("rotationPreset").toInt()), 4u, 7u)
    : 5u;

  const uint32_t requestedAcceleration = static_cast<uint32_t>(server_.arg("acceleration").toInt());
  const uint32_t acceleration = requestedAcceleration == 200 || requestedAcceleration == 600 || requestedAcceleration == 800
    ? requestedAcceleration
    : 400u;
  const float requestedGearRatio = server_.arg("gearRatio").toFloat();
  const uint32_t requestedRunDuration = server_.hasArg("runDurationMinutes")
    ? static_cast<uint32_t>(server_.arg("runDurationMinutes").toInt())
    : 20u;
  const uint32_t requestedMicrosteps = static_cast<uint32_t>(server_.arg("microsteps").toInt());
  const uint32_t requestedRunCurrent = static_cast<uint32_t>(server_.arg("runCurrent").toInt());
  const uint32_t microsteps = requestedMicrosteps == 8 || requestedMicrosteps == 16 ? requestedMicrosteps : 4u;
  const uint32_t runCurrent = requestedRunCurrent == 600 || requestedRunCurrent == 900 || requestedRunCurrent == 1000
    ? requestedRunCurrent
    : 800u;
  Serial.printf("POST settings: accel=%lu gear=%.2f reverse=%s preset=%lu microsteps=%lu runCurrent=%lu\n",
                static_cast<unsigned long>(requestedAcceleration),
                requestedGearRatio,
                server_.arg("reverse") == "1" ? "true" : "false",
                static_cast<unsigned long>(requestedPreset),
                static_cast<unsigned long>(microsteps),
                static_cast<unsigned long>(runCurrent));

  *deps_.motorAccelerationStepsPerSecond2 = acceleration;

  *deps_.gearRatio = constrainFloat(requestedGearRatio, kMinGearRatio, kMaxGearRatio);
#if ENABLE_DIRECTION_CONTROL
  *deps_.reverseRotation = server_.arg("reverse") == "1";
#else
  *deps_.reverseRotation = false;
#endif
  if (deps_.tmcSettingsAvailable) {
    *deps_.motorMicrosteps = microsteps;
    *deps_.tmcRunCurrentMilliamps = runCurrent;
  }
  if (deps_.motorRunDurationMinutes != nullptr) {
    *deps_.motorRunDurationMinutes = requestedRunDuration == 10 || requestedRunDuration == 15
      ? requestedRunDuration
      : 20u;
  }

  const uint32_t activeMicrosteps = deps_.motorMicrosteps != nullptr ? *deps_.motorMicrosteps : kMicrostepsPerStep;
  const float outputStepsPerRotation = static_cast<float>(kMotorStepsPerRevolution * activeMicrosteps) * (*deps_.gearRatio);
  *deps_.motorSpeedStepsPerSecond = constrain(
    static_cast<uint32_t>(lroundf(outputStepsPerRotation / static_cast<float>(requestedPreset))),
    kMinMotorSpeedStepsPerSecond,
    kMaxMotorSpeedStepsPerSecond
  );

  if (deps_.rotationPreset != nullptr) {
    *deps_.rotationPreset = requestedPreset;
  }

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
