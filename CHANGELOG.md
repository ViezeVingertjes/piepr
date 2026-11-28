# Changelog

## 1.4.0 - 2026-11-28

- A status page on the device: open `http://piepr.local` for the power right now, per
  phase, the meter readings and gas, and how Piepr itself is doing: when the last
  telegram came, how many arrived and how many were rejected, readers, WiFi signal,
  uptime. Old values are dimmed and what is wrong is shown in red. In English and
  Dutch, on a phone and on a computer; the same facts as JSON at `/api/status`
- The setup page looks like [piepr.nl](https://piepr.nl) now, and so does the status
  page: the same colours, type, buttons and pulse line, in light and dark
- After saving WiFi, the setup page points at the status page
- The pages are built from shared pieces, and comments and spare whitespace are left
  out of what goes into the firmware
- Installing 1.4.0 goes over USB, from [piepr.nl/installeren](https://piepr.nl/installeren/)
  or by hand, and clears the saved WiFi network: set it up again from your phone

## 1.3.0 - 2026-11-28

- A circuit board for the same parts, with an RJ12 socket: [hardware/pcb](hardware/pcb)
- Also runs on the Seeed Studio XIAO ESP32-S3 ([build guide](hardware/README.md))
- The release has one file per board, such as `piepr-supermini-<version>.bin`
- Installing 1.3.0 goes over USB, from [piepr.nl/installeren](https://piepr.nl/installeren/)
  or by hand, and clears the saved WiFi network: set it up again from your phone

## 1.2.1 - 2026-11-27

- Switching language on the setup page keeps the step you were on, the network list
  and what you typed, instead of starting over with a new scan
- A single WiFi attempt that fails and is followed by a connection is no longer logged
  as a warning
- The Dutch README and build guide have their banner and drawings in Dutch
- Installing 1.2.1 goes over USB, from [piepr.nl/installeren](https://piepr.nl/installeren/)
  or by hand, and clears the saved WiFi network: set it up again from your phone

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
