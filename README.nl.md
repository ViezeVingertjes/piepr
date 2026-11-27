<p align="center">
  <img src="assets/banner.svg" alt="Piepr: je slimme meter, live op je eigen netwerk" width="100%">
</p>

<p align="center">
  <a href="https://github.com/ViezeVingertjes/piepr/actions/workflows/ci.yml"><img alt="Build" src="https://img.shields.io/github/actions/workflow/status/ViezeVingertjes/piepr/ci.yml?branch=main&label=build&labelColor=0E2236"></a>
  <a href="https://github.com/ViezeVingertjes/piepr/releases/latest"><img alt="Nieuwste release" src="https://img.shields.io/github/v/release/ViezeVingertjes/piepr?label=release&labelColor=0E2236&color=FFC21A"></a>
  <a href="LICENSE"><img alt="Licentie: MIT" src="https://img.shields.io/badge/license-MIT-FFC21A?labelColor=0E2236"></a>
</p>

<p align="center">Nederlands · <a href="README.md">English</a></p>

**Piepr** is een kleine open-source uitlezer voor Nederlandse en Belgische slimme
meters. Steek hem in de P1-poort van je meter, zet hem met je telefoon op wifi, en
elke meting staat op je eigen netwerk. Geen app, geen account, geen cloud.

- Leest elk telegram van de P1-poort en controleert het (CRC) voordat het wordt doorgegeven
- Geeft de ruwe telegrammen door op `piepr.local`, poort `8088`, aan alle software
  die P1-data leest: Home Assistant (DSMR Smart Meter), Domoticz (P1 Smart Meter with
  LAN interface), DSMR-reader of je eigen script; tot 5 tegelijk
- Wifi stel je in op je telefoon: je kiest uit een lijst met netwerken in de buurt
- Herstelt zichzelf: lukt wifi niet, dan komt de setup-hotspot terug en zie je waarom
- Ongeveer €16 inclusief verzending voor je eerste (ongeveer €5,40 aan onderdelen
  per stuk): een ESP32-C3 SuperMini, een RJ12-kabel, een weerstand en een diode

<p align="center">
  <img src="assets/setup-flow.png" alt="Wifi instellen op een telefoon: netwerk kiezen, wachtwoord invullen, opgeslagen" width="820">
</p>

## Piepr gebruiken

1. Steek Piepr in de **P1-poort** van de meter en in een USB-lader.
2. Zodra het lampje **dubbel knippert**, verbind je je telefoon met het wifi-netwerk
   **Piepr-Setup**. Het wachtwoord is **piepr1234**, of je eigen `setup_pass` als je
   de firmware zelf hebt gebouwd. De instelpagina opent vanzelf (zo niet, ga dan naar
   `http://192.168.4.1`). De pagina is in het Engels.
3. Kies je thuisnetwerk, vul het wachtwoord in en sla op. Piepr werkt alleen met
   **2,4 GHz**-wifi.
4. Als het lampje **blijft branden**, is Piepr verbonden.
5. Vul in je software `piepr.local` in, met poort `8088`. In Home Assistant:
   Instellingen → Apparaten & diensten → Integratie toevoegen → DSMR Smart Meter →
   Netwerk. DSMR-versie: 5 voor de meeste meters vanaf 2016, 4 voor oudere, 5B in
   België.

Later een ander wifi-netwerk instellen? Houd de **BOOT**-knop ingedrukt tot het lampje
dubbel knippert (ongeveer 5 seconden).

| Lampje | Betekenis |
|---|---|
| aan, met een korte flikkering per meting | verbonden en leest de meter |
| knippert langzaam | zoekt wifi |
| knippert dubbel | instelmodus: verbind met Piepr-Setup |

| Probleem | Wat je doet |
|---|---|
| Piepr-Setup staat niet in je wifi-lijst | wacht 5 minuten, of houd BOOT ingedrukt tot het lampje dubbel knippert |
| Je netwerk staat er niet bij | het moet 2,4 GHz zijn met een goed signaal; zet de router of een versterker dichterbij, of kies *Other network…* |
| `piepr.local` werkt niet | zoek `piepr` op in de apparatenlijst van je router en gebruik het IP-adres |

## Meters

| Meter | P1-versie | |
|---|---|---|
| Nederlandse slimme meters | DSMR 4.x, 5.x | ondersteund |
| Belgische digitale meters (Fluvius) | e-MUCS P1 | ondersteund; zet eerst de gebruikerspoorten aan op [mijn.fluvius.be/poortbeheer](https://mijn.fluvius.be/poortbeheer) |
| Oudere Nederlandse meters | DSMR 2.2, 3 | niet ondersteund (9600 baud, geen controlesom) |
| Luxemburg (Smarty) | | niet ondersteund (versleuteld) |

Piepr wordt ontwikkeld op een Landis+Gyr E350 (DSMR 4.0). Werkt er iets niet op jouw
meter? Maak een [metermelding](https://github.com/ViezeVingertjes/piepr/issues/new?template=meter-report.yml).

## Zelf bouwen

1. Hardware: onderdelenlijst (ongeveer €16), bedrading en controles in de [bouwgids](hardware/README.nl.md).
2. Firmware: flash de [nieuwste release](https://github.com/ViezeVingertjes/piepr/releases/latest)
   rechtstreeks vanuit je browser, of bouw hem zelf; zie de [firmwaregids](firmware/README.md) (Engels).

| Map | Inhoud |
|---|---|
| [`hardware/`](hardware) | Bouwgids en bedradingsschema's |
| [`firmware/`](firmware) | ESP-IDF-firmware, tests en fuzzers |
| [`assets/`](assets) | Logo en afbeeldingen |
| [`tools/`](tools) | Een [nepmeter](firmware/docs/simulator.md) om mee te testen (Engels), en het script voor de afbeeldingen |

## Licentie

MIT, zie [LICENSE](LICENSE).
