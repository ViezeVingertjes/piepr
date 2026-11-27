# Log reference

What Piepr prints on its USB serial port (`pio device monitor`, or the console of the
web flasher), and what each line means.

| Message | Meaning |
|---|---|
| `main: piepr <version> starting (reset reason N)` | boot; N: 1 power-on, 3 software restart, 4 panic, 5/6/7 watchdog, 9 brownout, 11 USB/flash |
| `main: fw …, uptime …, clients n/5, heap free …, rssi …` | heartbeat every 60 s |
| `main: no WiFi for N s, restarting` | 15 min offline |
| `main: no readers for N s while online, restarting` | readers seen before, none for 30 min while online |
| `main: heap exhausted (largest block N), restarting` | memory low |
| `main: nvs init failed … / nvs unavailable …` | flash NVS problem; runs on, setup disabled |
| `main: recovery (no_connect / auth) for "…": starting WiFi setup` | automatic recovery |
| `wifi_creds: using the saved WiFi network` | WiFi set up earlier |
| `wifi_creds: no WiFi configured` | going to setup mode |
| `wifi: connected, ip=…` | online |
| `wifi: disconnected (reason N)` + `reconnecting in N s` | retries, 1 → 30 s back-off |
| `wifi: connect failed: …` | attempt rejected; retried |
| `wifi: lost IP address, reconnecting` | DHCP lease lost |
| `setup: restarting into setup mode` | entering setup (button or recovery) |
| `setup: access point "Piepr-Setup" on channel C, open http://192.168.4.1/` | setup ready |
| `wifi: N networks nearby, M listed` | page scan done |
| `wifi:station: … join / leave`, `DHCP server assigned IP to a client` | phone joined/left (ESP-IDF lines) |
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
| `p1: last 60s: ok N, crc N, overflow N, uart errors N; last valid telegram Ns ago` | every minute; DSMR 4: `ok 6`, errors 0 |
| same, as warning with `(check wiring / pull-up / pin 2)` | nothing valid for ≥ 60 s |

Signal strength (RSSI): better than −70 dBm fine; −70 to −80 marginal; worse than −80 expect dropouts.
