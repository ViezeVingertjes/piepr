"""Tests for p1sim.py. Run: python3 -m unittest discover -s tools"""
import pathlib
import re
import socket
import threading
import unittest

import p1sim

FIRMWARE = pathlib.Path(__file__).parent.parent / "firmware"
SEED = FIRMWARE / "fuzz/seeds/p1_framer/v5_telegram"


def firmware_constant(header: str, name: str) -> int:
    """The value of `#define name <number>` in one of the firmware's headers."""
    text = (FIRMWARE / header).read_text(encoding="utf-8")
    return int(re.search(rf"^#define\s+{name}\s+(\d+)", text, re.M).group(1))


def frame_ok(t: bytes) -> bool:
    """What Piepr's framer accepts: '/' ... '!' + CRC16 over that + CR LF."""
    m = re.fullmatch(rb"(/.*!)([0-9A-F]{4})\r\n", t, re.S)
    return bool(m) and p1sim.crc16_arc(m.group(1)) == int(m.group(2), 16)


def reading(t: bytes, obis: str, unit: bytes = b"kWh") -> float:
    """The value of line 1-0:<obis> in telegram `t`."""
    return float(re.search(rb"1-0:" + re.escape(obis.encode()) + rb"\((\d+\.\d+)\*" + unit + rb"\)", t).group(1))


class P1Sim(unittest.TestCase):
    def test_crc_check_value(self):
        self.assertEqual(p1sim.crc16_arc(b"123456789"), 0xBB3D)

    def test_matches_firmware_seed(self):
        self.assertTrue(frame_ok(SEED.read_bytes()))

    def test_telegram_frames_and_checks(self):
        for version in (4, 5):
            for seq in (0, 1, 500):
                self.assertTrue(frame_ok(p1sim.telegram(version, seq)))

    def test_version_line(self):
        self.assertIn(b"1-3:0.2.8(42)\r\n", p1sim.telegram(4, 0))
        self.assertIn(b"1-3:0.2.8(50)\r\n", p1sim.telegram(5, 0))

    def test_counter_of_the_active_tariff_rises(self):
        tariff = re.search(rb"0-0:96\.14\.0\((\d{4})\)", p1sim.telegram(5, 0)).group(1)
        self.assertEqual(tariff, b"0002")
        self.assertLess(reading(p1sim.telegram(5, 0), "1.8.2"), reading(p1sim.telegram(5, 3600), "1.8.2"))
        self.assertEqual(reading(p1sim.telegram(5, 0), "1.8.1"), reading(p1sim.telegram(5, 3600), "1.8.1"))

    def test_counter_is_the_power_added_up(self):
        # One hour of DSMR 5 telegrams: the kW values, one a second, summed to kWh.
        added_up = sum(reading(p1sim.telegram(5, seq), "1.7.0", b"kW") for seq in range(3600)) / 3600
        counted = reading(p1sim.telegram(5, 3600), "1.8.2") - reading(p1sim.telegram(5, 0), "1.8.2")
        self.assertAlmostEqual(counted, added_up, delta=0.005)

    def test_gas_reading_belongs_to_its_timestamp(self):
        def gas(when):
            return re.search(rb"0-1:24\.2\.1\((\d{12})[WS]\)\((\d+\.\d+)\*m3\)", p1sim.telegram(5, 0, when)).groups()

        hour = 3600 * 500000
        self.assertEqual(gas(hour + 10), gas(hour + 3590))
        self.assertNotEqual(gas(hour + 10)[0], gas(hour + 3610)[0])
        self.assertLess(float(gas(hour + 10)[1]), float(gas(hour + 3610)[1]))

    def test_a_reader_that_stops_reading_does_not_hold_up_the_others(self):
        server = p1sim.TcpReaders(0)
        port = server.listener.getsockname()[1]
        stuck = socket.create_connection(("localhost", port))
        alive = socket.create_connection(("localhost", port))
        self.addCleanup(lambda: [s.close() for s in (stuck, alive, server.listener, *server.readers)])
        server.wait(0.2)
        self.assertEqual(len(server.readers), 2)

        def drain():
            alive.settimeout(10)  # longer than the wait for the stuck one
            try:
                while alive.recv(65536):
                    pass
            except OSError:
                pass

        threading.Thread(target=drain, daemon=True).start()
        chunk = b"x" * 65536

        def flood():
            for _ in range(200):  # 13 MB: far more than the stuck reader's buffers hold
                server.send(chunk)

        sender = threading.Thread(target=flood, daemon=True)
        sender.start()
        sender.join(timeout=15)
        self.assertFalse(sender.is_alive(), "send() is still blocked by the reader that doesn't read")
        self.assertEqual(len(server.readers), 1)

    def test_fits_the_firmware_buffer(self):
        limit = firmware_constant("lib/p1core/include/p1_framer.h", "P1_MAX_TELEGRAM")
        self.assertLess(len(p1sim.telegram(5, 0)), limit)

    def test_serves_on_the_port_piepr_uses(self):
        self.assertEqual(p1sim.DEFAULT_PORT, firmware_constant("src/config.h", "TCP_PORT"))

    def test_no_slash_in_body(self):
        # Piepr's framer starts a new telegram at every '/'.
        self.assertEqual(p1sim.telegram(5, 0).count(b"/"), 1)


if __name__ == "__main__":
    unittest.main()
