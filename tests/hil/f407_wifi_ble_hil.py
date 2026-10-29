"""Safe USART1 feedback capture and Wi-Fi/BLE takeover result evaluation.

This tool never programs or resets the target. In a future authorized board
run, keep one serial session open, capture the baseline, perform the separately
authorized OpenOCD operation, then compare the post-program status.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time
from typing import Callable, Optional

from serial_port import SerialOpenError, open_serial
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
    snapshot_to_dict,
)


STATUS_COMMANDS = b"WIFI STATUS\r\nBT STATUS\r\n"
DEFAULT_TIMEOUT_SECONDS = 1.5
STATUS_QUERY_RETRY_INTERVAL_SECONDS = 0.25
BASELINE_SYNC_TIMEOUT_SECONDS = 30.0
POST_RESET_TIMEOUT_MS = 30000
POLL_INTERVAL_SECONDS = 0.25
REPO_ROOT = Path(__file__).resolve().parents[2]
LOG_DIRECTORY = REPO_ROOT / ".ai" / "logs"


class HILCaptureError(RuntimeError):
    """A sanitized error that does not include serial payload data."""


class SerialStatusClient:
    def __init__(self, serial_object: object, *, monotonic: Callable[[], float] = time.monotonic):
        self._serial = serial_object
        self._monotonic = monotonic
        self._parser = SanitizedStatusParser()
        self.boot_marker_seen = False
        self._status_queries_written = 0
        self._rx_bytes = 0
        self._wifi_status_lines = 0
        self._bluetooth_status_lines = 0
        self._wifi_sync_diagnostic_lines = 0
        self._wifi_tcp_failure_diagnostic_lines = 0

    def query_snapshot(self, timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS) -> StatusSnapshot:
        deadline = self._monotonic() + max(timeout_seconds, 0.0)
        next_query_at = self._monotonic()
        wifi: Optional[WifiStatus] = None
        bluetooth: Optional[BluetoothStatus] = None
        wifi_sync_diagnostic: Optional[WifiSyncDiagnostic] = None
        wifi_tcp_failure_diagnostic: Optional[WifiTcpFailureDiagnostic] = None
        while self._monotonic() < deadline:
            now = self._monotonic()
            if now >= next_query_at:
                try:
                    written = self._serial.write(STATUS_COMMANDS)
                except Exception as exc:
                    raise HILCaptureError("Status query could not be sent.") from exc
                if written is not None and written != len(STATUS_COMMANDS):
                    raise HILCaptureError("Status query was incomplete.")
                self._status_queries_written += 1
                next_query_at = now + STATUS_QUERY_RETRY_INTERVAL_SECONDS

            try:
                chunk = self._serial.read(128)
            except Exception as exc:
                raise HILCaptureError("Serial feedback could not be read.") from exc
            if not chunk:
                continue
            self._rx_bytes += len(chunk)
            for status in self._parser.feed(chunk):
                if isinstance(status, WifiStatus):
                    wifi = status
                    self._wifi_status_lines += 1
                elif isinstance(status, BluetoothStatus):
                    bluetooth = status
                    self._bluetooth_status_lines += 1
                elif isinstance(status, WifiSyncDiagnostic):
                    wifi_sync_diagnostic = status
                    self._wifi_sync_diagnostic_lines += 1
                elif isinstance(status, WifiTcpFailureDiagnostic):
                    wifi_tcp_failure_diagnostic = status
                    self._wifi_tcp_failure_diagnostic_lines += 1
                elif isinstance(status, BootMarker):
                    self.boot_marker_seen = True
            if wifi is not None and bluetooth is not None:
                break
        return StatusSnapshot(wifi=wifi, bluetooth=bluetooth,
                              wifi_sync_diagnostic=wifi_sync_diagnostic,
                              wifi_tcp_failure_diagnostic=wifi_tcp_failure_diagnostic)

    def transport_diagnostics(self) -> dict[str, object]:
        """Return fixed counters only; never expose serial payload bytes."""
        return {
            "status_queries_written": self._status_queries_written,
            "rx_bytes": self._rx_bytes,
            "wifi_status_lines": self._wifi_status_lines,
            "bluetooth_status_lines": self._bluetooth_status_lines,
            "wifi_sync_diagnostic_lines": self._wifi_sync_diagnostic_lines,
            "wifi_tcp_failure_diagnostic_lines": self._wifi_tcp_failure_diagnostic_lines,
            "discarded_lines": self._parser.discarded_lines,
            "overflow_lines": self._parser.overflow_lines,
            "startup_marker": "SEEN" if self.boot_marker_seen else "NOT_SEEN",
        }


def _merge_snapshots(previous: Optional[StatusSnapshot], current: StatusSnapshot) -> StatusSnapshot:
    return StatusSnapshot(
        wifi=current.wifi if current.wifi is not None else (previous.wifi if previous else None),
        bluetooth=(current.bluetooth if current.bluetooth is not None
                   else (previous.bluetooth if previous else None)),
        wifi_sync_diagnostic=(
            current.wifi_sync_diagnostic if current.wifi_sync_diagnostic is not None
            else (previous.wifi_sync_diagnostic
                  if previous is not None and current.wifi is None else None)
        ),
        wifi_tcp_failure_diagnostic=(
            current.wifi_tcp_failure_diagnostic
            if current.wifi_tcp_failure_diagnostic is not None
            else (previous.wifi_tcp_failure_diagnostic
                  if previous is not None and current.wifi is None else None)
        ),
    )


def _wifi_link_resolved(wifi: Optional[WifiStatus]) -> bool:
    if wifi is None:
        return False
    if wifi.state == "CONNECTED":
        return wifi.sta == "CONNECTED"
    if wifi.state in ("NO_CREDENTIALS", "UPSTREAM_LOST"):
        return wifi.sta == "DISCONNECTED"
    return False


def capture_baseline(
    client: SerialStatusClient,
    timeout_seconds: float = BASELINE_SYNC_TIMEOUT_SECONDS,
    *,
    per_snapshot_timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    poll_interval_seconds: float = POLL_INTERVAL_SECONDS,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> StatusSnapshot:
    """Wait boundedly for a known STA association result before taking baseline."""
    deadline = monotonic() + max(timeout_seconds, 0.0)
    latest: Optional[StatusSnapshot] = None

    while monotonic() < deadline:
        remaining = deadline - monotonic()
        current = client.query_snapshot(
            timeout_seconds=min(max(per_snapshot_timeout_seconds, 0.0), remaining)
        )
        latest = _merge_snapshots(latest, current)
        if _wifi_link_resolved(latest.wifi):
            return latest

        remaining = deadline - monotonic()
        if remaining <= 0.0:
            break
        sleep(min(max(poll_interval_seconds, 0.01), remaining))

    return latest if latest is not None else StatusSnapshot()


def monitor_after_reset(
    client: SerialStatusClient,
    before: StatusSnapshot,
    *,
    program_started_at: float,
    success_marker_at: float,
    timeout_ms: int = POST_RESET_TIMEOUT_MS,
    poll_interval_seconds: float = POLL_INTERVAL_SECONDS,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> tuple[StatusSnapshot, dict]:
    """Poll status and require a conservative reset-time bound for PASS.

    The actual successful reset must occur after ``program_started_at`` and
    before ``success_marker_at``. Therefore elapsed time from program start is
    an upper bound, while elapsed time from the post-success marker is a lower
    bound. Only the upper bound can establish a 30-second PASS.
    """
    if program_started_at > success_marker_at or success_marker_at > monotonic():
        raise HILCaptureError("HIL timing markers were invalid.")

    latest: Optional[StatusSnapshot] = None
    tcp_disconnected_seen = False
    wifi_result = "NOT PROVEN"
    bt_result = evaluate_bluetooth_preserved(before.bluetooth, None)
    bt_required = before.bluetooth is not None and before.bluetooth.state == "CONNECTED"

    while True:
        now = monotonic()
        lower_bound_elapsed_ms = max(0, int((now - success_marker_at) * 1000))
        remaining_ms = timeout_ms - lower_bound_elapsed_ms
        current = client.query_snapshot(timeout_seconds=min(0.4, max(remaining_ms / 1000.0, 0.0)))
        if (before.wifi is not None and before.wifi.tcp == "CONNECTED" and
                current.wifi is not None and current.wifi.tcp == "DISCONNECTED"):
            tcp_disconnected_seen = True
        latest = _merge_snapshots(latest, current)
        now = monotonic()
        lower_bound_elapsed_ms = max(0, int((now - success_marker_at) * 1000))
        upper_bound_elapsed_ms = max(0, int((now - program_started_at) * 1000))
        observed_wifi_result = evaluate_wifi_restored(
            before.wifi, latest.wifi, elapsed_ms=upper_bound_elapsed_ms,
            timeout_ms=timeout_ms, timing_is_upper_bound=True,
            lower_bound_elapsed_ms=lower_bound_elapsed_ms,
            tcp_disconnected_seen=tcp_disconnected_seen,
        )
        if tcp_disconnected_seen:
            wifi_result = "FAIL"
        elif observed_wifi_result == "FAIL" and wifi_result != "PASS":
            wifi_result = "FAIL"
        elif observed_wifi_result == "PASS" and wifi_result != "FAIL":
            wifi_result = observed_wifi_result

        observed_bt_result = evaluate_bluetooth_preserved(before.bluetooth, latest.bluetooth)
        if observed_bt_result == "FAIL" or (
                observed_bt_result == "PASS" and bt_result != "FAIL"):
            bt_result = observed_bt_result

        if wifi_result != "NOT PROVEN" and (not bt_required or bt_result != "NOT PROVEN"):
            break

        if lower_bound_elapsed_ms >= timeout_ms:
            if wifi_result == "NOT PROVEN":
                wifi_result = evaluate_wifi_restored(
                    before.wifi, latest.wifi, elapsed_ms=upper_bound_elapsed_ms,
                    timeout_ms=timeout_ms, timing_is_upper_bound=True,
                    lower_bound_elapsed_ms=lower_bound_elapsed_ms,
                    deadline_elapsed=True,
                    tcp_disconnected_seen=tcp_disconnected_seen,
                )
            break
        sleep(min(poll_interval_seconds, (timeout_ms - lower_bound_elapsed_ms) / 1000.0))

    latest = latest or StatusSnapshot()
    return latest, {
        "WIFI_RESTORED": wifi_result,
        "BT_PRESERVED": bt_result,
        "QIMING_AIR_VISIBILITY": "NOT PROVEN",
    }


def write_sanitized_report(
    phase: str,
    snapshot: StatusSnapshot,
    results: dict,
    *,
    before: Optional[StatusSnapshot] = None,
    transport: Optional[dict[str, object]] = None,
) -> Path:
    safe_transport = _validate_transport_diagnostics(transport)
    LOG_DIRECTORY.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    path = LOG_DIRECTORY / f"f407-wifi-ble-{phase}-{stamp}.json"
    document = {
        "schema": "f407-wifi-ble-hil-v2",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "phase": phase,
        "snapshot": snapshot_to_dict(snapshot),
        "before": snapshot_to_dict(before) if before is not None else None,
        "serial_transport": safe_transport,
        "results": results,
    }
    try:
        with path.open("x", encoding="utf-8", newline="\n") as report_file:
            json.dump(document, report_file, ensure_ascii=True, separators=(",", ":"))
            report_file.write("\n")
    except OSError as exc:
        raise HILCaptureError("Sanitized HIL report could not be saved.") from exc
    return path


def _validate_transport_diagnostics(
    transport: Optional[dict[str, object]],
) -> Optional[dict[str, object]]:
    if transport is None:
        return None
    counter_names = (
        "status_queries_written", "rx_bytes", "wifi_status_lines",
        "bluetooth_status_lines", "wifi_sync_diagnostic_lines",
        "wifi_tcp_failure_diagnostic_lines", "discarded_lines", "overflow_lines",
    )
    if set(transport) != {*counter_names, "startup_marker"}:
        raise HILCaptureError("Transport diagnostic fields were not allow-listed.")
    safe: dict[str, object] = {}
    for name in counter_names:
        value = transport[name]
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise HILCaptureError("Transport diagnostic counter was invalid.")
        safe[name] = value
    marker = transport["startup_marker"]
    if marker not in ("SEEN", "NOT_SEEN"):
        raise HILCaptureError("Transport startup marker was invalid.")
    safe["startup_marker"] = marker
    return safe


def _print_report(
    path: Path,
    snapshot: StatusSnapshot,
    results: dict,
    port_name: str,
    transport: Optional[dict[str, object]] = None,
) -> None:
    print(f"CH340_PORT={port_name}")
    if transport is not None:
        safe_transport = _validate_transport_diagnostics(transport)
        for name, value in safe_transport.items():
            print(f"SERIAL_{name.upper()}={value}")
    if snapshot.wifi is None:
        print("WIFI=NOT OBSERVED")
    else:
        wifi = snapshot.wifi
        print(
            "WIFI="
            f"{wifi.state}; CREDENTIALS={wifi.credentials}; RETRY_IN={wifi.retry_in}; "
            f"MODE={wifi.mode}; APSTA={wifi.apsta}; STA={wifi.sta}; IP={wifi.ip}; "
            f"TCP={wifi.tcp}; NET={wifi.net}"
        )
    print(f"BT={snapshot.bluetooth.state if snapshot.bluetooth else 'NOT OBSERVED'}")
    if snapshot.wifi_sync_diagnostic is not None:
        diagnostic = snapshot.wifi_sync_diagnostic
        print(
            "WIFI_SYNC_DIAG="
            f"DIALECT={diagnostic.dialect}; FLAGS=0x{diagnostic.flags:02X}; "
            f"LENGTH_BUCKET={diagnostic.length_bucket}; RESULT=UNKNOWN"
        )
    if snapshot.wifi_tcp_failure_diagnostic is not None:
        print(f"WIFI_TCP_FAILURE_DETAIL={snapshot.wifi_tcp_failure_diagnostic.detail}")
    for name, result in results.items():
        print(f"{name}={result}")
    print(f"SANITIZED_REPORT={path}")


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hold-for-program", action="store_true",
                        help="keep this one serial session open while an external authorized program/reset runs")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS,
                        help="per-snapshot feedback timeout in seconds")
    parser.add_argument("--sync-timeout", type=float, default=BASELINE_SYNC_TIMEOUT_SECONDS,
                        help="maximum seconds to resolve the initial STA link before reporting the baseline")
    args = parser.parse_args(argv)

    try:
        serial_object = open_serial()
    except SerialOpenError:
        print("HIL_CAPTURE=FAIL; CH340 identity/open gate did not pass.")
        return 2

    try:
        client = SerialStatusClient(serial_object)
        before = capture_baseline(
            client,
            timeout_seconds=args.sync_timeout,
            per_snapshot_timeout_seconds=args.timeout,
        )
        results = {
            "BASELINE_WIFI_SYNC": "PASS" if _wifi_link_resolved(before.wifi) else "NOT PROVEN",
            "WIFI_RESTORED": "NOT PROVEN",
            "BT_PRESERVED": "NOT PROVEN",
            "QIMING_AIR_VISIBILITY": "NOT PROVEN",
            "FLASH_LOAD": "NOT PROVEN",
        }
        transport = client.transport_diagnostics()
        baseline_path = write_sanitized_report("baseline", before, results,
                                               transport=transport)
        _print_report(baseline_path, before, results,
                      str(getattr(serial_object, "port", "CH340")), transport)

        if not args.hold_for_program:
            return 0
        if (before.wifi is None or before.wifi.state != "CONNECTED" or
                before.wifi.sta != "CONNECTED" or before.wifi.ip != "READY"):
            print("PROGRAM_GATE=NOT PROVEN; baseline STA/IP preconditions are missing.")
            return 3

        print("HIL_READY_FOR_PROGRAM=1; serial remains open; this receiver will not program or reset the target.")
        print("Immediately before starting a separately authorized OpenOCD operation, enter PROGRAM_STARTED.")
        try:
            start_marker = input().strip()
        except (EOFError, KeyboardInterrupt):
            start_marker = ""
        if start_marker != "PROGRAM_STARTED":
            stopped_results = dict(results)
            stopped_results["FLASH_LOAD"] = "NOT PROVEN"
            stopped_path = write_sanitized_report("program-not-proven", before,
                                                  stopped_results, before=before)
            print("FLASH_LOAD=NOT PROVEN; start marker missing; no reset recovery was claimed.")
            print(f"SANITIZED_REPORT={stopped_path}")
            return 4
        program_started_at = time.monotonic()

        print("After write, verify, and successful F407 reset are reported, enter FLASH_VERIFIED.")
        print("If programming fails, enter FLASH_FAILED. Other input stops without a recovery claim.")
        try:
            program_result = input().strip()
        except (EOFError, KeyboardInterrupt):
            program_result = ""

        if program_result == "FLASH_FAILED":
            failed_results = dict(results)
            failed_results["FLASH_LOAD"] = "FAIL"
            failed_path = write_sanitized_report("program-failed", before,
                                                 failed_results, before=before)
            print("FLASH_LOAD=FAIL; post-reset recovery was not sampled.")
            print(f"SANITIZED_REPORT={failed_path}")
            return 5
        if program_result != "FLASH_VERIFIED":
            stopped_results = dict(results)
            stopped_results["FLASH_LOAD"] = "NOT PROVEN"
            stopped_path = write_sanitized_report("program-not-proven", before,
                                                  stopped_results, before=before)
            print("FLASH_LOAD=NOT PROVEN; success marker missing; recovery was not sampled.")
            print(f"SANITIZED_REPORT={stopped_path}")
            return 4

        success_marker_at = time.monotonic()
        after, results = monitor_after_reset(
            client, before, program_started_at=program_started_at,
            success_marker_at=success_marker_at,
        )
        results["FLASH_LOAD"] = "PASS"
        phase = "post-reset"
        report_path = write_sanitized_report(phase, after, results, before=before)
        _print_report(report_path, after, results, str(getattr(serial_object, "port", "CH340")))
        return 0
    except HILCaptureError:
        print("HIL_CAPTURE=FAIL; sanitized status feedback was incomplete.")
        return 1
    finally:
        try:
            serial_object.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
