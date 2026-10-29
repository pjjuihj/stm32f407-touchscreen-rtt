import unittest

from takeover_protocol import (
    BluetoothStatus,
    BootMarker,
    SanitizedStatusParser,
    StatusSnapshot,
    WifiSyncDiagnostic,
    WifiTcpFailureDiagnostic,
    WifiStatus,
    evaluate_bluetooth_preserved,
    evaluate_wifi_restored,
    parse_bluetooth_status,
    parse_wifi_status,
    parse_wifi_sync_diagnostic,
    parse_wifi_tcp_failure_diagnostic,
    snapshot_to_dict,
)
from f407_wifi_ble_hil import (
    HILCaptureError,
    STATUS_COMMANDS,
    SerialStatusClient,
    _validate_transport_diagnostics,
    capture_baseline,
    monitor_after_reset,
)


CONNECTED_WIFI = (
    "WIFI STATUS: CONNECTED; CREDENTIALS=loaded; RETRY_IN=0; MODE=APSTA; "
    "APSTA=YES; STA=CONNECTED; IP=READY; TCP=CONNECTED; NET=PASS"
)
POST_RESET_WIFI = (
    "WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; MODE=APSTA; "
    "APSTA=YES; STA=CONNECTED; IP=READY; TCP=CONNECTED; NET=NOT_RUN"
)
CURRENT_WIFI_STATUS_WITH_LINK_INFO = (
    "WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; MODE=APSTA; "
    "APSTA=YES; STA=CONNECTED; IP=READY; TCP=DISCONNECTED; "
    "AP_BSSID=AA:BB:CC:DD:EE:FF; AP_CHANNEL=11; AP_RSSI=-54; NET=NOT_RUN"
)
CURRENT_WIFI_STATUS_WITHOUT_LINK_INFO = (
    "WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; MODE=APSTA; "
    "APSTA=YES; STA=CONNECTED; IP=READY; TCP=DISCONNECTED; "
    "AP_BSSID=UNKNOWN; AP_CHANNEL=UNKNOWN; AP_RSSI=UNKNOWN; NET=NOT_RUN"
)


class TakeoverProtocolTests(unittest.TestCase):
    def test_boot_marker_is_reduced_to_a_fixed_event_without_retaining_line(self):
        parser = SanitizedStatusParser()
        events = parser.feed(
            b"[SYSTEM] System starting... private-ssid private-password\r\n"
        )
        self.assertEqual(events, (BootMarker(),))
        self.assertNotIn("private-ssid", repr(events))
        self.assertNotIn("private-password", repr(parser.__dict__))

    def test_parses_only_allowlisted_wifi_fields(self):
        status = parse_wifi_status(POST_RESET_WIFI)
        self.assertEqual(status.state, "CONNECTED")
        self.assertEqual(status.credentials, "none")
        self.assertEqual(status.sta, "CONNECTED")
        self.assertEqual(status.ip, "READY")
        self.assertEqual(status.tcp, "CONNECTED")
        self.assertFalse(hasattr(status, "ssid"))

    def test_parses_current_wifi_status_and_drops_ap_link_identifiers(self):
        expected = WifiStatus(
            state="CONNECTED",
            credentials="none",
            retry_in=0,
            mode="APSTA",
            apsta="YES",
            sta="CONNECTED",
            ip="READY",
            tcp="DISCONNECTED",
            net="NOT_RUN",
        )
        for line in (
            CURRENT_WIFI_STATUS_WITH_LINK_INFO,
            CURRENT_WIFI_STATUS_WITHOUT_LINK_INFO,
        ):
            with self.subTest(line=line):
                status = parse_wifi_status(line)
                self.assertEqual(status, expected)
                self.assertNotIn("AA:BB:CC:DD:EE:FF", repr(status))
                self.assertFalse(hasattr(status, "bssid"))

    def test_rejects_status_lines_with_unapproved_or_unknown_fields(self):
        self.assertIsNone(parse_wifi_status(POST_RESET_WIFI + "; SSID=private"))
        self.assertIsNone(parse_wifi_status(POST_RESET_WIFI.replace("IP=READY", "IP=10.0.0.2")))
        self.assertIsNone(parse_wifi_status("WIFI STATUS: CONNECTED; SSID=private"))

    def test_parses_bluetooth_link_state_only(self):
        self.assertEqual(parse_bluetooth_status("BT STATUS: CONNECTED"),
                         BluetoothStatus("CONNECTED"))
        self.assertEqual(parse_bluetooth_status("BT STATUS: DISCONNECTED"),
                         BluetoothStatus("DISCONNECTED"))
        self.assertIsNone(parse_bluetooth_status("BT STATUS: CONNECTED; MAC=secret"))

    def test_parses_only_numeric_sanitized_station_sync_diagnostics(self):
        diagnostic = parse_wifi_sync_diagnostic(
            "WIFI SYNC DIAG: D=2 F=42 L=2 R=U")
        self.assertEqual(diagnostic, WifiSyncDiagnostic("NONOS", 0x42, 2))
        self.assertIsNone(parse_wifi_sync_diagnostic(
            'WIFI SYNC DIAG: D=2 F=42 L=2 SSID=private'))

        parser = SanitizedStatusParser()
        events = parser.feed(b"WIFI SYNC DIAG: D=2 F=42 L=2 R=U\r\n")
        self.assertEqual(events, (diagnostic,))

    def test_tcp_failure_diagnostic_keeps_only_fixed_safe_details(self):
        self.assertEqual(
            parse_wifi_tcp_failure_diagnostic("WIFI TCP DETAIL: connect FAIL"),
            WifiTcpFailureDiagnostic("CONNECT_FAIL"),
        )
        self.assertEqual(
            parse_wifi_tcp_failure_diagnostic("WIFI TCP DETAIL: ERR CODE:0x010a0000"),
            WifiTcpFailureDiagnostic("ERR_CODE_0x010A0000"),
        )
        self.assertIsNone(parse_wifi_tcp_failure_diagnostic(
            "WIFI TCP DETAIL: password=private ERROR"
        ))
        self.assertIsNone(parse_wifi_tcp_failure_diagnostic(
            "WIFI TCP DETAIL: " + "x" * 96
        ))

    def test_reassembles_fragmented_feedback_and_drops_raw_lines(self):
        parser = SanitizedStatusParser()
        self.assertEqual(parser.feed(b'WIFI STA'), ())
        wifi_events = parser.feed(b'TUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; MODE=APSTA; APSTA=YES; STA=CONNECTED; IP=READY; TCP=UNKNOWN; NET=NOT_RUN\r')
        self.assertEqual(len(wifi_events), 1)
        self.assertEqual(wifi_events[0].state, "CONNECTED")
        statuses = parser.feed(b'\nBT STATUS: CONNECTED\r\n+CWJAP:"secret-ssid",secret-data\r\n')
        self.assertEqual(statuses, (BluetoothStatus("CONNECTED"),))
        self.assertNotIn("secret-ssid", repr(statuses))

    def test_overlong_line_is_discarded_without_poisoning_next_status(self):
        parser = SanitizedStatusParser(max_line_bytes=32)
        self.assertEqual(parser.feed(b"X" * 80 + b"\rBT STATUS: CONNECTED\r"),
                         (BluetoothStatus("CONNECTED"),))

    def test_wifi_recovery_requires_connected_sta_ip_and_empty_ram_credentials(self):
        before = parse_wifi_status(CONNECTED_WIFI)
        after = parse_wifi_status(POST_RESET_WIFI)
        self.assertEqual(evaluate_wifi_restored(before, after, elapsed_ms=2500), "PASS")
        self.assertEqual(evaluate_wifi_restored(before, after, elapsed_ms=30001), "FAIL")
        self.assertEqual(evaluate_wifi_restored(
            before, after, elapsed_ms=30001, timing_is_upper_bound=True), "NOT PROVEN")

    def test_two_marker_timing_never_turns_an_uncertain_window_into_pass(self):
        before = parse_wifi_status(CONNECTED_WIFI)
        after = parse_wifi_status(POST_RESET_WIFI)
        self.assertEqual(evaluate_wifi_restored(
            before, after, elapsed_ms=31000, timing_is_upper_bound=True,
            lower_bound_elapsed_ms=1000), "NOT PROVEN")
        self.assertEqual(evaluate_wifi_restored(
            before, after, elapsed_ms=61000, timing_is_upper_bound=True,
            lower_bound_elapsed_ms=31001), "FAIL")

    def test_monitor_uses_program_start_as_upper_bound(self):
        class FakeClock:
            now = 31.0

            def monotonic(self):
                return self.now

            def sleep(self, duration):
                self.now += duration

        class FakeClient:
            def query_snapshot(self, timeout_seconds):
                return StatusSnapshot(
                    wifi=parse_wifi_status(POST_RESET_WIFI),
                    bluetooth=BluetoothStatus("CONNECTED"),
                )

        clock = FakeClock()
        before = StatusSnapshot(
            wifi=parse_wifi_status(CONNECTED_WIFI),
            bluetooth=BluetoothStatus("CONNECTED"),
        )
        _, results = monitor_after_reset(
            FakeClient(), before, program_started_at=0.0, success_marker_at=30.5,
            monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertEqual(results["WIFI_RESTORED"], "NOT PROVEN")
        self.assertEqual(results["BT_PRESERVED"], "PASS")

    def test_baseline_waits_past_unknown_until_sta_link_is_resolved(self):
        class FakeClock:
            now = 0.0

            def monotonic(self):
                return self.now

            def sleep(self, duration):
                self.now += duration

        class RetryingClient:
            def __init__(self):
                self.calls = 0

            def query_snapshot(self, timeout_seconds):
                self.calls += 1
                if self.calls == 1:
                    wifi = parse_wifi_status(
                        POST_RESET_WIFI.replace("CONNECTED; CREDENTIALS=none", "UNKNOWN; CREDENTIALS=none")
                        .replace("STA=CONNECTED", "STA=UNKNOWN")
                        .replace("IP=READY", "IP=UNKNOWN")
                    )
                else:
                    wifi = parse_wifi_status(POST_RESET_WIFI)
                return StatusSnapshot(wifi=wifi, bluetooth=BluetoothStatus("CONNECTED"))

        clock = FakeClock()
        client = RetryingClient()
        baseline = capture_baseline(
            client, timeout_seconds=2.0, monotonic=clock.monotonic, sleep=clock.sleep
        )
        self.assertEqual(client.calls, 2)
        self.assertEqual(baseline.wifi.state, "CONNECTED")
        self.assertEqual(baseline.wifi.sta, "CONNECTED")

    def test_resolved_status_clears_older_sync_diagnostic(self):
        class FakeClock:
            now = 0.0

            def monotonic(self):
                return self.now

            def sleep(self, duration):
                self.now += duration

        class FakeClient:
            def __init__(self):
                self.snapshots = [
                    StatusSnapshot(wifi_sync_diagnostic=WifiSyncDiagnostic("NONOS", 0x42, 2)),
                    StatusSnapshot(wifi=parse_wifi_status(POST_RESET_WIFI),
                                   bluetooth=BluetoothStatus("CONNECTED")),
                ]

            def query_snapshot(self, timeout_seconds):
                return self.snapshots.pop(0)

        clock = FakeClock()
        snapshot = capture_baseline(
            FakeClient(), timeout_seconds=1.0, per_snapshot_timeout_seconds=0.1,
            poll_interval_seconds=0.01, monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertEqual(snapshot.wifi.state, "CONNECTED")
        self.assertIsNone(snapshot.wifi_sync_diagnostic)

    def test_baseline_preserves_unknown_and_stops_at_timeout(self):
        class FakeClock:
            now = 0.0

            def monotonic(self):
                return self.now

            def sleep(self, duration):
                self.now += duration

        class UnknownClient:
            calls = 0

            def query_snapshot(self, timeout_seconds):
                self.calls += 1
                return StatusSnapshot(
                    wifi=parse_wifi_status(
                        POST_RESET_WIFI.replace("CONNECTED; CREDENTIALS=none", "UNKNOWN; CREDENTIALS=none")
                        .replace("STA=CONNECTED", "STA=UNKNOWN")
                        .replace("IP=READY", "IP=UNKNOWN")
                    ),
                    bluetooth=BluetoothStatus("DISCONNECTED"),
                )

        clock = FakeClock()
        client = UnknownClient()
        baseline = capture_baseline(
            client,
            timeout_seconds=1.0,
            poll_interval_seconds=0.25,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
        )
        self.assertGreater(client.calls, 1)
        self.assertEqual(clock.now, 1.0)
        self.assertEqual(baseline.wifi.state, "UNKNOWN")
        self.assertEqual(baseline.wifi.sta, "UNKNOWN")

    def test_monitor_fails_when_success_is_only_observed_after_full_reset_deadline(self):
        class FakeClock:
            now = 0.0

            def monotonic(self):
                return self.now

        class DelayedClient:
            def query_snapshot(self, timeout_seconds):
                clock.now = 31.0
                return StatusSnapshot(
                    wifi=parse_wifi_status(POST_RESET_WIFI),
                    bluetooth=BluetoothStatus("CONNECTED"),
                )

        clock = FakeClock()
        before = StatusSnapshot(
            wifi=parse_wifi_status(CONNECTED_WIFI),
            bluetooth=BluetoothStatus("CONNECTED"),
        )
        _, results = monitor_after_reset(
            DelayedClient(), before, program_started_at=0.0, success_marker_at=0.0,
            monotonic=clock.monotonic, sleep=lambda _: None,
        )
        self.assertEqual(results["WIFI_RESTORED"], "FAIL")

    def test_wifi_pass_is_retained_while_waiting_for_missing_bluetooth_feedback(self):
        class FakeClock:
            now = 29.5

            def monotonic(self):
                return self.now

            def sleep(self, duration):
                self.now += duration

        class WifiOnlyClient:
            def query_snapshot(self, timeout_seconds):
                clock.now += 0.2
                return StatusSnapshot(wifi=parse_wifi_status(POST_RESET_WIFI))

        clock = FakeClock()
        before = StatusSnapshot(
            wifi=parse_wifi_status(CONNECTED_WIFI),
            bluetooth=BluetoothStatus("CONNECTED"),
        )
        _, results = monitor_after_reset(
            WifiOnlyClient(), before, program_started_at=0.0, success_marker_at=29.0,
            monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertEqual(results["WIFI_RESTORED"], "PASS")
        self.assertEqual(results["BT_PRESERVED"], "NOT PROVEN")

    def test_tcp_connected_before_reset_must_not_become_disconnected(self):
        before = parse_wifi_status(CONNECTED_WIFI)
        disconnected_tcp = parse_wifi_status(POST_RESET_WIFI.replace("TCP=CONNECTED", "TCP=DISCONNECTED"))
        unknown_tcp = parse_wifi_status(POST_RESET_WIFI.replace("TCP=CONNECTED", "TCP=UNKNOWN"))
        self.assertEqual(evaluate_wifi_restored(before, disconnected_tcp, elapsed_ms=1000), "FAIL")
        self.assertEqual(evaluate_wifi_restored(before, unknown_tcp, elapsed_ms=1000), "NOT PROVEN")
        self.assertEqual(evaluate_wifi_restored(
            before, parse_wifi_status(POST_RESET_WIFI), elapsed_ms=1000,
            tcp_disconnected_seen=True), "FAIL")

    def test_unknown_or_inadequate_baseline_is_not_guessed(self):
        unknown = parse_wifi_status(POST_RESET_WIFI.replace("STA=CONNECTED", "STA=UNKNOWN"))
        after = parse_wifi_status(POST_RESET_WIFI)
        self.assertEqual(evaluate_wifi_restored(unknown, after, elapsed_ms=1000), "NOT PROVEN")
        self.assertEqual(evaluate_wifi_restored(None, after, elapsed_ms=1000), "NOT PROVEN")

    def test_bluetooth_preservation_requires_connected_baseline(self):
        self.assertEqual(evaluate_bluetooth_preserved(
            BluetoothStatus("CONNECTED"), BluetoothStatus("CONNECTED")), "PASS")
        self.assertEqual(evaluate_bluetooth_preserved(
            BluetoothStatus("CONNECTED"), BluetoothStatus("DISCONNECTED")), "FAIL")
        self.assertEqual(evaluate_bluetooth_preserved(
            BluetoothStatus("DISCONNECTED"), BluetoothStatus("CONNECTED")), "NOT PROVEN")

    def test_serial_client_sends_only_status_queries_and_discards_other_lines(self):
        class FakeSerial:
            def __init__(self):
                self.written = []
                self.chunks = [
                    b'+CWJAP:"private-network",private-data\r\n'
                    b'WIFI SYNC DIAG: D=2 F=42 L=2 R=U\r\n'
                    b'WIFI TCP DETAIL: ERR CODE:0x010A0000\r\n'
                    b'WIFI STATUS: CONNECTED; ',
                    b'CREDENTIALS=none; RETRY_IN=0; MODE=APSTA; APSTA=YES; '
                    b'STA=CONNECTED; IP=READY; TCP=UNKNOWN; NET=NOT_RUN\r\n'
                    b'BT STATUS: CONNECTED\r\n',
                ]

            def write(self, payload):
                self.written.append(payload)
                return len(payload)

            def read(self, size):
                return self.chunks.pop(0) if self.chunks else b""

        fake = FakeSerial()
        client = SerialStatusClient(fake, monotonic=lambda: 1.0)
        snapshot = client.query_snapshot(timeout_seconds=1.0)
        self.assertEqual(fake.written, [STATUS_COMMANDS])
        self.assertEqual(snapshot.wifi.state, "CONNECTED")
        self.assertEqual(snapshot.bluetooth.state, "CONNECTED")
        self.assertEqual(snapshot.wifi_sync_diagnostic,
                         WifiSyncDiagnostic("NONOS", 0x42, 2))
        self.assertEqual(snapshot.wifi_tcp_failure_diagnostic,
                         WifiTcpFailureDiagnostic("ERR_CODE_0x010A0000"))
        sanitized = snapshot_to_dict(snapshot)
        self.assertEqual(set(sanitized), {
            "wifi", "bluetooth", "wifi_sync_diagnostic",
            "wifi_tcp_failure_diagnostic",
        })
        self.assertNotIn("private-network", repr(sanitized))
        self.assertNotIn("private-data", repr(sanitized))

    def test_serial_client_exposes_only_sanitized_rx_counters(self):
        class DiagnosticSerial:
            def __init__(self):
                self.pending = [
                    b'[SYSTEM] System starting... private-ssid private-password\r\n'
                    b'+CWJAP:"private-network",private-data\r\n'
                    b'WIFI SYNC DIAG: D=2 F=42 L=2 R=U\r\n'
                    b'WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; '
                    b'MODE=APSTA; APSTA=YES; STA=CONNECTED; IP=READY; '
                    b'TCP=UNKNOWN; NET=NOT_RUN\r\n'
                    b'BT STATUS: CONNECTED\r\n'
                ]

            def write(self, payload):
                return len(payload)

            def read(self, size):
                return self.pending.pop(0) if self.pending else b""

        client = SerialStatusClient(DiagnosticSerial(), monotonic=lambda: 1.0)
        client.query_snapshot(timeout_seconds=1.0)
        diagnostics = client.transport_diagnostics()

        self.assertEqual(diagnostics["rx_bytes"], len(
            b'[SYSTEM] System starting... private-ssid private-password\r\n'
            b'+CWJAP:"private-network",private-data\r\n'
            b'WIFI SYNC DIAG: D=2 F=42 L=2 R=U\r\n'
            b'WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; '
            b'MODE=APSTA; APSTA=YES; STA=CONNECTED; IP=READY; '
            b'TCP=UNKNOWN; NET=NOT_RUN\r\n'
            b'BT STATUS: CONNECTED\r\n'
        ))
        self.assertEqual(diagnostics["wifi_status_lines"], 1)
        self.assertEqual(diagnostics["bluetooth_status_lines"], 1)
        self.assertEqual(diagnostics["wifi_sync_diagnostic_lines"], 1)
        self.assertEqual(diagnostics["wifi_tcp_failure_diagnostic_lines"], 0)
        self.assertEqual(diagnostics["discarded_lines"], 1)
        self.assertEqual(diagnostics["overflow_lines"], 0)
        self.assertEqual(diagnostics["startup_marker"], "SEEN")
        self.assertNotIn("private-ssid", repr(diagnostics))
        self.assertNotIn("private-password", repr(diagnostics))
        self.assertNotIn("private-network", repr(client.__dict__))
        self.assertNotIn("private-data", repr(client.__dict__))
        self.assertEqual(_validate_transport_diagnostics(diagnostics), diagnostics)
        with self.assertRaises(HILCaptureError):
            _validate_transport_diagnostics({**diagnostics, "raw_line": "private-password"})

    def test_serial_client_latches_startup_marker_for_open_reset_gate(self):
        class MarkerSerial:
            def __init__(self):
                self.pending = [
                    b"[SYSTEM] System starting...\r\n",
                    b"WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; "
                    b"MODE=APSTA; APSTA=YES; STA=CONNECTED; IP=READY; "
                    b"TCP=UNKNOWN; NET=NOT_RUN\r\nBT STATUS: CONNECTED\r\n",
                ]

            def write(self, payload):
                return len(payload)

            def read(self, size):
                return self.pending.pop(0) if self.pending else b""

        client = SerialStatusClient(MarkerSerial(), monotonic=lambda: 1.0)
        snapshot = client.query_snapshot(timeout_seconds=1.0)
        self.assertTrue(client.boot_marker_seen)
        self.assertEqual(snapshot.wifi.state, "CONNECTED")

    def test_serial_client_retries_status_query_if_open_time_query_is_lost(self):
        class FakeClock:
            now = 0.0

            def monotonic(self):
                return self.now

        class RetrySerial:
            def __init__(self):
                self.written = []
                self.pending = []

            def write(self, payload):
                self.written.append(payload)
                if len(self.written) == 2:
                    self.pending.append(
                        b"WIFI STATUS: CONNECTED; CREDENTIALS=none; RETRY_IN=0; "
                        b"MODE=APSTA; APSTA=YES; STA=CONNECTED; IP=READY; "
                        b"TCP=UNKNOWN; NET=NOT_RUN\r\nBT STATUS: CONNECTED\r\n"
                    )
                return len(payload)

            def read(self, size):
                clock.now += 0.05
                return self.pending.pop(0) if self.pending else b""

        clock = FakeClock()
        fake = RetrySerial()
        snapshot = SerialStatusClient(fake, monotonic=clock.monotonic).query_snapshot(
            timeout_seconds=1.0
        )

        self.assertIsNotNone(snapshot.wifi)
        self.assertEqual(snapshot.wifi.state, "CONNECTED")
        self.assertEqual(snapshot.bluetooth, BluetoothStatus("CONNECTED"))
        self.assertGreaterEqual(len(fake.written), 2)
        self.assertTrue(all(payload == STATUS_COMMANDS for payload in fake.written))


if __name__ == "__main__":
    unittest.main()
