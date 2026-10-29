import unittest

from serial_port import PortSelectionError, find_unique_ch340, open_serial


class Port:
    def __init__(self, device, vid, pid):
        self.device = device
        self.vid = vid
        self.pid = pid


class FakeSerial:
    def __init__(self):
        self.events = []
        self._dtr = None
        self._rts = None
        self.is_open = False

    @property
    def dtr(self):
        return self._dtr

    @dtr.setter
    def dtr(self, value):
        self._dtr = value
        self.events.append(("dtr", value))

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self._rts = value
        self.events.append(("rts", value))

    def open(self):
        self.events.append(("open", self.dtr, self.rts, self.xonxoff,
                            self.rtscts, self.dsrdtr, self.port))
        self.is_open = True

    def close(self):
        self.is_open = False


class SerialPortTests(unittest.TestCase):
    def test_selects_only_the_unique_ch340(self):
        port = find_unique_ch340([
            Port("COM4", 0x1234, 0x0001),
            Port("COM9", 0x1A86, 0x7523),
        ])
        self.assertEqual(port, "COM9")

    def test_missing_or_ambiguous_ch340_is_rejected(self):
        for ports in (
            [Port("COM4", 0x1234, 0x0001)],
            [Port("COM3", 0x1A86, 0x7523), Port("COM9", 0x1A86, 0x7523)],
        ):
            with self.subTest(ports=len(ports)):
                with self.assertRaises(PortSelectionError):
                    find_unique_ch340(ports)

    def test_invalid_identity_fails_before_serial_is_constructed(self):
        for ports in (
            [],
            [Port("COM3", 0x1A86, 0x7523), Port("COM9", 0x1A86, 0x7523)],
        ):
            constructions = []

            def factory():
                constructions.append(True)
                return FakeSerial()

            with self.subTest(matches=len(ports)):
                with self.assertRaises(PortSelectionError):
                    open_serial(port_enumerator=lambda ports=ports: ports,
                                serial_factory=factory)
                self.assertEqual(constructions, [])

    def test_control_lines_and_flow_control_are_safe_before_open(self):
        fake = FakeSerial()
        opened = open_serial(
            port_enumerator=lambda: [Port("COM9", 0x1A86, 0x7523)],
            serial_factory=lambda: fake,
        )

        self.assertIs(opened, fake)
        self.assertEqual(fake.events[-1], ("open", False, False, False,
                                           False, False, "COM9"))
        self.assertEqual(fake.baudrate, 115200)


if __name__ == "__main__":
    unittest.main()
