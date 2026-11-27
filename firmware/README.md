# Piepr firmware

Reads the smart meter's P1 port, checks each telegram's CRC16 and serves valid
telegrams raw on **TCP port 8088** (up to 5 readers; a 6th replaces the one connected longest). mDNS name:
**`piepr.local`** (`_dsmr._tcp`). WiFi is set up from a phone (setup portal).

- Stack: ESP-IDF 6.1.1 via PlatformIO (`espressif32 @ 8.0.0`), C. Only extra
  dependency: Espressif's `mdns` component.
- Meters: DSMR 4.x and 5.x, and Belgian e-MUCS P1; see the [meters table](../README.md#meters).
- Hardware: see the [build guide](../hardware/README.md).

## Flash a release (nothing to install)
1. Download the `.bin` file from the [latest release](https://github.com/ViezeVingertjes/piepr/releases/latest).
2. Open [Espressif's web flasher](https://espressif.github.io/esptool-js/) in Chrome or
   Edge, plug in the board, click **Connect** and pick its port. Not listed? Hold
   BOOT while plugging it in.
3. Set the flash address to `0x0`, choose the file and click **Program**.

Or from a terminal: `esptool.py --chip esp32c3 write_flash 0x0 piepr-<version>.bin`.
The release uses setup password **piepr1234**; for your own, build it yourself.
Flashing a release also clears the saved WiFi network: set it up again from your phone.

## Build it yourself
Needs [PlatformIO](https://platformio.org/install) (CLI or VS Code). The first build
downloads the toolchain.
```bash
cd firmware
cp secrets.ini.example secrets.ini   # set setup_pass (never committed)
pio run -t upload                    # build + flash over USB
pio device monitor                   # log
nc piepr.local 8088                  # a telegram every 10 s (DSMR 4) / 1 s (DSMR 5)
```
Then set up WiFi from your phone (below).

Reading it: any DSMR software that connects over TCP, at host `piepr.local`,
port `8088`. Home Assistant: Settings → Devices & services → Add integration →
*DSMR Smart Meter* → *Network*; DSMR version 5 for most meters from 2016 on,
4 for older ones, 5B in Belgium. If `.local` doesn't resolve, use the IP of
`piepr` from your router's device list (or the log line `wifi: connected, ip=…`).

## WiFi setup (phone)
With no network configured, the device starts in setup mode and stays there until
you save one. To change the network later: **hold BOOT until the LED double-blinks**
(about 5 s; not RST, and not during power-up).

1. Join **`Piepr-Setup`** with **piepr1234** (release firmware) or your own `setup_pass`. The page opens
   by itself (or browse to `http://192.168.4.1`).
2. Pick your network from the list (the strongest usable ones, after a scan of about
   8 s; ↻ scans again). 2.4 GHz only. Use *Other network…* for hidden or weak ones.
3. Enter the password (shown while typing; skipped for open networks).
4. Save. The device restarts and joins.

The page works offline (gzipped in flash, about 5 KB). Name 1–32 bytes, password
empty or 8–63 bytes; checked on the page and again on the device. Without
JavaScript, or on old browsers, it falls back to a plain form.

To leave without saving: press RST, or wait 10 minutes without page activity.

Android: if the page doesn't open on a phone that joined the hotspot before,
*forget* `Piepr-Setup` once and join again.

## Automatic recovery
When the saved network can't be used, the setup hotspot comes back and the page says why:

| Situation | When | Page says |
|---|---|---|
| Never connected since start-up | after 5 min | *Couldn't connect to "X"* |
| Password rejected 3× in a row | right away | *"X" rejected the password* |
| Connected before, then lost | keeps retrying; restarts after 15 min offline, then the rules above apply | |

After recovery the hotspot stays up 5 minutes after the last page activity, then the
device retries the saved network. A temporary outage needs no action.

## Project layout
| Path | Contents |
|---|---|
| `src/main.c` | Start-up (normal or setup boot); supervisor (heartbeat, restarts) |
| `src/config.h` | Tunables: pins, hostname, port, limits, timings (recovery thresholds: `lib/portalcore/include/recovery.h`) |
| `src/p1_reader.c` | UART on GPIO4 (inverted) → framer → broadcast; per-minute stats |
| `src/tcp_server.c` | TCP server: client slots (oldest evicted when full), non-blocking sends |
| `src/wifi.c` | Station with back-off and recovery counters; setup hotspot; scans |
| `src/wifi_credentials.c` | The saved network (NVS, written by the setup portal) |
| `src/setup_button.c` | BOOT long-press, polled from the LED tick |
| `src/setup_portal.c` | Setup request/restart; setup boot: hotspot, web form, captive DNS, timeout |
| `src/mdns_announce.c` | mDNS hostname + service |
| `src/status_led.c` | LED patterns; 125 ms tick also polls the button |
| `web/` | Setup pages, gzipped and embedded at build time |
| `lib/p1core/` | Pure C: `crc16`, `p1_framer` |
| `lib/portalcore/` | Pure C: `wifi_form`, `captive_dns`, `channel_pick`, `json_escape`, `setup_state` (network list, page JSON), `recovery` |
| `test/` | Unity tests, one folder per module (`pio test -e native`) |
| `fuzz/` | libFuzzer harnesses: `fuzz/run.sh <target> [seconds]` |
| `scripts/` | Build steps: `gen_secrets.py`, `gen_version.py`, `gen_web.py`, `gen_common.py`, `native_sanitizers.py`; `release_image.sh` makes the release file |
| `sdkconfig.defaults` | ESP-IDF options (explained in the file) |

Conventions: module name as function prefix (`tcp_server_*`), file-static state `s_`,
constants `UPPER_CASE`. Format with `.clang-format`
(`clang-format -i src/*.[ch] lib/*/*/*.[ch] test/*/*.c fuzz/*.c`).

## Configuration
- `secrets.ini` (from `secrets.ini.example`):
  - `setup_pass`: hotspot password, 8–63 bytes (the build fails otherwise).
  - Value = everything after the first `=`, trimmed; spaces, quotes, `\`, `;`, `#` are fine.
- App settings: `src/config.h`.
- ESP-IDF options: `sdkconfig.defaults`. After changing it or the ESP-IDF version:
  `rm -rf .pio/build/supermini sdkconfig.supermini`.
- Compiler warnings in `src/` are errors.

## LED (GPIO8)
| LED | Meaning |
|---|---|
| slow blink (2 Hz) | no WiFi / no IP |
| double blink (once a second) | setup mode |
| on | connected |
| brief flicker | valid telegram |

## Testing
- `pio test -e native` (Linux/macOS, gcc or clang): unit tests for all pure-C
  modules, with AddressSanitizer and UndefinedBehaviorSanitizer.
- `fuzz/run.sh p1_framer|wifi_form|captive_dns|json_escape [seconds]` (needs clang).

## Security
- The TCP stream is **plain text without authentication** on your LAN, by design.
- WiFi and setup passwords are in flash; physical USB access can read them.
- The setup hotspot (WPA2) only exists in setup mode.

## Troubleshooting
The log lines are explained in the [log reference](docs/log-reference.md).

| Symptom | Check |
|---|---|
| `ok 0`, uart errors 0 | pin 2 (Data Request) at 5 V? cable seated? |
| `crc` / `uart errors` high | 2.2 kΩ pull-up to 3V3; data wire on GPIO4 |
| LED keeps blinking slowly | WiFi details; weak signal (RSSI). Setup appears after 5 min |
| Network missing from the setup list | 5 GHz only, weak signal, hidden SSID (use *Other network…*), or a non-UTF-8 name (rename it) |
| Setup page doesn't open | forget `Piepr-Setup` on the phone and rejoin, or browse to `http://192.168.4.1` |
| Build fails: `setup_pass … must be 8-63 bytes` | set `setup_pass` in `secrets.ini` |
| No values in your software | right DSMR version, port 8088; does `nc piepr.local 8088` work? |
| Build fails after an ESP-IDF / `sdkconfig.defaults` change | clean build (see Configuration) |

## More
- [Log reference](docs/log-reference.md): every log line and what it means
- [How Piepr stays up](docs/internals.md): the rules the firmware keeps to
- [Testing without a meter](docs/simulator.md): a pretend meter for your desk
- [Security](../SECURITY.md): what is protected and what is not
