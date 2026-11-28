# Security

## Reporting a problem

Please report security problems privately: open the repository's
[Security tab](https://github.com/ViezeVingertjes/piepr/security) and choose
*Report a vulnerability*. Fixes go into the latest release.

## Known, and by design

Piepr is made for a home network you trust. These are choices, not bugs:

- **The readings are not protected on your network.** Port 8088 sends plain
  text to anyone on the network who connects, and the status page (port 80)
  shows the same readings without a sign-in. The status page only shows: it
  has nothing to change. Keep Piepr on a network you trust, or on a separate
  one for your devices.
- **The setup password of the release firmware is public** (`piepr1234`), and
  the setup hotspot comes back by itself when the saved network can't be used.
  While it is up, someone within WiFi range can connect Piepr to another
  network. They can't read your WiFi password from it. To close this, build
  the firmware with your own `setup_pass`.
- **The flash is not encrypted.** Someone holding the board and a USB cable can
  read the saved WiFi password.
