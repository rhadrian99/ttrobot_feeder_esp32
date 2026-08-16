# ESP32-WROOM NEMA 17 feeder

Proiect PlatformIO pentru ESP32-WROOM / ESP32 DevKit, TMC2208 in mod STEP/DIR si un singur buton start/stop.

Comportamentul este acelasi ca in proiectul ESP32-C3: apasarea butonului fizic porneste motorul, urmatoarea apasare il opreste. Motorul ruleaza continuu inainte pana la oprire.

ESP32-ul porneste si un Access Point WiFi local. Aplicatia web are un buton START/STOP pentru feeder. Cand feederul este pornit, LED-ul onboard clipeste la fiecare 2 secunde. Cand feederul este oprit, LED-ul onboard sta stins.

## Aplicatie web

Dupa upload, conecteaza telefonul sau laptopul la reteaua WiFi creata de ESP32:

```text
SSID: Feeder_XXXX
Parola: feeder1234
Adresa: http://192.168.4.1
```

Sufixul `XXXX` este generat din identificatorul cipului, ca sa fie mai usor de distins daca ai mai multe placi.

Pagina web include buton START/STOP, afiseaza starea curenta a feederului, versiunea firmware care ruleaza si are o sectiune de setari pentru motor. Butonul fizic si butonul web controleaza aceeasi stare. Cand motorul ruleaza, pagina principala afiseaza timpul ramas in format `MM:SS` si o bara de progres pana la oprirea automata.

Sectiunea de setari este activa doar cand feederul este oprit. Cand motorul se invarte, campurile sunt blocate ca sa nu se schimbe viteza, acceleratia sau directia in timpul miscarii.

Setarile salvate in flash sunt:

| Setare | Valoare implicita | Rol |
| --- | --- | --- |
| Acceleratie | `1000` pasi/s^2 | Acceleratia folosita de `FastAccelStepper` |
| Viteza | `400` pasi/s | Viteza motorului |
| Ratie reductor | `1` | Valoare persistenta pentru un reductor montat ulterior |
| Directie inversata | dezactivat | Schimba sensul de rotatie al motorului |
| Oprire automata | `20` minute | Opreste motorul dupa `10`, `15` sau `20` minute pentru a limita incalzirea |

Aceste valori sunt citite din flash la pornirea ESP32-ului. Daca nu exista inca valori salvate, firmware-ul foloseste valorile implicite.

Sub setarile motorului exista sectiunea **Update firmware**. Alege un fisier `.bin`, apasa `UPDATE FIRMWARE`, confirma dialogul, iar ESP32-ul incarca firmware-ul in slotul OTA liber si reporneste dupa update. Update-ul este blocat cat timp feederul ruleaza. Firmware-ul incarcat trebuie sa contina markerul proiectului `TTROBOT_FEEDER_ESP32_FW:`, altfel este respins inainte de scrierea in flash.

## Conexiuni

| ESP32-WROOM | TMC2208 / buton |
| --- | --- |
| GPIO25 | STEP |
| GPIO26 | DIR |
| GPIO27 | EN / ENABLE |
| GPIO14 | Buton catre GND |
| GND | GND comun cu driverul si sursa motorului |

Butonul foloseste `INPUT_PULLUP`, deci se leaga intre GPIO14 si GND.

Pe ESP32-WROOM nu folosi GPIO6-GPIO11 pentru cablaj extern; aceste pini sunt folositi de memoria flash a modulului. De aceea proiectul portat foloseste GPIO25, GPIO26, GPIO27 si GPIO14 in locul pinilor din varianta ESP32-C3.

## Alimentare

- Alimenteaza motorul din sursa separata potrivita pentru NEMA 17, prin VMOT/GND pe TMC2208.
- Leaga GND-ul sursei motorului cu GND-ul ESP32-WROOM.
- Nu alimenta motorul direct din ESP32-WROOM.
- Regleaza curentul driverului TMC2208 inainte de test, ca sa nu incalzeasca excesiv motorul sau driverul.

## Viteza, acceleratie si directie

Viteza si acceleratia sunt configurabile din aplicatia web. Valorile implicite din firmware sunt:

```cpp
constexpr uint32_t DefaultMotorSpeedStepsPerSecond = 400;
constexpr uint32_t DefaultMotorAccelerationStepsPerSecond2 = 1000;
```

Valorile sunt in pasi pe secunda, respectiv pasi pe secunda la patrat. Porneste conservator, apoi creste treptat in functie de mecanica si de curentul setat pe TMC2208. Directia poate fi inversata din aplicatia web, dar numai cand feederul este oprit.

## Build si upload

Instaleaza extensia VS Code **PlatformIO IDE** sau PlatformIO CLI. Dupa instalare, redeschide terminalul daca `pio` nu este gasit imediat.

Versiunea firmware este definita in [src/main.cpp](src/main.cpp):

```cpp
#define FW_VERSION "1.0.6"
```

La fiecare build, scriptul [copy_firmware.py](copy_firmware.py) copiaza automat binarul compilat in folderul `release`, cu versiunea si placa in nume. Versiunea `1.0.6` genereaza:

```text
release/firmware106_c3-supermini.bin
release/firmware106_wroom.bin
```

Pentru o versiune noua, modifica `FW_VERSION`, ruleaza build-ul si foloseste binarul nou din `release`.

Proiectul foloseste schema de partitii `min_spiffs.csv`, pregatita pentru OTA: doua sloturi de aplicatie (`ota_0` si `ota_1`) si un SPIFFS mic. Aplicatia web poate primi un fisier `.bin` si il poate scrie in slotul liber dupa ce firmware-ul trece validarea markerului de proiect.

Important: dupa schimbarea schemei de partitii, placa trebuie incarcata macar o data prin USB, ca noul partition table sa ajunga pe flash. Dupa aceea se poate folosi upload-ul firmware via web.

```powershell
pio run
pio run --target upload
pio device monitor
```

In acest workspace, PlatformIO Core poate fi rulat si asa:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run --target upload
```
