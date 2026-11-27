# Testing without a meter

`tools/p1sim.py` is a pretend smart meter. It writes valid telegrams, with a correct
checksum, rising counters and a serial number that is plainly fake, the way a DSMR 4
or DSMR 5 meter does. It needs Python 3.9 or newer and nothing else.

## Set up your software before you build anything

```bash
python3 tools/p1sim.py --tcp
```

This serves telegrams on port 8088, like a Piepr does. Point Home Assistant, Domoticz
or DSMR-reader at this computer's address instead of `piepr.local` and check that the
values come in. Once your Piepr is built you only change the host.

`--version 4` sends a telegram every 10 seconds, as a DSMR 4 meter does; the default
is DSMR 5, one a second. `--interval` and `--count` set the pace and the number, and
`--stdout` just prints them.

## Test a built Piepr on your desk

```bash
python3 tools/p1sim.py --serial /dev/ttyUSB0
```

This writes the telegrams to a USB serial adapter (`pip install pyserial` first). The
adapter's TX can't go straight to pad 4: the P1 data line is inverted, and a meter
only ever pulls it low. Three parts make the adapter behave like the meter:

| From | Through | To |
|---|---|---|
| adapter TX | 10 kΩ resistor | base of a small NPN transistor (BC547, 2N2222) |
| transistor emitter | | `G` on the board, and the adapter's GND |
| transistor collector | | pad `4` on the board |

The 2.2 kΩ resistor already on the board pulls the line up to 3.3 V. Power the board
over USB as usual; the wires for P1 pins 1 and 2 are not needed for this. The light
flickers at every telegram, and `nc piepr.local 8088` shows them.

## Tests

```bash
python3 -m unittest discover -s tools
```
