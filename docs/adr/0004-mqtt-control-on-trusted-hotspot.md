# ADR 0004: Restrict Unauthenticated MQTT Control to a Trusted Hotspot

- **Status:** Accepted for first-version design.
- **Date:** 2026-10-29

## Context

The first version is a single-board demonstration on a private hotspot controlled by the project owner. Adding Broker credentials and topic permissions increases setup and configuration work.

## Decision

- Run the local Broker on the project computer and keep the browser dashboard reachable only from that computer.
- Do not add MQTT application authentication for the first version. Use remote control only on the owner's hotspot with known, trusted devices.
- Do not change Windows firewall or ICS settings as part of this design.
- Before using a campus, shared, or otherwise untrusted network, add Broker authentication and topic ACLs.

## Consequences

- The first demonstration has less setup, but any device with access to the trusted hotspot may be able to publish control commands.
- The unauthenticated design is out of scope for shared or untrusted networks.

## Alternatives considered

- Add Broker authentication and topic ACLs in the first version: deferred while deployment remains limited to the owner's trusted hotspot.
