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

## Tests

```bash
python3 -m unittest discover -s tools
```
