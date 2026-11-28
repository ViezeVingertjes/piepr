# Piepr firmware

Reads the smart meter's P1 port, checks each telegram's CRC16 and serves valid
telegrams raw on **TCP port 8088** (up to 5 readers; a 6th replaces the one connected longest). mDNS name:
**`piepr.local`** (`_dsmr._tcp`). WiFi is set up from a phone (setup portal).

- Stack: ESP-IDF 6.1.1 via PlatformIO (`espressif32 @ 8.0.0`), C. Only extra
  dependency: Espressif's `mdns` component.
- Meters: DSMR 4.x and 5.x, and Belgian e-MUCS P1; see the [meters table](../README.md#meters).
- Boards: see [Boards](#boards).
- Hardware: see the [build guide](../hardware/README.md).

## Flash a release (nothing to install)
Open [piepr.nl/en/install](https://piepr.nl/en/install/) in Chrome or Edge on a
computer, plug in the board with a USB cable that carries data, click **Install Piepr**
and pick the board's port. Not listed? Hold BOOT while plugging it in.

By hand: download your board's file from the
[latest release](https://github.com/ViezeVingertjes/piepr/releases/latest) and write it
at address `0x0`, for example with
`esptool --chip esp32c3 write-flash 0x0 piepr-supermini-<version>.bin` or
`esptool --chip esp32s3 write-flash 0x0 piepr-xiao-s3-<version>.bin`.

The release uses setup password **piepr1234**; for your own, build it yourself.
Flashing a release also clears the saved WiFi network: set it up again from your phone.

## Build it yourself
Needs [PlatformIO](https://platformio.org/install) (CLI or VS Code). The first build
downloads the toolchain.
```bash
cd firmware
cp secrets.ini.example secrets.ini   # set setup_pass (never committed)
pio run -e supermini -t upload       # build + flash over USB (or -e xiao-s3)
pio device monitor                   # log
nc piepr.local 8088                  # a telegram every 10 s (DSMR 4) / 1 s (DSMR 5)
```
Then set up WiFi from your phone (below).

Reading it: any DSMR software that connects over TCP, at host `piepr.local`,
port `8088`. Home Assistant: Settings → Devices & services → Add integration →
*DSMR Smart Meter* → *Network*; DSMR version 4 or 5, usually printed
on the front of the meter; 5B in Belgium. If `.local` doesn't resolve, use the IP of
`piepr` from your router's device list (or the log line `net: connected, ip=…`).

## WiFi setup (phone)
With no network configured, the device starts in setup mode and stays there until
you save one. To change the network later: **hold BOOT until the LED double-blinks**
(about 5 s; not RST, and not during power-up).

1. Join **`Piepr-Setup`** with **piepr1234** (release firmware) or your own `setup_pass`. The page opens
   by itself (or browse to `http://192.168.4.1`, Piepr's own address on that hotspot).
2. Pick your network from the list (the strongest usable ones, after a scan of about
   10 s; ↻ scans again). 2.4 GHz only. Use *Other network…* for hidden or weak ones.
3. Enter the password (shown while typing; skipped for open networks).
4. Save. The device restarts and joins.

The page is in Dutch on a phone set to Dutch and in English otherwise; a link at the
top switches between the two. It works
offline (gzipped in flash, about 5 KB per language). Name 1–32 bytes, password
empty or 8–63 bytes; checked on the page and again on the device. Without
JavaScript, or on old browsers, it falls back to a plain form.

To leave without saving (only when a network was saved before): press RST, or wait
10 minutes without page activity.

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
| `src/config.h` | Tunables: hostname, port, limits, timings (recovery thresholds: `lib/portalcore/include/recovery.h`) |
| `src/board.h`, `src/boards/` | The board being built (from its env in `platformio.ini`): pins, LED, setup button |
| `src/p1_reader.c` | UART on the board's P1 pin (inverted) → framer → broadcast; per-minute stats |
| `src/tcp_server.c` | TCP server: client slots (oldest evicted when full), non-blocking sends |
| `src/wifi.c` | Station with back-off and recovery counters; setup hotspot; scans |
| `src/wifi_credentials.c` | The saved network (NVS, written by the setup portal) |
| `src/setup_button.c` | BOOT long-press, polled from the LED tick (nothing on a board without a button) |
| `src/setup_portal.c` | Setup request/restart; setup boot: hotspot, web form, captive DNS, timeout |
| `src/mdns_announce.c` | mDNS hostname + service |
| `src/status_led.c` | LED patterns; 125 ms tick also polls the button |
| `web/` | Setup pages, written once in English: text marked `[[like this]]` is translated with `nl.json`, shared pieces live in `parts/`. Built into the firmware gzipped, in English and Dutch |
| `lib/p1core/` | Pure C: `crc16`, `p1_framer`, `p1_ident` (the meter's type name, for the log), `p1_parse` (the values in a telegram: readings, power, voltage, current, gas) |
| `lib/portalcore/` | Pure C: `wifi_form`, `captive_dns`, `channel_pick`, `json_escape`, `lang_pick`, `setup_state` (network list, page JSON), `recovery` |
| `test/` | Unity tests, one folder per module (`pio test -e native`) |
| `fuzz/` | libFuzzer harnesses: `fuzz/run.sh <target> [seconds]` |
| `scripts/` | Build steps: `gen_secrets.py`, `gen_version.py`, `gen_web.py` (with `web_pages.py` and its tests), `gen_common.py`, `native_sanitizers.py`; `release_image.sh` makes the release file; `format.sh` formats the C code |
| `sdkconfig.defaults` | ESP-IDF options for every board (explained in the file) |
| `dependencies.lock.<chip>` | Pinned component versions, one file per chip (`esp32c3`, `esp32s3`) |
| `partitions.csv` | Flash layout, the same on every board: two firmware slots and the settings |

Conventions: module name as function prefix (`tcp_server_*`), file-static state `s_`,
constants `UPPER_CASE`. Format with `scripts/format.sh`.

## Configuration
- `secrets.ini` (from `secrets.ini.example`):
  - `setup_pass`: hotspot password, 8–63 bytes (the build fails otherwise).
  - Value = everything after the first `=`, trimmed; spaces, quotes, `\`, `;`, `#` are fine.
- App settings: `src/config.h`; pins: `src/boards/<board>.h`.
- ESP-IDF options: `sdkconfig.defaults`. After changing it or the ESP-IDF version, for
  each board: `rm -rf .pio/build/<board> sdkconfig.<board>`.
- Compiler warnings in `src/` are errors.

## Boards
| Env (`-e`) | Board | Chip | P1 data | LED | Setup button |
|---|---|---|---|---|---|
| `supermini` | ESP32-C3 SuperMini | ESP32-C3 | GPIO4 (pad 4) | GPIO8 | BOOT (GPIO9) |
| `xiao-s3` | Seeed Studio XIAO ESP32-S3 | ESP32-S3 | GPIO9 (D10) | GPIO21 | BOOT (GPIO0) |

`pio run` builds every board. The release has `piepr-<board>-<version>.bin` per board,
and a `manifest.json` listing each board's chip and image: the installer at piepr.nl
picks the image by the chip it finds. Every board has the same flash layout.

Adding a board:
1. `src/boards/<board>.h` (the env name, `-` written as `_`) with the settings listed
   in `src/board.h`. It checks the chip (`CONFIG_IDF_TARGET_*`). Leave
   `BOARD_SETUP_BUTTON_GPIO` out if there is no button: WiFi setup then comes back only
   through [recovery](#automatic-recovery). Keep P1 off strapping pins and USB pins.
2. A line for it in `src/board.h`.
3. `[env:<board>]` in `platformio.ini` (`extends = esp32`, its `board`, and
   `build_flags = -DPIEPR_BOARD_<BOARD>`), added to `default_envs` so that CI builds it
   and the release includes it.
4. A new chip also gets a `dependencies.lock.<chip>`: the first build writes it; commit
   it. A second board with a chip that is already supported needs a board choice in
   the piepr.nl installer first, which tells boards apart by chip.
5. Its row in the table above, and its pads in the [build guide](../hardware/README.md).

## LED
| LED | Meaning |
|---|---|
| even blink (2 Hz) | no WiFi / no IP |
| double blink (once a second) | setup mode |
| on | connected |
| brief flicker | valid telegram |

## Testing
- `pio test -e native` (Linux/macOS, gcc or clang): unit tests for all pure-C
  modules, with AddressSanitizer and UndefinedBehaviorSanitizer.
- `fuzz/run.sh <target> [seconds]` (needs clang); the targets are the `fuzz/fuzz_*.c` files.
- `python3 -m unittest discover -s scripts`: tests of the page build step.

## Security
- The TCP stream is **plain text without authentication** on your LAN, by design.
- WiFi and setup passwords are in flash; physical USB access can read them.
- The setup hotspot (WPA2) only exists in setup mode.

## Troubleshooting
The log lines are explained in the [log reference](docs/log-reference.md).

| Symptom | Check |
|---|---|
| `ok 0`, uart errors 0 | pin 2 (Data Request) at 5 V? cable seated? |
| `crc` / `uart errors` high | 2.2 kΩ pull-up to 3V3; data wire on the P1 pin ([Boards](#boards)) |
| LED keeps blinking evenly | WiFi details; weak signal (RSSI). Setup appears after 5 min |
| Network missing from the setup list | 5 GHz only, weak signal, hidden SSID (use *Other network…*), or a non-UTF-8 name (rename it) |
| Setup page doesn't open | forget `Piepr-Setup` on the phone and rejoin, or browse to `http://192.168.4.1` |
| Build fails: `setup_pass … must be 8-63 bytes` | set `setup_pass` in `secrets.ini` |
| No values in your software | right DSMR version, port 8088; does `nc piepr.local 8088` work? |
| Build fails after an ESP-IDF / `sdkconfig.defaults` change | clean build (see Configuration) |

## More
- [Log reference](docs/log-reference.md): every log line and what it means
- [How Piepr stays up](docs/internals.md): the rules the firmware keeps to
- [Testing without a meter](docs/simulator.md): a pretend meter, to set up your software first
- [Security](../SECURITY.md): what is protected and what is not
