# Changelog

## Unreleased

No change in what the firmware does. Around it: automated builds and tests,
release images built from tags, a Dutch README and build guide, and a P1
simulator. The setup page gets the new tab icon with the next release.

## 1.0.0 - 2026-11-27

First release.

- Reads every telegram from the P1 port (DSMR 4 and 5), checks it and serves it
  at `piepr.local`, port `8088`, to up to 5 programs at once
- WiFi setup from your phone, with a list of nearby networks
- Brings the setup hotspot back by itself when the saved network can't be used
