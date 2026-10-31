# ADR 0005: Scope MQTT Commands to the Current F407 Boot Session

- **Status:** Accepted for first-version design.
- **Date:** 2026-10-29

## Context

QoS 1 and dashboard retries can deliver a command more than once, while a lost acknowledgment can leave the caller unsure whether an action executed. Retained or queued commands could also arrive after the user believes the device is offline. Persisting session and command history would require Flash writes.

## Decision

- Generate a new 64-bit session ID for each F407 boot using the hardware RNG. If the RNG is unavailable, keep remote command execution disabled. Scope every remote command to the current session; reject commands from earlier sessions.
- Give each new action a fresh 64-bit command ID. Retries reuse that ID. Never retain or queue command messages for offline delivery; use a fresh MQTT clean session on reconnect.
- Keep a bounded duplicate-result cache in RAM and replay the prior acknowledgment for IDs still in the cache. Do not claim exactly-once execution beyond that bounded cache.
- Allow one dashboard command to await acknowledgment at a time. After finite retries of the same ID, report the result as unknown and let the user decide what to do; do not silently create a new command ID.
- Do not persist command history or telemetry backlog to Flash. A reset begins a new session and sequence and discards the old RAM state.

## Consequences

- Old-session and offline commands cannot be intentionally replayed after reconnect, and duplicate IDs within the cache do not repeat an action.
- An acknowledgment lost beyond the retry window leaves an unknown result; reboot also loses the bounded cache and unsent telemetry backlog.

## Alternatives considered

- Persist pending commands and duplicate history in Flash: rejected for the first version because it adds write management and can cause delayed actuator actions after reconnect.
- Automatically issue a new command after an unknown result: rejected because the first action may already have executed.
