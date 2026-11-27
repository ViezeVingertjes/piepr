# Changelog

## Unreleased

- A link at the top of the setup page switches between Dutch and English

## 1.1.0 - 2026-11-27

- The setup page is in Dutch on a phone set to Dutch, and in English otherwise
- Room for two firmware versions on the board, as groundwork for updates without
  a USB cable. Because of this, 1.1.0 itself is installed over USB like before,
  which clears the saved WiFi network: set it up again from your phone
- New tab icon on the setup page

Around the firmware: automated builds and tests, release images built from tags,
a Dutch README and build guide, and a P1 simulator.

## 1.0.0 - 2026-11-27

First release.

- Reads every telegram from the P1 port (DSMR 4 and 5), checks it and serves it
  at `piepr.local`, port `8088`, to up to 5 programs at once
- WiFi setup from your phone, with a list of nearby networks
- Brings the setup hotspot back by itself when the saved network can't be used
