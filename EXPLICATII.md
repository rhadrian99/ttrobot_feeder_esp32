# Explicatii proiect ESP32 feeder

Acest proiect controleaza un motor pas cu pas NEMA 17 in modul STEP/DIR, folosind un ESP32-WROOM / ESP32 DevKit sau un ESP32-C3 SuperMini. Varianta C3 poate folosi un TMC2208 sau TMC2209 configurat si diagnosticat prin UART, iar varianta WROOM pastreaza compatibilitatea STEP/DIR existenta.

## Pe scurt

- ESP32-ul porneste si configureaza pinii pentru motor, buton si LED.
- Driverul de motor este tinut dezactivat la pornire.
- La apasarea butonului, motorul porneste si ruleaza continuu inainte.
- La urmatoarea apasare, motorul se opreste controlat, apoi driverul este dezactivat.
- ESP32-ul creeaza un Access Point WiFi si serveste o aplicatie web la `http://192.168.4.1`.
- Din aplicatia web se poate porni/opri feederul cu un buton START/STOP.
- Aplicatia web afiseaza versiunea firmware care ruleaza.
- Pagina principala afiseaza timpul ramas pana la oprirea automata si o bara de progres.
- Din aplicatia web se pot modifica ratia reductorului, optional directia, timpul unei rotatii, acceleratia si, pe C3, microstepping-ul si curentul driverului TMC.
- Pagina de setari afiseaza modelul TMC selectat la compilare si rezultatul ultimei verificari UART.
- Durata maxima de functionare poate fi setata la `10`, `15` sau `20` minute; valoarea implicita este `20` minute.
- Din aplicatia web se poate incarca un firmware `.bin` nou si flash-ui in slotul OTA liber, cu confirmare inainte de update.
- Setarile motorului sunt salvate in flash si sunt reincarcate la pornire.
- Senzorul Hall numara rotatiile reale si permite detectarea unui mecanism blocat.
- La primul blocaj, firmware-ul incearca automat trei miscari scurte inainte/inapoi, apoi reporneste motorul.
- La al doilea blocaj consecutiv, motorul ramane oprit pana la interventia utilizatorului.
- LED-ul onboard indica starea senzorului Hall: se stinge cand magnetul este detectat si se aprinde in rest.

## Fisiere importante

- `platformio.ini` defineste tintele pentru ESP32-WROOM si ESP32-C3 SuperMini, framework-ul Arduino, viteza seriala si bibliotecile `FastAccelStepper` si `TMCStepper`.
- `copy_firmware.py` copiaza firmware-ul compilat in folderul `release`, cu versiunea in nume.
- `src/main.cpp` contine controlul motorului, citirea butonului si senzorului Hall, precum si recuperarea la blocaj.
- `src/board_config.h` contine pinii, polaritatile si identitatea OTA specifice fiecarei placi.
- `src/FeederWebApp.cpp` si `src/FeederWebApp.h` contin Access Point-ul, serverul HTTP, interfata web si update-ul OTA.
- `README.md` contine schema de conectare si comenzile de build/upload.

## Configuratia PlatformIO

Proiectul are doua medii PlatformIO:

| Mediu | Placa PlatformIO | Hardware |
| --- | --- | --- |
| `esp32-wroom` | `esp32dev` | ESP32-WROOM / ESP32 DevKit clasic |
| `esp32-c3-supermini` | `lolin_c3_mini` | ESP32-C3 SuperMini |

Mediul implicit este `esp32-c3-supermini`. Pentru WROOM trebuie selectat explicit mediul `esp32-wroom` la build sau upload.

Schema de partitii este `min_spiffs.csv`. Ea imparte flash-ul de 4 MB in doua sloturi OTA mari si o zona SPIFFS mica:

| Partitie | Rol | Dimensiune |
| --- | --- | --- |
| `nvs` | setari persistente ESP32/WiFi | `0x5000` |
| `otadata` | metadata pentru alegerea slotului OTA activ | `0x2000` |
| `app0` / `ota_0` | primul slot firmware | `0x1E0000` |
| `app1` / `ota_1` | al doilea slot firmware | `0x1E0000` |
| `spiffs` | fisiere mici, daca vor fi necesare | `0x20000` |
| `coredump` | diagnostic crash | `0x10000` |

Aceasta partitionare pregateste proiectul pentru update firmware via web: firmware-ul curent ruleaza dintr-un slot, iar noul `.bin` poate fi scris in celalalt slot. Dupa restart, bootloader-ul poate porni noua versiune.

Schimbarea schemei de partitii trebuie incarcata o data prin USB, deoarece partition table-ul este scris separat de aplicatie. Dupa aceea, endpoint-ul OTA existent poate actualiza firmware-ul din aplicatia web.

Biblioteca principala este:

```ini
lib_deps =
  gin66/FastAccelStepper@^0.30.0
  teemuatlut/TMCStepper@^0.7.3
```

`FastAccelStepper` genereaza impulsurile STEP mai precis decat o bucla manuala cu `delayMicroseconds`, ceea ce ajuta la miscarea mai stabila a motorului. `TMCStepper` configureaza si citeste registrele TMC2208 sau TMC2209 prin UART pe ESP32-C3.

Bibliotecile `WiFi`, `DNSServer` si `WebServer` vin din framework-ul Arduino pentru ESP32 si sunt folosite pentru Access Point, captive portal si pagina web de control. Biblioteca `Preferences` este folosita pentru salvarea setarilor motorului in flash.

## Versionarea firmware-ului

Versiunea firmware este definita in `src/main.cpp` prin `FW_VERSION`. Compatibilitatea OTA este verificata separat prin markerul de placa definit in `src/board_config.h`.

Scriptul `copy_firmware.py` ruleaza automat dupa build prin `extra_scripts = post:copy_firmware.py` din `platformio.ini`. Scriptul citeste `FW_VERSION`, elimina punctele din versiune si copiaza firmware-ul in folderul `release`.

Exemplu: `FW_VERSION "1.0.7"` produce `release/firmware107_c3-supermini.bin` sau `release/firmware107_wroom.bin`, in functie de mediul compilat.

## WiFi si aplicatia web

La pornire, firmware-ul genereaza un SSID unic de forma:

```text
Feeder_XXXX
```

Parola este:

```text
feeder1234
```

Dupa conectare la reteaua ESP32-ului, pagina se deschide la:

```text
http://192.168.4.1
```

Functionalitatea WiFi este preluata ca idee din proiectul C3 Super Mini: Access Point local, DNS captive portal, `WebServer` pe portul 80, rute HTTP si verificare periodica a AP-ului.

Rutele principale sunt:

| Ruta | Metoda | Rol |
| --- | --- | --- |
| `/` | GET | Serveste aplicatia web |
| `/settings-page` | GET | Serveste pagina separata de setari si update firmware |
| `/status` | GET | Returneaza JSON cu starea feederului |
| `/settings` | GET | Returneaza setarile salvate ale motorului |
| `/settings` | POST | Salveaza setarile motorului daca feederul este oprit |
| `/update` | POST | Incarca si flash-uieste un firmware `.bin` nou daca feederul este oprit |
| `/toggle` | POST | Comuta feederul intre pornit si oprit |
| `/start` | POST | Porneste feederul daca era oprit |
| `/stop` | POST | Opreste feederul daca era pornit |

Rutele de captive portal (`/generate_204`, `/hotspot-detect.html`, `/ncsi.txt` etc.) redirectioneaza catre pagina principala, ca telefonul/laptopul sa gaseasca mai usor interfata.

## Pini folositi

### ESP32-WROOM

| Pin ESP32-WROOM | Rol |
| --- | --- |
| GPIO25 | STEP catre TMC2208 |
| GPIO26 | DIR catre TMC2208 |
| GPIO27 | ENABLE catre TMC2208 |
| GPIO14 | Buton catre GND |
| GPIO4 | Semnal senzor Hall, activ pe LOW |
| LED_BUILTIN / GPIO2 fallback | LED onboard |

GPIO6-GPIO11 nu sunt folositi deoarece pe modulele ESP32-WROOM sunt legati de memoria flash interna. Folosirea lor pentru cablaj extern poate bloca pornirea placii sau poate duce la comportament instabil.

### ESP32-C3 SuperMini

| Pin ESP32-C3 | Rol |
| --- | --- |
| GPIO7 | STEP catre TMC2208/TMC2209 |
| GPIO6 | DIR catre TMC2208/TMC2209 |
| GPIO10 | ENABLE catre TMC2208/TMC2209 |
| GPIO5 | Buton catre GND |
| GPIO4 | Semnal senzor Hall, activ pe LOW |
| GPIO8 | LED onboard, activ pe LOW |
| GPIO0 | RX UART, direct la PDN_UART TMC2208/TMC2209 |
| GPIO1 | TX UART, la PDN_UART prin rezistenta de 1 kOhm |

Pinii STEP, DIR si ENABLE din tabel sunt aceiasi pentru TMC2208 si TMC2209. Pentru legatura UART single-wire, RX se leaga la `PDN_UART`, iar TX ajunge pe aceeasi linie prin rezistenta de `1 kOhm`. Toate masele trebuie sa fie comune.

### Selectarea TMC2208 sau TMC2209 pe C3

Modelul se selecteaza la compilare in `src/board_config.h`:

```cpp
#define TMC_DRIVER_MODEL TMC_DRIVER_MODEL_2208
```

Pentru TMC2209 se foloseste:

```cpp
#define TMC_DRIVER_MODEL TMC_DRIVER_MODEL_2209
```

Configuratia curenta este pentru modulul BIGTREETECH TMC2208 V3.0. Cele doua rezistente marcate `R110` indica `R_SENSE = 0.11 Ohm`, valoare folosita de calculul curentului RMS. TMC2208 foloseste adresa UART fixa si constructorul fara parametru de adresa; TMC2209 foloseste adresa `0`, configurabila hardware prin MS1/MS2.

La initializare, firmware-ul citeste `IOIN.VERSION`: asteapta `0x20` pentru TMC2208 si `0x21` pentru TMC2209. Daca versiunea nu corespunde, setarile UART nu sunt aplicate si este raportata eroarea pe seriala. Pagina de setari afiseaza separat modelul configurat si starea comunicatiei UART.

## Cum functioneaza codul

### Constante, pini si setari persistente

In `namespace Pins` sunt definite toate conexiunile hardware. Daca placa nu defineste `LED_BUILTIN`, codul foloseste GPIO2 ca fallback, fiind pinul uzual pentru LED-ul onboard pe multe placi ESP32 DevKit.

Valorile implicite sunt acceleratie `400` pasi/s^2, ratie reductor `1` si preset `5` secunde pentru o rotatie la iesire. C3 porneste implicit cu microstepping `1/4` si curent RUN `800 mA RMS`; WROOM pastreaza microstepping-ul hardware `1/8`.

Viteza este calculata automat, nu mai este salvata direct:

```text
pasi_rotatie_iesire = 200 * microstepping * ratie_reductor
viteza_pasi_secunda = pasi_rotatie_iesire / secunde_per_rotatie
```

De exemplu, pentru C3 la `1/4`, ratie `1` si preset `5 s`, viteza este `800 / 5 = 160 pasi/s`.

La pornire, `loadMotorSettings()` citeste din namespace-ul NVS `feeder` urmatoarele chei:

| Cheie | Variabila | Valoare implicita |
| --- | --- | --- |
| `accel` | `motorAccelerationStepsPerSecond2` | `400` |
| `ratio` | `gearRatio` | `1.0` |
| `reverse` | `reverseRotation` | `false` |
| `rotationPreset` | `rotationPreset` | `5` secunde/rotatie |
| `runMinutes` | `motorRunDurationMinutes` | `20` minute |
| `microsteps` | `motorMicrosteps` | `4` pe C3 |
| `runCurrent` | `tmcRunCurrentMilliamps` | `800 mA RMS` pe C3 |

La salvare, `saveMotorSettings()` scrie aceleasi valori in flash. Valorile sunt limitate intre praguri minime si maxime inainte de aplicare.

Debounce-ul butonului este de `35 ms`. Presetul este limitat la `4-7` secunde/rotatie, acceleratia foloseste presetarile `200`, `400`, `600` si `800` pasi/s^2, iar ratia reductorului este limitata la `1-5`.

Pe C3, microstepping-ul poate fi `1/4`, `1/8` sau `1/16`, iar curentul RUN poate fi `600`, `800`, `900` sau `1000 mA RMS`. Curentul HOLD este calculat automat la 50% din RUN. Salvarea reaplica imediat microstepping-ul si curentul prin UART, apoi recalculeaza frecventa STEP pentru a pastra perioada mecanica selectata.

Diagnosticul serial citeste registrele comune ambelor drivere: `GCONF`, `CHOPCONF`, `IHOLD_IRUN`, `PWMCONF`, `DRV_STATUS` si `IFCNT`. Starile de supratemperatura, scurtcircuit, bobina deschisa, StealthChop si standstill sunt citite prin accessorii bibliotecii `TMCStepper`, nu prin masti de biti comune presupuse.

### Pornirea si oprirea motorului

Functia `toggleMotor()` inverseaza starea motorului:

- daca motorul era oprit, activeaza iesirile driverului si porneste miscarea cu `runForward()` sau `runBackward()`, in functie de setarea de directie;
- daca motorul era pornit, cere oprirea cu `stopMove()` si dezactiveaza iesirile driverului.

Driverul este dezactivat si la pornirea placii. Astfel, bobinele motorului nu raman alimentate inutil cand feederul este oprit, ceea ce reduce incalzirea motorului si a driverului.

La fiecare pornire manuala, firmware-ul porneste si un cronometru de sesiune. Durata este selectata din pagina de setari: `10`, `15` sau `20` minute. La expirare, firmware-ul opreste miscarea si dezactiveaza driverul. O secventa automata de recuperare dupa blocaj nu reseteaza cronometrul, astfel incat limita se aplica intregii sesiuni de functionare.

### Senzor Hall si detectarea blocajului

Senzorul Hall este configurat cu `INPUT_PULLUP` si este activ pe `LOW`. O trecere a magnetului prin fata senzorului este considerata o rotatie completa a mecanismului. Firmware-ul numara fronturile inactive-active si masoara timpul dintre doua rotatii.

Impulsurile Hall care apar mai repede de 50% din perioada selectata sunt considerate zgomot sau retrigger si sunt ignorate. Ele nu modifica perioada afisata, contorul de rotatii sau referinta folosita pentru detectarea blocajului si sunt raportate pe seriala pentru diagnostic.

Timeout-ul de blocaj este de doua ori perioada selectata, dar niciodata mai mic de `3000 ms`. Pentru presetul de `5 s/rotatie`, lipsa unui impuls Hall timp de peste `10 s` declanseaza recuperarea.

La primul blocaj, secventa automata este:

1. motorul este oprit si driverul este dezactivat temporar;
2. driverul este reactivat;
3. motorul executa trei cicluri de aproximativ `15` grade inainte si inapoi;
4. motorul reporneste in directia normala.

Daca apare inca un blocaj inainte ca senzorul Hall sa confirme o rotatie reusita, firmware-ul seteaza starea de blocaj permanent si asteapta o pornire manuala. O rotatie confirmata reseteaza contorul de incercari.

### Butonul

Butonul este configurat cu `INPUT_PULLUP`, deci starea normala este `HIGH`. Cand apesi butonul legat la GND, pinul devine `LOW`.

Functia `updateButton()` face debounce software. Ea accepta schimbarea de stare doar daca semnalul ramane stabil mai mult de `35 ms`. Motorul se comuta doar pe frontul de apasare, cand starea confirmata devine `LOW`.

### LED-ul onboard

Functia `updateStatusLed()` urmareste direct senzorul Hall. LED-ul este stins cat timp magnetul este detectat si aprins in rest. Codul tine cont de polaritatea diferita a LED-ului: activ pe HIGH la WROOM si activ pe LOW la C3 SuperMini.

### Bucla principala

`loop()` ruleaza continuu actualizari rapide si neblocante:

```cpp
webApp.loop();
updateButton();
updateRotationCounter();
updateJamRecovery();
updateMotorRunTimer();
updateStatusLed();
```

`webApp.loop()` proceseaza cererile DNS si HTTP, verifica periodic Access Point-ul si executa restartul programat dupa un update OTA. Aceasta structura lasa controlul motorului si monitorizarea Hall sa ruleze fara intarzieri lungi.

### Setarile din web UI

Sectiunea de setari din pagina web contine:

- ratia reductorului, implicit `1`;
- optional, un switch pentru inversarea directiei de rotatie;
- presetul pentru perioada unei rotatii: `4`, `5`, `6` sau `7` secunde;
- durata pana la oprirea automata: `10`, `15` sau `20` minute, implicit `20` minute;
- acceleratia: `200`, `400`, `600` sau `800` pasi/s^2, implicit `400`;
- pe C3, microstepping `1/4`, `1/8` sau `1/16`;
- pe C3, curent RUN `600`, `800`, `900` sau `1000 mA RMS`.

Afisarea controlului de directie este stabilita in `src/board_config.h`:

```cpp
#define ENABLE_DIRECTION_CONTROL 0
```

Cu valoarea `0`, controlul este ascuns, sensul normal este fortat la citirea NVS si la orice salvare, iar o valoare `reverse=true` ramasa dintr-un firmware anterior nu mai poate porni motorul invers fara indicatie in UI. Cu valoarea `1`, switch-ul este afisat si directia aleasa este salvata in NVS.

Pe C3, sectiunea TMC afiseaza `TMC2208` sau `TMC2209`, conform modelului selectat la compilare, si `Comunicare UART: OK` numai daca `IOIN.VERSION` a corespuns modelului la ultima configurare. Mesajul `FARA RASPUNS` indica de obicei model selectat gresit, cablaj PDN_UART incorect sau lipsa masei comune.

Campurile sunt dezactivate automat cat timp `motorRunning` este `true`. Endpoint-ul `/settings` refuza si el salvarea cu status `409` daca feederul ruleaza, deci protectia exista si in firmware, nu doar in interfata.

Ratia reductorului si presetul schimba viteza calculata a motorului. Endpoint-ul `/settings` returneaza atat viteza rezultata in pasi/s, cat si perioada calculata in secunde/rotatie.

Pagina principala afiseaza numarul de rotatii, perioada ultimei rotatii si avertizarea de blocaj. Campul `jammedPermanent` diferentiaza recuperarea automata in curs de situatia care necesita verificarea manuala a mecanismului.

Cat timp sesiunea este activa, endpoint-ul `/status` returneaza si `timerActive`, `runDurationSeconds` si `runRemainingSeconds`. Interfata transforma aceste valori intr-o numaratoare inversa `MM:SS` si o bara de progres. Bara este verde la inceput, galbena cand ramane cel mult 50% din timp si rosie cand ramane cel mult 20%. Cand motorul este oprit, timpul este afisat ca `--:--`.

### Update firmware din web UI

Sectiunea **Update firmware** este afisata sub setarile motorului. Utilizatorul alege un fisier `.bin`, apasa `UPDATE FIRMWARE`, apoi confirma intr-un dialog similar cu cel pentru salvarea setarilor.

In firmware, ruta `/update` foloseste biblioteca `Update` din framework-ul ESP32. Inainte de `Update.begin()`, firmware-ul pastreaza inceputul upload-ului in RAM si cauta markerul placii: `TTROBOT_FEEDER_C3_FW:` pentru C3 sau `TTROBOT_FEEDER_ESP32_FW:` pentru WROOM. Daca markerul nu apare in primii 32 KB, upload-ul este respins fara sa fie scris in flash. Dupa validare, upload-ul este scris incremental in slotul OTA liber. Daca update-ul se termina cu succes, raspunsul HTTP este trimis catre browser, apoi ESP32-ul reporneste dupa o mica intarziere.

Sectiunea este dezactivata in interfata cat timp `motorRunning` este `true`, iar endpoint-ul `/update` refuza update-ul cu status `409` daca feederul ruleaza. Inainte de scrierea firmware-ului, iesirile motorului sunt dezactivate.

## Observatii de review

Review-ul curent a verificat selectia TMC2208/TMC2209, persistenta NVS, pagina de setari, diagnosticul UART, controlul directiei si build-urile C3/WROOM. Codul ramane simplu si neblocant, iar separarea functiilor delimiteaza controlul motorului, citirea butonului si senzorului Hall de aplicatia web.

Puncte bune:

- foloseste `FastAccelStepper` in loc sa genereze manual impulsuri STEP;
- foloseste `millis()` pentru LED si debounce, fara `delay()`;
- tine driverul dezactivat la pornire;
- dezactiveaza driverul cand motorul este oprit, reducand incalzirea in repaus;
- verifica rotatia reala cu senzorul Hall, nu presupune ca impulsurile STEP au miscat mecanismul;
- incearca o recuperare limitata si trece in stare de eroare dupa doua blocari consecutive;
- nu foloseste GPIO6-GPIO11 pe ESP32-WROOM;
- butonul foloseste pull-up intern, deci necesita cablaj minim.
- valideaza modelul TMC prin `IOIN.VERSION` inainte de aplicarea setarilor UART;
- permite compilarea aceluiasi firmware C3 pentru TMC2208 sau TMC2209;
- nu pastreaza o directie inversata invizibila atunci cand controlul de directie este dezactivat.

Riscuri / lucruri de verificat pe placa reala:

- LED-ul onboard poate fi pe alt pin sau poate lipsi pe unele placi ESP32-WROOM; daca nu urmareste senzorul Hall, trebuie verificat pinul si polaritatea LED-ului placii.
- Daca driverul are pinul `EN` configurat diferit, `EnableActiveLevel` poate trebui schimbat din `LOW` in `HIGH`.
- Viteza si acceleratia sunt conservative, dar trebuie ajustate dupa mecanica, tensiune, curentul driverului si microstepping.
- Butonul nu are rezistor extern sau condensator de filtrare; debounce-ul software este suficient pentru test, dar la fire lungi poate fi nevoie de filtrare hardware.
- Senzorul Hall trebuie montat astfel incat sa produca un singur impuls clar la fiecare rotatie; zgomotul sau mai multi magneti vor altera numaratoarea si detectarea blocajului.
- Raspunsul HTTP de salvare nu diferentiaza inca fiecare eroare individuala `Preferences::put*`; o eroare rara de scriere NVS poate aplica valorile doar in RAM pana la restart. Mesajele seriale trebuie verificate daca setarile nu persista.

## Daca motorul sau driverul se incalzeste prea tare

Un motor pas cu pas se poate incalzi in functionare normala, dar temperatura trebuie sa ramana sub limita din fisa sa tehnica. Pentru diagnostic si reducerea temperaturii:

1. Pe C3, alege din pagina web curentul RUN potrivit motorului; calculul presupune `R_SENSE = 0.11 Ohm`. Pe un driver configurat analogic, regleaza `VREF` conform modulului si motorului.
2. Verifica faptul ca pinul ENABLE functioneaza si ca driverul chiar dezactiveaza bobinele cand feederul este oprit.
3. Verifica blocajele mecanice, alinierea axului, frecarea, sarcina si acceleratia. Un mecanism greu sau blocat mentine motorul solicitat si poate declansa repetat recuperarea.
4. Monteaza radiator pe driver si asigura ventilatie in carcasa. Daca driverul intra in protectie termica, motorul poate pierde pasi.
5. Verifica tensiunea sursei, conexiunile bobinelor si masa comuna. Nu conecta sau deconecta motorul cat timp driverul este alimentat.
6. Masoara temperatura cu un termometru. Aproximativ `50-60 C` poate fi normal pentru multe motoare pas cu pas; la `70-80 C` este prudent sa reduci curentul si sa verifici fisa tehnica a motorului.

Oprirea automata limiteaza durata unei sesiuni continue si dezactiveaza driverul la expirare. Ea nu inlocuieste reglarea corecta a curentului, eliminarea frecarilor si racirea driverului.

## Comenzi utile

Build:

```powershell
pio run --environment esp32-wroom
pio run --environment esp32-c3-supermini
```

Upload:

```powershell
pio run --target upload --environment esp32-wroom
pio run --target upload --environment esp32-c3-supermini
```

Monitor serial:

```powershell
pio device monitor
```
