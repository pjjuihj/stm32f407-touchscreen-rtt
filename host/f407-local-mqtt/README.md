# F407 Local MQTT Dashboard

This is the host-side first-version shell for the local IoT project. It provides:

- An in-memory MQTT 3.1.1 broker bound only to the configured private hotspot address.
- A browser dashboard and MQTT-over-WebSocket endpoint bound only to 127.0.0.1.
- Retained device availability and state, plus non-retained ACK and command topics.
- A strict topic and payload gate; the dashboard only publishes the three approved command types.
- No cloud connection, Windows firewall or ICS changes, automatic browser launch, or persistent broker database.

The F407 MQTT 3.1.1 client is implemented in source and uses the ESP8266's existing AT TCP socket; it does not rely on an ESP MQTT AT command. The ESP inventory check from Issue 6 attempt 65 returned `UNKNOWN`, which neither proves nor disproves MQTT AT support. If the F407 hardware RNG is unavailable, telemetry uses a reserved all-zero session marker and remote control remains disabled.

**First-version verification:** the MQTT-enabled F407 firmware builds with CMake/Ninja. The host CMake suite has 18 passing offline tests; `npm test` adds a black-box broker/dashboard test using loopback sockets and a simulated device. These checks do not prove board connectivity or physical LED/buzzer output. The last verified on-board image is the temporary MQTT-scheduler isolation diagnostic (Issue 6 attempt 70, SHA-256 `D8AEFD9CCA3CC7BEB264D9AEAEBF869C9910FA873FF7C7DE2DAF70A02547798D`), not the MQTT release image. The MQTT release image has not passed board-level acceptance: attempt 69 reached Wi-Fi/IP ready, but the ESP returned `AT_ERROR` during TCP connection. Further hardware work requires a new per-operation authorization.

## Run locally

Install the pinned, project-local packages and start the host from this folder:

    npm ci --ignore-scripts
    npm start

Run the host-side integration test with `npm test`. It starts the service on loopback-only test ports, checks the dashboard assets, exercises MQTT over TCP and WebSocket with a simulated device, and shuts down the test process. It does not contact the F407 or change firewall/ICS settings.

Open the printed http://127.0.0.1:9000 address in this computer's browser. The broker defaults to 192.168.137.1:1883, the Windows hotspot address used by this project. If the hotspot interface uses another private IPv4 address, set `F407_MQTT_BIND_ADDRESS` in that PowerShell window before starting the app, and use the same address for `IOT_MQTT_BROKER_HOST` when configuring the firmware. The process refuses public and wildcard bind addresses; it never falls back to 0.0.0.0.

F407_DASHBOARD_PORT and F407_MQTT_PORT can override the local web and broker ports. The dashboard listener remains loopback-only.

No firewall rule is created. If the existing host policy prevents a board from reaching the broker, the app reports the connection as unverified; configure network policy separately only when explicitly authorized.

The firmware uses CMake. Its default `IOT_MQTT_ENABLE=ON` includes the F407 MQTT client and disables the older automatic TCP diagnostic. Setting `IOT_MQTT_AT_CAPABILITY_DIAGNOSTIC_ENABLE=ON` excludes the MQTT client source so the diagnostic and production client do not compete for the ESP command channel. The F407 broker address and port are compile-time settings `IOT_MQTT_BROKER_HOST` and `IOT_MQTT_BROKER_PORT`; if the broker bind address is changed in PowerShell, configure with the same address, for example:

    cmake -S . -B .ai/tmp/f407-mqtt-build -DIOT_MQTT_ENABLE=ON -DIOT_MQTT_BROKER_HOST=192.168.137.1 -DIOT_MQTT_BROKER_PORT=1883

This command is an example, not a build result. Do not treat a successful CMake build as board-level MQTT proof; the firmware still needs a separately authorized flash and hardware acceptance.

## MQTT topics

Topic root: f407/f407-touchscreen-01/.

| Topic | Direction | QoS | Retained |
| --- | --- | ---: | --- |
| availability | Device to broker | 1 | Yes |
| state | Device to broker | 1 | Yes |
| cmd | Dashboard to device | 1 | No |
| ack | Device to dashboard | 1 | No |

The broker accepts only the fixed device client ID or a generated dashboard client ID, and rejects either client if it requests a persistent session. The dashboard publishes commands only while it has a fresh F407 heartbeat and the F407 MQTT session is available. Commands are non-retained, and the clean-session gate prevents offline command queueing. Broker persistence is in memory; stopping this process clears retained snapshots.

## Version 1 JSON contract

All JSON is UTF-8 and bounded to 1024 bytes. state, cmd, and ack require schema_version: 1 and the fixed device_id.

State:

    {
      "schema_version": 1,
      "device_id": "f407-touchscreen-01",
      "session_id": "0123456789abcdef",
      "seq": 1,
      "uptime_s": 10,
      "wifi": { "sta": "connected", "ip": "192.168.137.10" },
      "bt_connected": false,
      "led": [false, true, false],
      "buzzer_gpio_high": false,
      "dropped_snapshots": 0,
      "remote_control_available": true
    }

Command examples:

    {"schema_version":1,"device_id":"f407-touchscreen-01","session_id":"0123456789abcdef","command_id":"fedcba9876543210","command":"led.set","led_index":1,"value":"on"}
    {"schema_version":1,"device_id":"f407-touchscreen-01","session_id":"0123456789abcdef","command_id":"fedcba9876543211","command":"buzzer.pulse","duration_ms":500}
    {"schema_version":1,"device_id":"f407-touchscreen-01","session_id":"0123456789abcdef","command_id":"fedcba9876543212","command":"system.get"}

Acknowledgement:

    {
      "schema_version": 1,
      "device_id": "f407-touchscreen-01",
      "session_id": "0123456789abcdef",
      "command_id": "fedcba9876543210",
      "command": "led.set",
      "result": "OK",
      "readback": { "led": [false, true, false], "buzzer_gpio_high": false }
    }

The dashboard does not interpret an unsupported schema as a legacy version. It sends one command at a time, retries the same command ID at most twice after the initial send, and then holds an explicit unknown-result lock until the user clears it. When the browser loses its broker connection or the F407 heartbeat becomes stale, it stops retries rather than queueing commands offline.

This demo deliberately has no MQTT username/password. Use it only on the owner's trusted hotspot with known devices. The broker rejects public/wildcard bind addresses but does not authenticate clients on that private network.
