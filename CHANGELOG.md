# Changelog

## 1.2.0 - 2026-11-27

- A link at the top of the setup page switches between Dutch and English
- A shorter, clearer log: only Piepr's own lines, with the reason for a start-up or a
  lost WiFi connection in words, a line when telegrams start arriving and when a
  device joins the setup hotspot. The WiFi lines are now tagged `net:`
- Installing 1.2.0 goes over USB, from [piepr.nl/installeren](https://piepr.nl/installeren/)
  or by hand, and clears the saved WiFi network: set it up again from your phone

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
