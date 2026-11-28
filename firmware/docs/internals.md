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
- Status page: a telegram goes to the readers first and is only then parsed for the
  page; the counts and values are one record behind a lock that is held just long
  enough to change or copy it. The
  web server's task runs below the reader's and the stream's, takes 3 browsers (a 4th
  replaces the one idle longest) and answers from one fixed buffer. If it can't start,
  the stream works without it; a browser that stalls is dropped after 3 seconds. The
  page asks for `/api/status` every 2 seconds, one request at a time, and not while
  its tab is hidden.
- WiFi: strongest AP for the SSID; reconnects forever with back-off; a lost IP
  forces a fresh connect; restart after 15 min offline; [automatic recovery](../README.md#automatic-recovery).
- Silent failures: restart if readers were seen and none for 30 min while
  online (e.g. a mesh node without uplink), or if the largest free heap block drops
  under 16 KB.
- Setup portal: separate boot, always left by a restart. Form and DNS
  parsing is unit-tested and fuzzed. Name and password are stored as one record (a
  power cut can't mix old and new). A BOOT button stuck at start-up is ignored.
- Task watchdog (10 s) on the reader, server and supervisor: a hang reboots.
