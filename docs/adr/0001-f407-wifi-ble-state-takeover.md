# ADR 0001: Nonblocking Wi-Fi and BLE State Takeover

- **Status:** Accepted for local implementation; hardware acceptance pending.
- **Date:** 2026-10-04

## Context

The ESP8266 can remain powered and retain Wi-Fi and transport state while the STM32F407 resets. DX-BT05 also owns its live Bluetooth link independently of the MCU. Unconditionally initializing, disconnecting, or rebuilding these module states after an MCU-only reset can disrupt working connections. Synchronous waits also delay the display and other main-loop work.

## Decision

1. Keep the current bare-metal main loop and screen. Refresh the first LCD frame before starting Wi-Fi synchronization.
2. Synchronize ESP8266 state through a bounded UART4 state machine. Support modern ESP-AT and legacy NONOS-AT query forms.
3. Preserve the `qiming` SoftAP and target AP+STA operation. Query TCP/transport state before any mode change. Only attempt an AP+STA mode adjustment when transport is explicitly known disconnected; otherwise preserve the current module configuration and report the limitation.
4. Track mode, STA association, IPv4 readiness, TCP/transport state, and the one-shot NCSI result independently. An ambiguous or malformed reply remains unknown. Only an explicitly known-free transport permits the NCSI temporary socket.
5. Keep newly supplied Wi-Fi credentials in F407 RAM only. For modern ESP-AT, issue `AT+SYSSTORE=0` before network configuration. Do not erase potentially useful module-side stored state; whether older firmware has prior persisted credentials is not established.
6. Treat the DX-BT05 connection indicator as hardware-owned via PF13 `STATE`. USART3 receive interrupts only enqueue bytes; bounded `S...E` commands are decoded and applied by `BT_Process()` in the main loop. No reset/disconnect is sent to the module during takeover.

## Subsequent Decision: Explicit ESP Credential Persistence (2026-10-23)

The RAM-only rule in Decision 5 is superseded for the new, explicit `WIFI CONNECT SAVE <ssid>|<password>` command:

- SAVE writes the AP+STA mode and Wi-Fi profile to the ESP8266's nonvolatile configuration and enables power-up auto-connect.
- The ordinary `WIFI CONNECT <ssid>|<password>` command remains volatile. Modern ESP-AT returns to `AT+SYSSTORE=0` after the SAVE sequence.
- `WIFI CLEAR` clears F407 runtime credentials and disconnects the current station; it does not erase the ESP's saved profile.
- Do not claim cold-start auto-reconnect is hardware-verified until the saved-profile flow has been flashed and tested across an ESP power cycle.

The command split follows Espressif's [ESP-AT 2.1 Wi-Fi command reference](https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/Wi-Fi_AT_Commands.html) and [ESP8266 Non-OS AT reference](https://www.espressif.com/sites/default/files/documentation/4a-esp8266_at_instruction_set_en.pdf): modern ESP-AT stores `CWMODE`/`CWJAP` configuration when `SYSSTORE=1`, while Non-OS AT provides `CWMODE_DEF`/`CWJAP_DEF`; both document power-up auto-connect through `CWAUTOCONN=1`.

## Consequences

- An F407 reset can restore its view of an existing Wi-Fi or BLE connection without deliberately resetting either module.
- If a query is incomplete, the firmware may report unknown and defer a potentially disruptive operation rather than guessing.
- AP+STA channel coordination may briefly interrupt SoftAP clients while STA changes channel; the SoftAP configuration itself is not rewritten.
- The BLE frame vocabulary and electrical STATE behavior require on-device/app verification; host tests validate parser and dispatch behavior only.
- Host tests/build establish software behavior only. Reset recovery, the live ESP AT dialect, NCSI reachability, and BLE app compatibility remain unproven until hardware tests.

## Alternatives considered

- Reset the ESP8266 whenever the F407 resets: rejected because it destroys the live module state the takeover is intended to preserve.
- Assume an unknown query means disconnected: rejected because it can trigger a mode change or temporary socket while a real connection exists.
- Perform module setup with blocking delays before the UI: rejected because it delays the screen and other main-loop work.
- Persist the hotspot password in F407 flash: rejected at the original decision date; the later SAVE command stores it on the ESP8266, not in F407 flash.
