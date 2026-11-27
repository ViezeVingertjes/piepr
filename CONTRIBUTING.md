# Contributing

Bug reports, meter reports and pull requests are welcome.

## Meter reports

Reports about the meter you use it on help a lot, also when everything just
works: open a
[meter report](https://github.com/ViezeVingertjes/piepr/issues/new?template=meter-report.yml).
Remove the serial numbers from anything you paste: every line with `96.1.0` or
`96.1.1` in it (such as `0-0:96.1.1`, `0-1:96.1.0` and `0-1:96.1.1`).

## Working on the firmware

Everything runs from `firmware/`; the [firmware guide](firmware/README.md) has
the layout and the conventions.

```bash
pio test -e native            # unit tests, with sanitizers
pio run                       # build (needs secrets.ini, see the guide)
fuzz/run.sh p1_framer 60      # one fuzzer for a minute (needs clang)
scripts/format.sh             # format the C code (--check only reports)
```

`format.sh` needs clang-format 22.1.5 (`pip install clang-format==22.1.5`): other
versions lay the code out differently.

Outside `firmware/`:

```bash
python3 -m unittest discover -s tools              # tests of the pretend meter
python3 -m unittest discover -s firmware/scripts   # tests of the page build step
tools/render_images.sh                             # after editing an image in a src/ folder (Linux)
```

The images are edited in `assets/src/` and `hardware/src/`; the script turns their
text into outlines, so they look the same on a machine without the Ubuntu font. An
image with text has a Dutch twin, `name.nl.svg`, for the Dutch pages: change both.

A change to the parsing code in `lib/` comes with a test. The same checks run
on every pull request.
