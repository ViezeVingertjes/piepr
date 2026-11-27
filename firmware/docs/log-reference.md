# Log reference

What Piepr prints on its USB serial port (`pio device monitor`, or *Show the log* on
the [install page](https://piepr.nl/en/install/)), and what each line means.

The log holds Piepr's own lines, plus warnings and errors from the software underneath
(ESP-IDF). The few lines right after a reset that start with `ESP-ROM` come from the
chip itself.

| Message | Meaning |
|---|---|
| `main: piepr <version> starting after … (reset reason N)` | start-up, with the cause in words: power-on, a restart, a reset over USB. A warning after a crash, a hang (watchdog) or a power dip (weak charger or cable) |
| `main: fw …, uptime …, clients n/5, heap free …, rssi …` | heartbeat every 5 minutes |
| `main: no WiFi for N s, restarting` | 15 min offline |
| `main: no readers for N s while online, restarting` | readers seen before, none for 30 min while online |
| `main: heap exhausted (largest block N), restarting` | memory low |
| `main: nvs init failed … / nvs unavailable …` | flash NVS problem; runs on, setup disabled |
| `main: recovery (no_connect / auth) for "…": starting WiFi setup` | automatic recovery |
| `wifi_creds: using the saved WiFi network` | WiFi set up earlier |
| `wifi_creds: no WiFi configured` | going to setup mode |
| `net: connecting to "…"` | first attempt after start-up |
| `net: connected, ip=…, signal N dBm` | online |
| `net: not connected yet (reason N); retrying in N s` | one attempt that didn't get through; normal right after a reset, when the access point still holds the previous session |
| `net: disconnected (reason N: …); retrying in N s` | the connection was lost, or a second attempt in a row failed, with the reason in words where there is something to do about it (network not found; network found, but its security does not fit; sign-in failed: wrong password or weak signal; access point stopped answering). Retries after 1 s, growing to 30 s |
| `net: connect failed: …; retrying in N s` | attempt rejected before it started; retried |
| `… ; no retry scheduled` at the end of either line | the retry could not be planned; Piepr restarts after 15 minutes offline |
| `net: lost IP address, reconnecting` | DHCP lease lost |
| `setup: restarting into setup mode` | entering setup (button or recovery) |
| `setup: access point "Piepr-Setup" on channel C, open http://192.168.4.1/` | setup ready |
| `setup: a device joined the hotspot` / `left the hotspot` | your phone joined or left Piepr-Setup |
| `net: N networks nearby, M listed` | scan for the setup page done |
| `setup: saved WiFi settings for "…", restarting` | saved |
| `setup: setup timeout, restarting` | no page activity for 10 min (5 after recovery); only with a known network |
| `button: hold BOOT for 5 s to start WiFi setup` | button armed |
| `mdns: announcing piepr.local, _dsmr._tcp port 8088` | mDNS up |
| `tcp: listening on port 8088 (max 5 clients)` | server up |
| `tcp: client … connected (n/5)` / `disconnected (n/5)` | reader joined/left |
| `tcp: client evicted (oldest) (n/5)` | 6th reader; the one connected longest is closed |
| `tcp: client sent data (not allowed), disconnected` | readers must only read |
| `tcp: client dropped (too slow, sent N) / (send error N) (n/5)` | reader too slow or gone |
| `tcp: client slot N connection error (errno N)` | aborted/reset (113 keepalive timeout, 104 reset by peer) |
| `p1: reading P1 on GPIO4, 115200 baud, inverted` | UART up |
| `p1: telegrams arriving from "…" (N bytes each)` | the first valid telegram, with the meter's own type name; shown again when telegrams return after a silent minute |
| `p1: last 60s: ok N, crc N, overflow N, uart errors N; last valid telegram Ns ago` | every minute; DSMR 4: `ok 6`, errors 0 |
| same, as warning with `(check wiring / pull-up / pin 2)` | nothing valid for ≥ 60 s |

Signal strength (RSSI): better than −70 dBm fine; −70 to −80 marginal; worse than −80 expect dropouts.
