#!/usr/bin/env python3
"""A pretend smart meter: writes valid P1 telegrams, for testing without a meter.

    p1sim.py --tcp      serve telegrams on port 8088, like a Piepr does
    p1sim.py --stdout   print them

See firmware/docs/simulator.md. Needs Python 3.9 or newer and nothing else.
"""
from __future__ import annotations

import argparse
import math
import os
import select
import socket
import sys
import time

DEFAULT_PORT = 8088
INTERVAL_S = {4: 10.0, 5: 1.0}  # how often a real meter sends
VERSION_CODE = {4: "42", 5: "50"}
EQUIPMENT_ID = "PIEPR-SIMULATOR".encode().hex().upper()  # clearly not a real meter
VOLTS = 230.0
SEND_TIMEOUT_S = 2.0


def crc16_arc(data: bytes) -> int:
    """CRC-16/ARC, as P1 telegrams use it over '/' up to and including '!'."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


# A household that is never quite idle: a base load plus two slow waves, each
# (kW, seconds). Between 0.03 and 0.87 kW.
BASE_KW = 0.45
WAVES = ((0.30, 700), (0.12, 53))


def _power_kw(t: float) -> float:
    return BASE_KW + sum(kw * math.sin(t / period) for kw, period in WAVES)


def _energy_kwh(t: float) -> float:
    """The power above, added up from 0 to t seconds (so the counters match it)."""
    kw_seconds = BASE_KW * t + sum(kw * period * (1 - math.cos(t / period)) for kw, period in WAVES)
    return kw_seconds / 3600


def _stamp(when: float) -> str:
    local = time.localtime(when)
    return time.strftime("%y%m%d%H%M%S", local) + ("S" if local.tm_isdst > 0 else "W")


def telegram(version: int, seq: int, when: float | None = None) -> bytes:
    """Telegram number `seq` (from 0) of a DSMR 4 or 5 meter, '/' up to "!XXXX\\r\\n"."""
    when = time.time() if when is None else when
    elapsed = seq * INTERVAL_S[version]
    power = _power_kw(elapsed)
    used = _energy_kwh(elapsed)
    gas_hour = int(when // 3600)  # gas is read on the hour: one value per timestamp

    lines = [
        f"/PPR{version}\\SIMULATOR",
        "",
        f"1-3:0.2.8({VERSION_CODE[version]})",
        f"0-0:1.0.0({_stamp(when)})",
        f"0-0:96.1.1({EQUIPMENT_ID})",
        "1-0:1.8.1(001234.567*kWh)",
        f"1-0:1.8.2({2345.678 + used:010.3f}*kWh)",  # tariff 2 is the active one
        "1-0:2.8.1(000000.000*kWh)",
        "1-0:2.8.2(000000.000*kWh)",
        "0-0:96.14.0(0002)",
        f"1-0:1.7.0({power:06.3f}*kW)",
        "1-0:2.7.0(00.000*kW)",
        "0-0:96.7.21(00004)",
        "0-0:96.7.9(00002)",
        "1-0:99.97.0(0)(0-0:96.7.19)",
        "1-0:32.32.0(00000)",
        "1-0:32.36.0(00000)",
        "0-0:96.13.0()",
    ]
    if version == 5:
        lines.append(f"1-0:32.7.0({VOLTS:05.1f}*V)")
    lines += [
        f"1-0:31.7.0({round(power * 1000 / VOLTS):03d}*A)",
        f"1-0:21.7.0({power:06.3f}*kW)",
        "1-0:22.7.0(00.000*kW)",
        "0-1:24.1.0(003)",
        f"0-1:96.1.0({EQUIPMENT_ID})",
        f"0-1:24.2.1({_stamp(gas_hour * 3600)})({0.042 * gas_hour:09.3f}*m3)",
    ]
    body = ("\r\n".join(lines) + "\r\n!").encode("ascii")
    return body + f"{crc16_arc(body):04X}\r\n".encode("ascii")


class TcpReaders:
    """Sends each telegram to everyone connected, as Piepr does."""

    def __init__(self, port: int):
        self.listener = socket.create_server(("", port))
        self.listener.setblocking(False)
        self.readers: list[socket.socket] = []

    def wait(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while True:
            left = max(0.0, deadline - time.monotonic())
            ready, _, _ = select.select([self.listener], [], [], left)
            if not ready:
                return
            try:
                reader, address = self.listener.accept()
            except OSError:  # gone again before we got to it
                continue
            # A reader that stops reading is dropped instead of holding up the rest.
            reader.settimeout(SEND_TIMEOUT_S)
            self.readers.append(reader)
            print(f"reader connected: {address[0]}", file=sys.stderr)

    def send(self, data: bytes) -> None:
        for reader in list(self.readers):
            try:
                reader.sendall(data)
            except OSError:
                self.readers.remove(reader)
                reader.close()
                print("reader gone", file=sys.stderr)


class Stream:
    """Standard output."""

    def __init__(self, out):
        self.out = out

    def wait(self, seconds: float) -> None:
        time.sleep(seconds)

    def send(self, data: bytes) -> None:
        self.out.write(data)
        self.out.flush()


def main() -> None:
    parser = argparse.ArgumentParser(description="Write P1 telegrams like a smart meter does.")
    parser.add_argument("--version", type=int, choices=(4, 5), default=5, help="DSMR version (default 5)")
    parser.add_argument("--interval", type=float, help="seconds between telegrams (default 10 for DSMR 4, 1 for 5)")
    parser.add_argument("--count", type=int, help="stop after this many telegrams")
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--stdout", action="store_true", help="print the telegrams")
    target.add_argument("--tcp", type=int, nargs="?", const=DEFAULT_PORT, metavar="PORT",
                        help=f"serve them over TCP (default port {DEFAULT_PORT})")
    args = parser.parse_args()

    interval = INTERVAL_S[args.version] if args.interval is None else args.interval
    if args.tcp is not None:
        sink = TcpReaders(args.tcp)
        print(f"serving DSMR {args.version} telegrams on port {args.tcp}", file=sys.stderr)
    else:
        sink = Stream(sys.stdout.buffer)

    seq = 0
    next_at = time.monotonic()
    try:
        while args.count is None or seq < args.count:
            sink.wait(max(0.0, next_at - time.monotonic()))
            sink.send(telegram(args.version, seq))
            seq += 1
            next_at += interval
    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        # The reader of our output left (| head): keep Python from complaining on exit.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
    except OSError as error:
        sys.exit(f"stopped: {error}")


if __name__ == "__main__":
    main()
