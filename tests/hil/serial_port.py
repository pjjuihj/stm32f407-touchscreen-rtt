"""Safe, identity-based serial access for the F407 USART1 HIL workflow."""

from __future__ import annotations

from typing import Callable, Iterable, Optional


CH340_VID = 0x1A86
CH340_PID = 0x7523
SERIAL_BAUDRATE = 115200


class PortSelectionError(RuntimeError):
    """The expected USB-UART identity is absent or ambiguous."""


class SerialOpenError(RuntimeError):
    """The uniquely identified serial device could not be opened safely."""


def find_unique_ch340(ports: Iterable[object]) -> str:
    matches = []
    for port in ports:
        if getattr(port, "vid", None) == CH340_VID and getattr(port, "pid", None) == CH340_PID:
            device = getattr(port, "device", None)
            if isinstance(device, str) and device:
                matches.append(device)

    if len(matches) != 1:
        raise PortSelectionError(
            "Expected exactly one CH340 USB-UART (VID 1A86, PID 7523); "
            f"found {len(matches)}. No serial port was opened."
        )
    return matches[0]


def open_serial(
    *,
    port_enumerator: Optional[Callable[[], Iterable[object]]] = None,
    serial_factory: Optional[Callable[[], object]] = None,
    timeout: float = 0.05,
) -> object:
    """Open the unique CH340 after configuring inactive control lines.

    Dependencies are loaded lazily so the identity and ordering tests do not
    need hardware or pySerial. A caller cannot force a fallback port.
    """
    if port_enumerator is None or serial_factory is None:
        try:
            import serial
            from serial.tools import list_ports
        except ImportError as exc:
            raise SerialOpenError("pySerial is unavailable; no serial port was opened.") from exc
        if port_enumerator is None:
            port_enumerator = list_ports.comports
        if serial_factory is None:
            serial_factory = serial.Serial

    try:
        device = find_unique_ch340(port_enumerator())
    except PortSelectionError:
        raise
    except Exception as exc:
        raise PortSelectionError(
            "CH340 devices could not be enumerated; no serial port was opened."
        ) from exc
    try:
        port = serial_factory()
    except Exception as exc:
        raise SerialOpenError("Serial object could not be created; no port was opened.") from exc
    try:
        port.port = device
        port.baudrate = SERIAL_BAUDRATE
        port.timeout = timeout
        port.write_timeout = timeout
        port.xonxoff = False
        port.rtscts = False
        port.dsrdtr = False
        port.dtr = False
        port.rts = False
        port.open()
        return port
    except Exception as exc:
        try:
            if getattr(port, "is_open", False):
                port.close()
        except Exception:
            pass
        raise SerialOpenError(
            "The uniquely identified CH340 could not be opened with safe line settings."
        ) from exc
