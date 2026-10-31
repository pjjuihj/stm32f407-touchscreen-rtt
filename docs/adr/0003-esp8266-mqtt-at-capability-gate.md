# ADR 0003: Delegate MQTT Transport to ESP8266 MQTT AT

- **Status:** Superseded by [ADR 0006](0006-f407-mqtt-over-esp-raw-tcp.md).
- **Date:** 2026-10-29

## Context

The board already uses an ESP8266 for Wi-Fi, but its custom AT image has not been proven to provide the MQTT commands and delivery controls required by this design. Implementing MQTT directly on the F407 would add a TCP receive path and MQTT protocol handling to the MCU.

## Decision

- Delegate MQTT network transport to the ESP8266 MQTT AT client. Keep device state, command validation, actuator behavior, and acknowledgments in the F407 application.
- Before implementation, use read-only checks to verify MQTT configuration and connection, QoS 1 publish/subscribe, subscription receive notifications, retained publications, Last Will, and clean-session support.
- Use a fresh nonpersistent MQTT session on reconnect, and configure the Broker not to queue commands for an offline subscriber. A non-retained command by itself does not prevent persistent-session queuing.
- If any required capability is missing, stop and compare alternatives. Do not automatically flash the ESP8266 or F407.

## Consequences

- The F407 avoids a new full MQTT/TCP receive stack, while the solution remains dependent on the actual ESP AT dialect and Broker behavior.
- The official ESP-AT command reference is only a capability checklist; it does not establish what is present in this board's custom image.

## Alternatives considered

- Implement MQTT and TCP reception on the F407: deferred unless the capability gate fails.
- Replace MQTT with a custom TCP protocol: deferred unless the capability gate fails.
