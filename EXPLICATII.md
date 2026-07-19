# Explicatii proiect ESP32-WROOM feeder

Acest proiect controleaza un motor pas cu pas NEMA 17 printr-un driver TMC2208, folosind un ESP32-WROOM / ESP32 DevKit. Motorul este comandat in modul STEP/DIR, iar pornirea si oprirea se fac dintr-un singur buton.

## Pe scurt

- ESP32-ul porneste si configureaza pinii pentru motor, buton si LED.
- Driverul TMC2208 este tinut dezactivat la pornire.
- La apasarea butonului, motorul porneste si ruleaza continuu inainte.
- La urmatoarea apasare, motorul se opreste controlat, apoi driverul este dezactivat.
- ESP32-ul creeaza un Access Point WiFi si serveste o aplicatie web la `http://192.168.4.1`.
- Din aplicatia web se poate porni/opri feederul cu un buton START/STOP.
- LED-ul onboard clipeste la fiecare 2 secunde doar cat timp feederul este pornit; cand feederul este oprit, LED-ul sta stins.

## Fisiere importante

- `platformio.ini` defineste placa, framework-ul Arduino, viteza seriala si biblioteca `FastAccelStepper`.
- `src/main.cpp` contine tot firmware-ul pentru ESP32.
- `README.md` contine schema de conectare si comenzile de build/upload.

## Configuratia PlatformIO

Mediul folosit este `esp32-wroom`, cu placa `esp32dev`. Aceasta tinta PlatformIO este potrivita pentru multe placi ESP32-WROOM / ESP32 DevKit clasice.

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

Schimbarea schemei de partitii trebuie incarcata o data prin USB, deoarece partition table-ul este scris separat de aplicatie. Dupa aceea, se poate implementa endpoint-ul OTA in aplicatia web.

Biblioteca principala este:

```ini
lib_deps =
  gin66/FastAccelStepper@^0.30.0
```

`FastAccelStepper` genereaza impulsurile STEP mai precis decat o bucla manuala cu `delayMicroseconds`, ceea ce ajuta la miscarea mai stabila a motorului.

Bibliotecile `WiFi`, `DNSServer` si `WebServer` vin din framework-ul Arduino pentru ESP32 si sunt folosite pentru Access Point, captive portal si pagina web de control.

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
| `/status` | GET | Returneaza JSON cu starea feederului |
| `/toggle` | POST | Comuta feederul intre pornit si oprit |
| `/start` | POST | Porneste feederul daca era oprit |
| `/stop` | POST | Opreste feederul daca era pornit |

Rutele de captive portal (`/generate_204`, `/hotspot-detect.html`, `/ncsi.txt` etc.) redirectioneaza catre pagina principala, ca telefonul/laptopul sa gaseasca mai usor interfata.

## Pini folositi

| Pin ESP32-WROOM | Rol |
| --- | --- |
| GPIO25 | STEP catre TMC2208 |
| GPIO26 | DIR catre TMC2208 |
| GPIO27 | ENABLE catre TMC2208 |
| GPIO14 | Buton catre GND |
| LED_BUILTIN / GPIO2 fallback | LED onboard |

GPIO6-GPIO11 nu sunt folositi deoarece pe modulele ESP32-WROOM sunt legati de memoria flash interna. Folosirea lor pentru cablaj extern poate bloca pornirea placii sau poate duce la comportament instabil.

## Cum functioneaza codul

### Constante si pini

In `namespace Pins` sunt definite toate conexiunile hardware. Daca placa nu defineste `LED_BUILTIN`, codul foloseste GPIO2 ca fallback, fiind pinul uzual pentru LED-ul onboard pe multe placi ESP32 DevKit.

Viteza motorului este setata la `800` pasi pe secunda, iar acceleratia la `400` pasi pe secunda la patrat. Debounce-ul butonului este de `35 ms`, iar LED-ul isi schimba starea la fiecare `2000 ms`.

### Pornirea si oprirea motorului

Functia `toggleMotor()` inverseaza starea motorului:

- daca motorul era oprit, activeaza iesirile driverului si porneste miscarea inainte cu `runForward()`;
- daca motorul era pornit, cere oprirea cu `stopMove()` si marcheaza driverul pentru dezactivare dupa ce motorul chiar s-a oprit.

Dezactivarea driverului dupa oprire este gestionata de `updateMotorEnable()`. Asta evita taierea brusca a iesirilor inainte ca libraria sa termine oprirea miscarii.

### Butonul

Butonul este configurat cu `INPUT_PULLUP`, deci starea normala este `HIGH`. Cand apesi butonul legat la GND, pinul devine `LOW`.

Functia `updateButton()` face debounce software. Ea accepta schimbarea de stare doar daca semnalul ramane stabil mai mult de `35 ms`. Motorul se comuta doar pe frontul de apasare, cand starea confirmata devine `LOW`.

### LED-ul onboard

Functia `updateStatusLed()` foloseste `millis()` pentru temporizare. Nu foloseste `delay()`, deci nu blocheaza citirea butonului, serverul web sau controlul motorului.

LED-ul ramane stins cand `motorRunning` este `false`. Cand feederul este pornit, LED-ul isi schimba starea la fiecare 2 secunde.

### Bucla principala

`loop()` ruleaza continuu trei actualizari rapide:

```cpp
updateButton();
updateMotorEnable();
updateStatusLed();
```

Inainte de acestea, bucla proceseaza cererile DNS si HTTP:

```cpp
dnsServer.processNextRequest();
server.handleClient();
```

Aceasta structura este buna pentru firmware simplu deoarece fiecare functie face putin lucru si revine imediat.

## Observatii de review

Codul este potrivit pentru scopul actual: simplu, neblocant si usor de modificat. Separarea functiilor face clar ce parte controleaza motorul, ce parte citeste butonul si ce parte clipeste LED-ul.

Puncte bune:

- foloseste `FastAccelStepper` in loc sa genereze manual impulsuri STEP;
- foloseste `millis()` pentru LED si debounce, fara `delay()`;
- tine driverul dezactivat la pornire;
- nu foloseste GPIO6-GPIO11 pe ESP32-WROOM;
- butonul foloseste pull-up intern, deci necesita cablaj minim.

Riscuri / lucruri de verificat pe placa reala:

- LED-ul onboard poate fi pe alt pin sau poate lipsi pe unele placi ESP32-WROOM; daca nu clipeste, trebuie verificat pinul LED-ului placii.
- Unele placi au LED activ pe `LOW`, caz in care clipirea exista, dar starea aprins/stins este inversata.
- Daca driverul TMC2208 are pinul `EN` configurat diferit, `EnableActiveLevel` poate trebui schimbat din `LOW` in `HIGH`.
- Viteza si acceleratia sunt conservative, dar trebuie ajustate dupa mecanica, tensiune, curentul driverului si microstepping.
- Butonul nu are rezistor extern sau condensator de filtrare; debounce-ul software este suficient pentru test, dar la fire lungi poate fi nevoie de filtrare hardware.

## Comenzi utile

Build:

```powershell
pio run
```

Upload:

```powershell
pio run --target upload --environment esp32-wroom
```

Monitor serial:

```powershell
pio device monitor
```
