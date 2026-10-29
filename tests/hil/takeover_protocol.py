"""Allow-listed serial feedback parsing and Wi-Fi/BLE reset result rules."""

from __future__ import annotations

from dataclasses import asdict, dataclass
import re
from typing import Optional, Tuple, Union


@dataclass(frozen=True)
class WifiStatus:
    state: str
    credentials: str
    retry_in: int
    mode: str
    apsta: str
    sta: str
    ip: str
    tcp: str
    net: str


@dataclass(frozen=True)
class BluetoothStatus:
    state: str


@dataclass(frozen=True)
class WifiSyncDiagnostic:
    dialect: str
    flags: int
    length_bucket: int


@dataclass(frozen=True)
class WifiTcpFailureDiagnostic:
    detail: str


@dataclass(frozen=True)
class BootMarker:
    """A fixed, allow-listed indication that the MCU startup log was observed."""


@dataclass(frozen=True)
class StatusSnapshot:
    wifi: Optional[WifiStatus] = None
    bluetooth: Optional[BluetoothStatus] = None
    wifi_sync_diagnostic: Optional[WifiSyncDiagnostic] = None
    wifi_tcp_failure_diagnostic: Optional[WifiTcpFailureDiagnostic] = None


_WIFI_PATTERN = re.compile(
    r"^WIFI STATUS: "
    r"(?P<state>NO_CREDENTIALS|MODULE_SYNCING|UNKNOWN|CONNECTING|CONNECTED|RETRY_WAIT|UPSTREAM_LOST); "
    r"CREDENTIALS=(?P<credentials>loaded|none); "
    r"RETRY_IN=(?P<retry_in>[0-9]{1,5}); "
    r"MODE=(?P<mode>OFF|STA|AP|APSTA|UNKNOWN); "
    r"APSTA=(?P<apsta>YES|NO|UNKNOWN); "
    r"STA=(?P<sta>CONNECTED|DISCONNECTED|UNKNOWN); "
    r"IP=(?P<ip>READY|NOT_READY|UNKNOWN); "
    r"TCP=(?P<tcp>CONNECTED|DISCONNECTED|UNKNOWN); "
    r"(?:AP_BSSID=(?:UNKNOWN|[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}); "
    r"AP_CHANNEL=(?:UNKNOWN|[0-9]{1,3}); "
    r"AP_RSSI=(?:UNKNOWN|-?[0-9]{1,3}); )?"
    r"NET=(?P<net>NOT_RUN|RUNNING|PASS|FAIL|SKIPPED)$"
)
_BT_PATTERN = re.compile(r"^BT STATUS: (?P<state>CONNECTED|DISCONNECTED)$")
_WIFI_DIAGNOSTIC_PATTERN = re.compile(
    r"^WIFI SYNC DIAG: D=(?P<dialect>[012]) "
    r"F=(?P<flags>[0-9A-F]{2}) L=(?P<length_bucket>[0-6]) R=U$"
)
_WIFI_TCP_FAILURE_PATTERN = re.compile(r"^WIFI TCP DETAIL: (?P<detail>[^\r\n]{1,95})$")
_WIFI_TCP_FAILURE_ERROR_CODE_PATTERN = re.compile(
    r"^ERR CODE:0x(?P<code>[0-9A-Fa-f]{1,8})$"
)


def parse_wifi_status(line: str) -> Optional[WifiStatus]:
    match = _WIFI_PATTERN.fullmatch(line.strip())
    if match is None:
        return None
    fields = match.groupdict()
    return WifiStatus(
        state=fields["state"],
        credentials=fields["credentials"],
        retry_in=int(fields["retry_in"]),
        mode=fields["mode"],
        apsta=fields["apsta"],
        sta=fields["sta"],
        ip=fields["ip"],
        tcp=fields["tcp"],
        net=fields["net"],
    )


def parse_bluetooth_status(line: str) -> Optional[BluetoothStatus]:
    match = _BT_PATTERN.fullmatch(line.strip())
    if match is None:
        return None
    return BluetoothStatus(state=match.group("state"))


def parse_wifi_sync_diagnostic(line: str) -> Optional[WifiSyncDiagnostic]:
    match = _WIFI_DIAGNOSTIC_PATTERN.fullmatch(line.strip())
    if match is None:
        return None
    dialect = {"0": "UNKNOWN", "1": "ESP_AT", "2": "NONOS"}[
        match.group("dialect")
    ]
    return WifiSyncDiagnostic(
        dialect=dialect,
        flags=int(match.group("flags"), 16),
        length_bucket=int(match.group("length_bucket")),
    )


def parse_wifi_tcp_failure_diagnostic(
    line: str,
) -> Optional[WifiTcpFailureDiagnostic]:
    match = _WIFI_TCP_FAILURE_PATTERN.fullmatch(line.strip())
    if match is None:
        return None

    detail = match.group("detail")
    known_details = {
        "ERROR": "ERROR",
        "FAIL": "FAIL",
        "CONNECT FAIL": "CONNECT_FAIL",
        "ALREADY CONNECTED": "ALREADY_CONNECTED",
        "BUSY": "BUSY",
        "DNS FAIL": "DNS_FAIL",
    }
    normalized = detail.upper()
    if normalized in known_details:
        return WifiTcpFailureDiagnostic(detail=known_details[normalized])

    error_code = _WIFI_TCP_FAILURE_ERROR_CODE_PATTERN.fullmatch(detail)
    if error_code is not None:
        return WifiTcpFailureDiagnostic(
            detail=f"ERR_CODE_0x{error_code.group('code').upper()}"
        )
    return None


Status = Union[
    WifiStatus,
    BluetoothStatus,
    WifiSyncDiagnostic,
    WifiTcpFailureDiagnostic,
    BootMarker,
]


class SanitizedStatusParser:
    """Convert fragmented serial bytes to status objects; retain no raw lines."""

    def __init__(self, max_line_bytes: int = 256):
        if max_line_bytes < 1:
            raise ValueError("max_line_bytes must be positive")
        self._max_line_bytes = max_line_bytes
        self._line = bytearray()
        self._discarding = False
        self.discarded_lines = 0
        self.overflow_lines = 0

    def feed(self, data: bytes) -> Tuple[Status, ...]:
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("serial data must be bytes-like")

        parsed = []
        for byte in data:
            if byte in (ord("\r"), ord("\n")):
                if not self._discarding and self._line:
                    event = self._parse_current_line()
                    if event is not None:
                        parsed.append(event)
                    else:
                        self.discarded_lines += 1
                self._line.clear()
                self._discarding = False
                continue

            if self._discarding:
                continue
            if len(self._line) >= self._max_line_bytes:
                self._line.clear()
                self._discarding = True
                self.overflow_lines += 1
                continue
            self._line.append(byte)
        return tuple(parsed)

    def _parse_current_line(self) -> Optional[Status]:
        # Keep only this fixed event; never retain or return the containing line.
        if b"System starting..." in self._line:
            return BootMarker()
        try:
            line = self._line.decode("ascii")
        except UnicodeDecodeError:
            return None
        return (
            parse_wifi_status(line)
            or parse_bluetooth_status(line)
            or parse_wifi_sync_diagnostic(line)
            or parse_wifi_tcp_failure_diagnostic(line)
        )


def evaluate_wifi_restored(
    before: Optional[WifiStatus],
    after: Optional[WifiStatus],
    *,
    elapsed_ms: int,
    timeout_ms: int = 30000,
    tcp_disconnected_seen: bool = False,
    timing_is_upper_bound: bool = False,
    lower_bound_elapsed_ms: Optional[int] = None,
    deadline_elapsed: bool = False,
) -> str:
    """Return PASS/FAIL/NOT PROVEN without treating UNKNOWN as disconnected."""
    if before is None or before.state != "CONNECTED" or before.sta != "CONNECTED" or before.ip != "READY":
        return "NOT PROVEN"
    if after is None:
        return "NOT PROVEN"

    if before.tcp == "CONNECTED" and (tcp_disconnected_seen or after.tcp == "DISCONNECTED"):
        return "FAIL"

    restored = (
        after.state == "CONNECTED"
        and after.credentials == "none"
        and after.sta == "CONNECTED"
        and after.ip == "READY"
    )
    if before.tcp == "CONNECTED":
        if after.tcp == "UNKNOWN":
            return "NOT PROVEN"
        restored = restored and after.tcp == "CONNECTED"

    if restored:
        if 0 <= elapsed_ms <= timeout_ms:
            return "PASS"
        if timing_is_upper_bound:
            if lower_bound_elapsed_ms is not None and lower_bound_elapsed_ms > timeout_ms:
                return "FAIL"
            return "NOT PROVEN"
        return "FAIL"

    if deadline_elapsed:
        required = (after.state, after.credentials, after.sta, after.ip)
        if "UNKNOWN" in required:
            return "NOT PROVEN"
        if before.tcp == "CONNECTED" and after.tcp == "UNKNOWN":
            return "NOT PROVEN"
        return "FAIL"
    return "NOT PROVEN"


def evaluate_bluetooth_preserved(
    before: Optional[BluetoothStatus], after: Optional[BluetoothStatus]
) -> str:
    if before is None or before.state != "CONNECTED" or after is None:
        return "NOT PROVEN"
    if after.state == "CONNECTED":
        return "PASS"
    return "FAIL"


def snapshot_to_dict(snapshot: StatusSnapshot) -> dict:
    return {
        "wifi": asdict(snapshot.wifi) if snapshot.wifi is not None else None,
        "bluetooth": asdict(snapshot.bluetooth) if snapshot.bluetooth is not None else None,
        "wifi_sync_diagnostic": (
            asdict(snapshot.wifi_sync_diagnostic)
            if snapshot.wifi_sync_diagnostic is not None else None
        ),
        "wifi_tcp_failure_diagnostic": (
            asdict(snapshot.wifi_tcp_failure_diagnostic)
            if snapshot.wifi_tcp_failure_diagnostic is not None else None
        ),
    }
