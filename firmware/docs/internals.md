# How Piepr stays up

The rules the firmware keeps to, so it can sit in a meter cupboard
without anyone looking at it.

- No allocation in steady state: fixed 4 KB telegram buffer, 5 client slots.
- Framer: `/` starts a telegram; oversized frames dropped; only complete telegrams
  with a valid CRC are sent. Every state has an exit.
- UART: each event drains the RX buffer; overflow → flush + reset; ISR in IRAM.
  Errors are counted and logged once a minute (no log flood when unplugged).
- TCP: sends never block; a client that can't take a whole telegram is dropped;
  clients that send data are disconnected; keepalive finds dead clients (~60 s);
  when full, the oldest client makes room. Half-closed clients (`nc … < /dev/null`)
  are dropped by keepalive while the meter is silent, by design.
- WiFi: strongest AP for the SSID; reconnects forever with back-off; a lost IP
  forces a fresh connect; restart after 15 min offline; [automatic recovery](../README.md#automatic-recovery).
- Silent failures: restart if readers were seen and none for 30 min while
  online (e.g. a mesh node without uplink), or if the largest free heap block drops
  under 16 KB.
- Setup portal: separate boot, always left by a restart. Form and DNS
  parsing is unit-tested and fuzzed. Name and password are stored as one record (a
  power cut can't mix old and new). A BOOT button stuck at start-up is ignored.
- Task watchdog (10 s) on the reader, server and supervisor: a hang reboots.
