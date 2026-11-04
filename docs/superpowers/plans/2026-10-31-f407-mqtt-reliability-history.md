# F407 MQTT Reliability History Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add a browser-local 24-hour reliability timeline and CSV export to the existing F407 local MQTT dashboard.

**Architecture:** Add two dependency-free browser scripts: one owns IndexedDB retention and memory fallback; the other projects incoming MQTT state, availability, and ACK messages into deduplicated history events. The existing dashboard remains the MQTT client and renders the timeline, summaries, storage status, and CSV export.

**Tech Stack:** Browser JavaScript, IndexedDB, HTML/CSS, Node.js 20+ with built-in node:test.

## Global Constraints

- Keep history local to the dashboard origin and browser profile.
- Retain no more than 24 hours and 5,000 events; show the actual retained coverage when the count cap shortens it.
- Record meaningful live state changes immediately and stable live state at most once per 60 seconds.
- Treat retained Broker snapshots as recent-known snapshots, never as live transitions or freshness recovery.
- Deduplicate sequenced state by device/session/seq, legacy no-seq state by a short normalized-fingerprint window, and ACK by session/command ID.
- Measure ACK latency only for the matching pending command/session, from first publish to ACK receipt.
- Store only selected status and ACK fields; never store credentials, raw MQTT payloads, serial data, or exception stacks.
- Keep MQTT rendering and message handling operational when browser storage fails; use bounded in-memory history and show a persistence warning.
- Add no npm dependencies; do not modify firmware, Broker behavior, network settings, or hardware.
- Do not stage, commit, push, or publish changes without a separate request.

## Baseline and File Map

The approved design is in docs/superpowers/specs/2026-10-31-f407-mqtt-reliability-history-design.md.

The managed implementation worktree is based on merge commit 8c91295f228933862e5037012312f013d5683bc4, which includes PR #13 and the local MQTT dashboard. The dirty primary checkout was left unchanged. This worktree was clean before implementation; a start-state checkpoint was created at `.ai/backups/20261031-f407-mqtt-history-implementation/` before editing source files.

Before implementation, create a fresh managed worktree at the verified main commit using a checkout that can resolve that commit. Confirm host/f407-local-mqtt/public/app.js and host/f407-local-mqtt/package.json exist, and verify the new worktree has no pre-existing user changes. The current local Git installation lacks its HTTPS remote helper; if the verified main commit cannot be resolved locally, stop and repair/replace the checkout through the connected repository workflow before editing code.

| File | Responsibility |
|---|---|
| host/f407-local-mqtt/public/history-store.js | IndexedDB store, 24-hour/5,000-event retention, bounded memory fallback, capacity-drop metadata, CSV serialization |
| host/f407-local-mqtt/public/history-recorder.js | Project MQTT messages into safe history events; detect changes, minute samples, retained snapshots, dedupe, dropped-snapshot deltas, and matched ACK latency |
| host/f407-local-mqtt/public/app.js | Connect the recorder to existing MQTT handlers, render history status and summaries, paginate events, trigger CSV download |
| host/f407-local-mqtt/public/index.html | Load the scripts in order and add the accessible history panel |
| host/f407-local-mqtt/public/styles.css | Style the timeline, summaries, warning, pagination, and responsive layout |
| host/f407-local-mqtt/test/history-store.test.mjs | Offline tests for storage retention, fallback, ordering, and CSV |
| host/f407-local-mqtt/test/history-recorder.test.mjs | Offline tests for MQTT-to-history projection and deduplication |
| host/f407-local-mqtt/test/app-state-compat.test.mjs | Dashboard compatibility test for visible no-seq status and disabled remote controls |
| host/f407-local-mqtt/test/server.test.mjs | Assert the served dashboard includes the history assets and panel |
| host/f407-local-mqtt/README.md | Explain local-only history, retention limits, CSV export, and clearing browser site data |

The two browser scripts expose these interfaces on globalThis:

```javascript
F407HistoryStore.create({ indexedDB, backend, now, onStatus });
// -> { ready, record(event), list(), getStatus() }

F407HistoryRecorder.create({ store, now });
// -> { observeState(state, packet), observeAvailability(value, packet), observeAck(ack, options), observeStorage(status) }

F407HistoryStore.toCsv(events);
```

For tests, backend implements open(), getAll(), put(event), delete(ids), getMeta(key), and putMeta(key, value). Production uses an IndexedDB backend; tests inject a deterministic in-memory backend or one that rejects writes.

---

### Task 0: Establish the merged dashboard baseline

**Files:** None; worktree setup only.

- [x] Create a managed worktree at commit 8c91295f228933862e5037012312f013d5683bc4 on a feature branch named codex/f407-mqtt-history-implementation.
- [x] Confirm the commit and dashboard files before touching source:

```powershell
git rev-parse HEAD
Test-Path host/f407-local-mqtt/public/app.js
Test-Path host/f407-local-mqtt/package.json
git status --short --branch
```

Expected: HEAD is 8c91295f228933862e5037012312f013d5683bc4, both paths exist, and the worktree is clean except for this approved design/plan if copied into the worktree.

- [x] Copy only the approved design and plan documents from the design worktree into the fresh worktree. Do not copy host source files from the dirty primary checkout.
- [x] Confirmed the exact main commit and dashboard files were available, so no fallback or stop path was needed.

### Task 1: Add tested local history storage

**Files:**
- Create: host/f407-local-mqtt/public/history-store.js
- Create: host/f407-local-mqtt/test/history-store.test.mjs

**Interfaces:**
- Produces F407HistoryStore.create(options), returning ready, record(event), list(), and getStatus().
- Produces F407HistoryStore.toCsv(events), returning UTF-8 CSV text.
- The store accepts an injected backend only for tests; production opens IndexedDB database f407-dashboard-history, version 1, with events and metadata stores.

- [x] Write failing tests for expiry, the 5,000-event cap, ordering, memory fallback, and CSV quoting. Use a deterministic clock and injected backend. Example cap test:

```javascript
test('keeps the newest 5000 rows and counts capacity evictions', async () => {
  const history = F407HistoryStore.create({ backend: createMemoryBackend(), now: () => now });
  await history.ready;
  for (let index = 0; index < 5001; index += 1) {
    await history.record({ type: 'state', receivedAt: now + index, data: { seq: index + 1 } });
  }
  const rows = await history.list();
  assert.equal(rows.length, 5000);
  assert.equal(rows[0].data.seq, 2);
  assert.equal((await history.getStatus()).capacityDropped, 1);
});
```

- [x] Run the focused test and confirm it fails for the missing module/API:

```powershell
npm test -- test/history-store.test.mjs
```

Expected: FAIL because the history store does not exist yet.

- [x] Implement IndexedDB schema version 1 with an events store indexed by receivedAt and a metadata store for the cumulative capacityDropped count. Validate and project fields before writing; never persist the full incoming MQTT payload.
- [x] On startup and after append, delete rows older than 24 hours, then delete the oldest rows until at most 5,000 remain. Increment capacityDropped only for count-cap eviction, not age expiry.
- [x] Keep a 5,000-row in-memory ring in parallel. If IndexedDB is missing or a transaction fails, continue recording in memory, mark persistent=false, report one sanitized status change, and resolve record() without throwing into MQTT handlers.
- [x] Implement list() with same-device/session state rows ordered by seq while retaining receive-time positions for other events; calculate expiration, capacity, oldestAt, and coverageMs from receive time.
- [x] Implement CSV headers and RFC-style escaping for commas, quotes, and line breaks. Export only event fields defined by the design.
- [x] Rerun the focused test and require PASS. Then run npm test from host/f407-local-mqtt and require the existing server test plus the new store tests to pass.

### Task 2: Add deterministic MQTT event projection

**Files:**
- Create: host/f407-local-mqtt/public/history-recorder.js
- Create: host/f407-local-mqtt/test/history-recorder.test.mjs

**Interfaces:**
- Consumes F407HistoryStore.create(...).record(event).
- Produces F407HistoryRecorder.create({ store, now }).
- observeState(state, packet) accepts the already validated dashboard state plus packet.retain.
- observeAvailability(value, packet) accepts online/offline plus packet.retain.
- observeAck(ack, { matched, latencyMs, receivedAt }) accepts a validated ACK; latencyMs is null unless command and session IDs matched.

- [x] Write tests for each rule before implementation. Required cases:

```javascript
test('does not turn a retained state snapshot into a live transition', () => {
  recorder.observeState(state({ session_id: '0123456789abcdef', seq: 1 }), { retain: true });
  assert.equal(events[0].type, 'state-snapshot');
  assert.equal(events[0].retained, true);
  assert.equal(events[0].countsAsLive, false);
});

test('deduplicates QoS redelivery and only records stable samples once per minute', () => {
  recorder.observeState(state({ session_id: '0123456789abcdef', seq: 1 }), { retain: false });
  recorder.observeState(state({ session_id: '0123456789abcdef', seq: 1 }), { retain: false });
  now += 60000;
  recorder.observeState(state({ session_id: '0123456789abcdef', seq: 7 }), { retain: false });
  assert.equal(events.filter((event) => event.type === 'state').length, 2);
});
```

- [x] Run the focused test and confirm it fails before implementation:

```powershell
npm test -- test/history-recorder.test.mjs
```

Expected: FAIL because the recorder API does not exist yet.

- [x] Implement meaningful state comparison over Wi-Fi/IP, Bluetooth, LED states, buzzer GPIO, remote-control availability, and dropped-snapshot count. Exclude seq and uptime from change comparison; include session changes.
- [x] Record the first live state and each meaningful change immediately. Record an unchanged live state no more than once per 60 seconds. When a change and sample coincide, write one state row with both reasons.
- [x] Deduplicate live state by device/session/seq and ACK by session/command ID. A repeated retained snapshot is deduplicated by its normalized state fingerprint.
- [x] Store retained state/availability as recent-known snapshots only. Deduplicate repeats within the page, give a new page and post-live replay a fresh persisted key, and do not count snapshots as live transitions or heartbeat freshness.
- [x] Accept state messages without seq for display/history and minute sampling; keep remote commands disabled and label the missing sequence.
- [x] On the first transition from persistent storage to memory-only mode, record one sanitized storage event in memory. Do not recursively record repeated failures or include exception details.
- [x] For dropped_snapshots, establish a baseline at the first state of each session; record only positive deltas within that session.
- [x] Record ACK once. Keep latency null for unmatched ACKs; use the supplied latency only for a matching session ID and command ID.
- [x] Rerun recorder tests and require PASS, then run npm test and require all tests to pass.

### Task 3: Integrate collection and ACK timing into the dashboard

**Files:**
- Modify: host/f407-local-mqtt/public/app.js
- Modify: host/f407-local-mqtt/public/index.html

**Interfaces:**
- app.js creates one store and recorder before connectDashboard().
- MQTT handlers pass their validated payload and packet retain flag to the recorder.
- The existing 40-row recent-activity list remains a transient diagnostic list; the new history is separate.

- [x] Load history-store.js, then history-recorder.js, then app.js as deferred classic scripts in index.html.
- [x] Create the store with an onStatus callback that updates the persistence banner and does not log storage exceptions or local paths. On the first persistent-to-memory-only transition, call observeStorage() once.
- [x] Call observeAvailability from parseAvailability(), observeState for accepted live states in the sentinel, retained-live-confirmation, and normal-state branches of handleState(), and observeAck before clearPending() in handleAcknowledgement().
- [x] Ignore invalid and lower-sequence live states already rejected by handleState(); record a retained snapshot separately and its first live confirmation as live without changing freshness logic.
- [x] In sendNewCommand()/publishPendingCommand(), set pendingCommand.firstPublishedAt using performance.now() on the first actual publish only. On a matching ACK, compute max(0, performance.now() - firstPublishedAt) and pass it to observeAck; retries keep the original time.
- [x] Keep the current command behavior, MQTT topics, QoS, retry limit, and retained flags unchanged.
- [x] Add a server.test.mjs assertion that the served HTML contains both history scripts in order and the history panel IDs. Run npm test and require PASS.

### Task 4: Add the history panel, pagination, and CSV export

**Files:**
- Modify: host/f407-local-mqtt/public/index.html
- Modify: host/f407-local-mqtt/public/styles.css
- Modify: host/f407-local-mqtt/public/app.js
- Modify: host/f407-local-mqtt/README.md

- [x] Add an accessible history section after the existing dashboard content. Include separate Wi-Fi/IP and MQTT availability summaries; do not collapse them into one online badge.
- [x] Display retained coverage (oldest time and duration), stored row count, history capacity evictions, device-reported dropped snapshots, last live update time, and ACK latency count/minimum/median/maximum.
- [x] Render the newest 100 rows using textContent. Add a “加载更早记录” button that appends the next 100 rows until all retained rows are visible; do not place all 5,000 rows in the DOM at startup.
- [x] Add a CSV button that reads all currently retained rows, serializes them with F407HistoryStore.toCsv(), creates a UTF-8 Blob, triggers a download, and revokes its object URL.
- [x] If IndexedDB is unavailable, quota is exceeded, or a write fails, show “历史记录未能持久保存”; keep the live dashboard and bounded in-memory history operational.
- [x] Use the existing dark dashboard styles and responsive breakpoints. Keep all user-visible text inserted with textContent; do not interpolate MQTT data into innerHTML.
- [x] Update README.md with the local-only storage boundary, 24-hour/5,000-row cap, CSV export, and how clearing this site's browser data removes history.
- [x] Check the narrow and wide dashboard layouts, export the CSV, and inspect the downloaded header and rows in a text view.

### Task 5: Full offline and local-browser verification

**Files:** No additional files unless a focused test exposes a missing acceptance case.

- [x] From host/f407-local-mqtt, run npm test with the existing lockfile dependencies. If node_modules is absent, run npm ci without changing package.json or package-lock.json.
- [x] Start the local dashboard with loopback bind settings and use only a simulated MQTT device/client. Verify state changes, minute sampling, retained snapshot labeling, dedupe, dropped-count delta, matched/unmatched ACK latency, persistence across page reload, and CSV contents.
- [x] Simulate IndexedDB open failure in the browser; confirm the persistence warning appears, MQTT state and controls still render, and bounded in-memory history remains queryable.
- [x] Simulate a browser event-store write rejection; confirm the persistence warning and memory history remain available, then export the fallback history as CSV. The downloaded CSV contains the simulated state, availability, and `write-failed` storage record.
- [x] Confirm history rows older than 24 hours expire, the count never exceeds 5,000, and the visible coverage reports the actual oldest retained row.
- [x] Review the complete diff and worktree status. Do not build firmware, access serial/probe, reset hardware, change Wi-Fi/firewall/ICS, or perform a hardware test.

### Review follow-up: ordering and snapshot compatibility

- [x] Prevent retained snapshots from an earlier page from suppressing the current page's recent-known snapshot.
- [x] Sort same-device/session state and snapshot rows by seq without relying on a non-transitive comparator; calculate pruning and coverage by receivedAt.
- [x] Preserve valid no-seq status transitions and minute samples while disabling remote commands until a valid seq is available.
- [x] Add focused regression tests for retained replay, no-seq transitions, sequence ordering, coverage, and capacity eviction.
- [x] Run the complete offline suite and final diff checks.
