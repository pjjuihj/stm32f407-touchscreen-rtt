# ADR 0002: Do Not Run an Automatic Network Probe

- **Status:** Accepted for implementation; hardware acceptance pending.
- **Date:** 2026-10-08
- **Scope:** Supersedes only the one-shot NCSI probe decision in ADR 0001, decision 4. All other ADR 0001 decisions remain in force.

## Context

Issue 5 limits the firmware's AP+STA responsibility to preserving and observing the ESP8266 state needed for hotspot association. An automatic HTTP connectivity probe adds a temporary TCP connection and payload transmission after station IPv4 becomes ready. That probe is outside the required AP+STA behavior and can disturb a transport the firmware does not own.

## Decision

- Do not automatically open a TCP socket, send an HTTP request, or close a temporary socket to test Internet reachability.
- Keep the UART status field NET=NOT_RUN so existing status consumers retain the same field order and value vocabulary.
- Keep explicit user-requested TCP connect, send, status-query, and close operations separate from automatic synchronization.
- Treat Internet reachability as outside Issue 5 and Issue 6 acceptance.

## Consequences

- Automatic Wi-Fi synchronization ends after station and IP state are observed; it does not use the TCP transport for a connectivity test.
- NET=NOT_RUN means no network probe was run. It does not mean Internet access passed or failed.
- Host tests can verify that synchronization emits no automatic TCP open/send/close commands. They cannot establish hotspot association or Internet access on hardware.
