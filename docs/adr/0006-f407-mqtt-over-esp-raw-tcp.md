# ADR 0006: Run MQTT on the F407 over the ESP AT TCP Socket

- **Status:** Accepted for the first version; the final F407 image builds and the strict offline suite passes 18/18. The dashboard subscription and `+IPD` trailing-CRLF test issues described in earlier drafts were corrected. On 2026-10-30, attempt 69 wrote and read back the final image successfully, but runtime MQTT acceptance failed: with STA/IP ready, three automatic `AT+CIPSTART` attempts returned generic `ERROR`; no MQTT availability or heartbeat reached the Broker. A later 90-second diagnostic again recorded automatic connect failures. The filtered PktMon capture was found at `C:\Windows\System32\PktMon.etl`; conversion yielded zero packets and zero drops for TCP port 1883 at hotspot address `192.168.137.1`. No matching packet was captured during that window. Because the UART summary has no per-event timestamps, the capture cannot be tied to a specific `AT_ERROR` or used to localize its cause. Hardware acceptance remains open.
- **Date:** 2026-10-29
- **Supersedes:** The ESP MQTT AT transport choice in [ADR 0003](0003-esp8266-mqtt-at-capability-gate.md).

## Context

The ESP8266 command inventory result from Issue 6 attempt 65 was `UNKNOWN / QUERY_REJECTED_OR_UNSUPPORTED`. It did not establish whether the custom module firmware has MQTT AT commands. Repeatedly probing or reflashing the ESP is not a suitable prerequisite for the first application version. The existing F407 Wi-Fi layer already provides an AT-controlled TCP socket, but application code did not receive its `+IPD` payload bytes.

## Decision

- Keep Wi-Fi association and the AT TCP socket on the ESP8266; implement the MQTT 3.1.1 session, packet handling, JSON contract, state queue, command validation, and acknowledgments on the F407.
- Route bounded `+IPD` payloads from the existing UART4 receive path into the F407 TCP receive queue. Keep parsing and MQTT state transitions cooperative in the main loop so LVGL and touch processing remain schedulable.
- Connect to the local Broker at `192.168.137.1:1883` by default. The host Broker and dashboard remain loopback/hotspot bound as documented in the host README.
- Use a clean MQTT session, QoS 1 for state, ACK, command, and availability, retained state and availability, a retained offline Last Will, and non-retained commands.
- Do not reflash the ESP as part of this decision. Do not claim MQTT AT support or lack of support from the attempt 65 query result.
- Keep a CMake switch for the read-only AT inventory diagnostic; that diagnostic build excludes the F407 MQTT client so only one state machine owns the ESP command path.

## Consequences

- The F407 now carries a bounded TCP receive parser and MQTT client, which increases code and RAM use compared with delegating MQTT to the ESP.
- The implementation can be evaluated without changing the ESP image. The final image is on the target and its flash segments were read back successfully, but the ESP rejects each automatic TCP open with a generic `ERROR`; TCP payload reception and end-to-end MQTT behavior remain unverified.
- The host capture records no matching packets in its 90-second window: the broker listener was active, the capture filter was TCP/1883 and `192.168.137.1`, and the Pcapng conversion contained zero packets and zero drops. The UART result has no per-event timestamps, so it is not proven that a particular failed `AT+CIPSTART` overlapped the capture; this does not yet localize the failure. Source code builds the single-connection `AT+CIPSTART="TCP","host",port` form documented by Espressif, but the actual UART transmit bytes and a more specific module error were not captured.
- A successful source build alone will not prove that the installed ESP AT firmware delivers the expected `+IPD` stream or that the screen and touch remain responsive during network operation.
- The current strict offline suite passes 18/18, including MQTT core, Wi-Fi takeover, and `+IPD` framing. The dashboard subscription uses the corrected MQTT.js form, and the parser test confirms that trailing AT bytes pass through after the declared TCP payload length. Host-side results do not prove board-level MQTT acceptance.

## Alternatives considered

- Delegate MQTT to ESP MQTT AT: deferred because attempt 65 returned `UNKNOWN` and the current module's feature set remains unproven.
- Replace MQTT with a custom application TCP protocol: rejected because the project needs standard local broker tooling and MQTT topics.
- Flash a new ESP image: rejected for the first version because it adds a separate recovery risk and was not needed to implement the F407-side client.
