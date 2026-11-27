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
clang-format -i src/*.[ch] lib/*/*/*.[ch] test/*/*.c fuzz/*.c
```

Use clang-format 22.1.5 (`pip install clang-format==22.1.5`): other versions
lay the code out differently and the build check will complain.

A change to the parsing code in `lib/` comes with a test. The same checks run
on every pull request.
