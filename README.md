<p align="center">
  <img src="assets/banner.svg" alt="Piepr: your smart meter, live on your own network" width="100%">
</p>

<p align="center">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-FFC21A?labelColor=0E2236"></a>
  <img alt="DSMR 4 and 5" src="https://img.shields.io/badge/DSMR-4%20%7C%205-FFC21A?labelColor=0E2236">
  <img alt="ESP32-C3" src="https://img.shields.io/badge/board-ESP32--C3%20SuperMini-FFC21A?labelColor=0E2236">
  <img alt="ESP-IDF 6.1" src="https://img.shields.io/badge/ESP--IDF-6.1-FFC21A?labelColor=0E2236">
</p>

**Piepr** is a small open-source reader for Dutch and Belgian smart meters. Plug it
into the meter's P1 port, connect it to WiFi from your phone, and every reading is
available on your own network. No app, no account, no cloud.

- Reads every telegram from the P1 port and checks it (CRC) before passing it on
- Serves the raw telegrams at `piepr.local`, port `8088`, for any software that
  reads P1 data: Home Assistant (DSMR Smart Meter), Domoticz (P1 Smart Meter with LAN
  interface), DSMR-reader, or your own script; up to 5 at the same time
- WiFi setup from your phone, with a list of nearby networks
- Recovers by itself: if WiFi fails, the setup hotspot comes back and says why
- About €16 delivered for your first one (about €5.40 in parts each): an ESP32-C3
  SuperMini, an RJ12 cable, a resistor and a diode

<p align="center">
  <img src="assets/setup-flow.png" alt="Setting up WiFi from a phone: pick a network, enter the password, saved" width="820">
</p>

## Using Piepr

1. Plug Piepr into the meter's **P1 port** and into a USB charger.
2. When the light **double-blinks**, join the WiFi network **Piepr-Setup** on your
   phone. The password is **piepr1234**, or your own `setup_pass` if you built the
   firmware yourself. The setup page opens by itself (otherwise browse to
   `http://192.168.4.1`).
3. Pick your home network, enter its password and save. Piepr works with
   **2.4 GHz** WiFi only.
4. When the light **stays on**, Piepr is connected.
5. Point your software at `piepr.local`, port `8088`. In Home Assistant: Settings →
   Devices & services → Add integration → DSMR Smart Meter → Network. DSMR version:
   5 for most meters from 2016 on, 4 for older ones, 5B in Belgium.

To change the WiFi later, hold the **BOOT** button until the light double-blinks
(about 5 seconds).

| Light | Meaning |
|---|---|
| on, with a short flicker per reading | connected and reading the meter |
| slow blink | looking for WiFi |
| double blink | setup mode: join Piepr-Setup |

| Problem | What to do |
|---|---|
| Piepr-Setup isn't in your WiFi list | wait 5 minutes, or hold BOOT until the light double-blinks |
| Your network isn't listed | it must be 2.4 GHz with a good signal; move the router or an extender closer, or use *Other network…* |
| `piepr.local` doesn't work | find `piepr` in your router's device list and use its IP address |

## Build your own

1. **Hardware:** parts list (about €16), wiring and checks in the [build guide](hardware/README.md).
2. **Firmware:** flash the [latest release](https://github.com/ViezeVingertjes/piepr/releases/latest)
   straight from your browser, or build your own; see the [firmware guide](firmware/README.md).

| Folder | Contents |
|---|---|
| [`hardware/`](hardware) | Build guide and wiring diagrams |
| [`firmware/`](firmware) | ESP-IDF firmware, tests and fuzzers |
| [`assets/`](assets) | Logo and images |

## License

MIT, see [LICENSE](LICENSE).
