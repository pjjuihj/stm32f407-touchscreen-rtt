import assert from 'node:assert/strict';
import test from 'node:test';

await import('../public/history-recorder.js');

const { create } = globalThis.F407HistoryRecorder || {};

function state(overrides = {}) {
  return {
    schema_version: 1,
    device_id: 'f407-touchscreen-01',
    session_id: '0123456789abcdef',
    seq: 1,
    uptime_s: 10,
    wifi: { sta: 'connected', ip: '192.168.137.10' },
    bt_connected: false,
    led: [false, false, false],
    buzzer_gpio_high: false,
    dropped_snapshots: 0,
    remote_control_available: true,
    ...overrides
  };
}

function createRecorder(start = 1000) {
  let now = start;
  const events = [];
  const store = { record: (event) => { events.push(structuredClone(event)); } };
  return {
    recorder: create({ store, now: () => now }),
    events,
    advance(ms) { now += ms; },
    setNow(value) { now = value; }
  };
}

test('exports the history recorder API', () => {
  assert.equal(typeof create, 'function');
});

test('records retained state as a snapshot, not a live transition', () => {
  const { recorder, events } = createRecorder();
  recorder.observeState(state(), { retain: true });

  assert.equal(events.length, 1);
  assert.equal(events[0].type, 'state-snapshot');
  assert.equal(events[0].retained, true);
  assert.equal(events[0].countsAsLive, false);
});

test('records the current retained state when the same snapshot exists from an earlier page', async () => {
  const previous = state({ seq: 4 });
  const storedSnapshot = {
    type: 'state-snapshot',
    device_id: previous.device_id,
    session_id: previous.session_id,
    seq: previous.seq,
    receivedAt: 1000,
    retained: true,
    countsAsLive: false,
    data: {
      uptime_s: previous.uptime_s,
      wifi: previous.wifi,
      bt_connected: previous.bt_connected,
      led: previous.led,
      buzzer_gpio_high: previous.buzzer_gpio_high,
      dropped_snapshots: previous.dropped_snapshots,
      remote_control_available: previous.remote_control_available
    }
  };
  const events = [];
  const recorder = create({
    store: { list: async () => [storedSnapshot], record: (event) => events.push(structuredClone(event)) },
    now: () => 2000
  });
  recorder.observeState(previous, { retain: true });
  await recorder.ready;

  assert.equal(events.length, 1);
  assert.equal(events[0].type, 'state-snapshot');
  assert.notEqual(events[0].dedupeKey, undefined);
});

test('deduplicates QoS redelivery and samples stable live state at most once per minute', () => {
  const { recorder, events, advance } = createRecorder();
  recorder.observeState(state(), { retain: false });
  recorder.observeState(state(), { retain: false });
  advance(30000);
  recorder.observeState(state({ seq: 2, uptime_s: 40 }), { retain: false });
  advance(30000);
  recorder.observeState(state({ seq: 7, uptime_s: 70 }), { retain: false });
  advance(30000);
  recorder.observeState(state({ seq: 8, uptime_s: 100 }), { retain: false });

  const states = events.filter((event) => event.type === 'state');
  assert.equal(states.length, 2);
  assert.equal(states[0].reason, 'initial');
  assert.equal(states[1].reason, 'sample');
});

test('records meaningful state changes immediately and combines a coincident sample', () => {
  const { recorder, events, advance } = createRecorder();
  recorder.observeState(state(), { retain: false });
  advance(20000);
  recorder.observeState(state({ seq: 2, wifi: { sta: 'disconnected', ip: null } }), { retain: false });
  advance(60000);
  recorder.observeState(state({ seq: 3, wifi: { sta: 'disconnected', ip: null }, bt_connected: true }), { retain: false });

  assert.deepEqual(events.filter((event) => event.type === 'state').map((event) => event.reason), [
    'initial', 'change', 'change+sample'
  ]);
});

test('live confirmation of a retained snapshot enters live history', () => {
  const { recorder, events, advance } = createRecorder();
  const snapshot = state({ seq: 9 });
  recorder.observeState(snapshot, { retain: true });
  advance(1000);
  recorder.observeState(snapshot, { retain: false });

  assert.deepEqual(events.map((event) => [event.type, event.countsAsLive]), [
    ['state-snapshot', false], ['state', true]
  ]);
});

test('deduplicates repeated retained snapshots by normalized state fingerprint', () => {
  const { recorder, events } = createRecorder();
  recorder.observeState(state({ seq: 1, uptime_s: 10 }), { retain: true });
  recorder.observeState(state({ seq: 2, uptime_s: 20 }), { retain: true });
  assert.equal(events.length, 1);
});

test('assigns a fresh dedupe key to a retained snapshot after live activity', () => {
  const { recorder, events } = createRecorder();
  const snapshot = state({ seq: 9 });
  recorder.observeState(snapshot, { retain: true });
  recorder.observeState(snapshot, { retain: false });
  recorder.observeState(snapshot, { retain: true });

  const snapshots = events.filter((event) => event.type === 'state-snapshot');
  assert.equal(snapshots.length, 2);
  assert.notEqual(snapshots[0].dedupeKey, snapshots[1].dedupeKey);
});

test('resets dropped-snapshot baseline on a new session and records only positive deltas', () => {
  const { recorder, events } = createRecorder();
  recorder.observeState(state({ dropped_snapshots: 5 }), { retain: false });
  recorder.observeState(state({ seq: 2, dropped_snapshots: 2 }), { retain: false });
  recorder.observeState(state({ seq: 3, dropped_snapshots: 4 }), { retain: false });
  recorder.observeState(state({ session_id: 'fedcba9876543210', seq: 1, dropped_snapshots: 0 }), { retain: false });
  recorder.observeState(state({ session_id: 'fedcba9876543210', seq: 2, dropped_snapshots: 3 }), { retain: false });

  assert.deepEqual(events.filter((event) => event.data?.droppedDelta)
    .map((event) => [event.session_id, event.data.droppedDelta]), [
    ['0123456789abcdef', 2], ['fedcba9876543210', 3]
  ]);
});

test('records only live availability transitions and labels retained snapshots separately', () => {
  const { recorder, events } = createRecorder();
  recorder.observeAvailability('online', { retain: true });
  recorder.observeAvailability('online', { retain: true });
  recorder.observeAvailability('online', { retain: false });
  recorder.observeAvailability('online', { retain: false });
  recorder.observeAvailability('offline', { retain: false });

  assert.deepEqual(events.map((event) => [event.type, event.data.value, event.retained, event.countsAsLive]), [
    ['availability', 'online', true, false],
    ['availability', 'online', false, true],
    ['availability', 'offline', false, true]
  ]);
});

test('a retained conflicting availability snapshot does not hide the next live transition', () => {
  const { recorder, events } = createRecorder();
  recorder.observeAvailability('online', { retain: false });
  recorder.observeAvailability('offline', { retain: true });
  recorder.observeAvailability('online', { retain: false });

  assert.deepEqual(events.map((event) => [event.data.value, event.retained, event.countsAsLive]), [
    ['online', false, true],
    ['offline', true, false],
    ['online', false, true]
  ]);
});

test('records a retained availability value again after a live transition', () => {
  const { recorder, events } = createRecorder();
  recorder.observeAvailability('offline', { retain: true });
  recorder.observeAvailability('online', { retain: false });
  recorder.observeAvailability('offline', { retain: true });

  assert.deepEqual(events.map((event) => [event.data.value, event.retained]), [
    ['offline', true], ['online', false], ['offline', true]
  ]);
});

test('deduplicates ACKs and only includes latency for a matching session and command', () => {
  const { recorder, events } = createRecorder();
  const ack = {
    schema_version: 1,
    device_id: 'f407-touchscreen-01',
    session_id: '0123456789abcdef',
    command_id: 'fedcba9876543210',
    command: 'led.set',
    result: 'OK',
    readback: { led: [true, false, false], buzzer_gpio_high: false },
    password: 'must-not-be-stored'
  };
  recorder.observeAck(ack, { matched: true, latencyMs: 42.5, receivedAt: 1200 });
  recorder.observeAck(ack, { matched: true, latencyMs: 42.5, receivedAt: 1201 });
  recorder.observeAck({ ...ack, command_id: '1111111111111111' }, {
    matched: false, latencyMs: 999, receivedAt: 1300
  });

  const acks = events.filter((event) => event.type === 'ack');
  assert.equal(acks.length, 2);
  assert.equal(acks[0].data.latency_ms, 42.5);
  assert.equal(acks[1].data.latency_ms, null);
  assert.equal(JSON.stringify(acks).includes('must-not-be-stored'), false);
});

test('records one sanitized storage event on transition to memory-only history', () => {
  const { recorder, events } = createRecorder();
  recorder.observeStorage({ persistent: true });
  recorder.observeStorage({ persistent: false, reason: 'write-failed', error: 'secret stack' });
  recorder.observeStorage({ persistent: false, reason: 'write-failed', error: 'secret stack' });

  assert.equal(events.length, 1);
  assert.equal(events[0].type, 'storage');
  assert.equal(JSON.stringify(events).includes('secret stack'), false);
});

test('hydrates the last live state so a browser reload does not log unchanged state as initial', async () => {
  let now = 30000;
  const previous = state({ seq: 20, uptime_s: 20 });
  const storedState = {
    type: 'state',
    device_id: previous.device_id,
    session_id: previous.session_id,
    seq: previous.seq,
    receivedAt: 1000,
    retained: false,
    countsAsLive: true,
    data: {
      uptime_s: previous.uptime_s,
      wifi: previous.wifi,
      bt_connected: previous.bt_connected,
      led: previous.led,
      buzzer_gpio_high: previous.buzzer_gpio_high,
      dropped_snapshots: previous.dropped_snapshots,
      remote_control_available: previous.remote_control_available
    }
  };
  const events = [];
  const store = {
    list: async () => [storedState],
    record: (event) => { events.push(structuredClone(event)); }
  };
  const recorder = create({ store, now: () => now });
  recorder.observeState(state({ seq: 21, uptime_s: 21 }), { retain: false });
  await recorder.ready;
  assert.equal(events.length, 0);

  now = 61000;
  recorder.observeState(state({ seq: 22, uptime_s: 61 }), { retain: false });
  assert.deepEqual(events.map((event) => event.reason), ['sample']);
});

test('records the current retained availability but hydrates ACK keys to avoid duplicates', async () => {
  const ack = {
    type: 'ack',
    session_id: '0123456789abcdef',
    receivedAt: 1000,
    retained: false,
    countsAsLive: true,
    data: { command_id: 'fedcba9876543210', command: 'led.set', result: 'OK', latency_ms: 10 }
  };
  let recordCalls = 0;
  const store = {
    list: async () => [
      { type: 'availability', receivedAt: 900, retained: true, data: { value: 'online' } },
      ack
    ],
    record() { recordCalls += 1; }
  };
  const recorder = create({ store, now: () => 2000 });
  recorder.observeAvailability('online', { retain: true });
  recorder.observeAck({
    session_id: ack.session_id,
    command_id: ack.data.command_id,
    command: 'led.set',
    result: 'OK'
  });
  await recorder.ready;
  assert.equal(recordCalls, 1);
});

test('records seq-less states with a short dedupe window and minute samples', () => {
  const { recorder, events, advance } = createRecorder();
  const seqLess = state({ seq: undefined });
  delete seqLess.seq;

  recorder.observeState(seqLess, { retain: false });
  advance(1000);
  recorder.observeState({ ...seqLess }, { retain: false });
  assert.equal(events.filter((event) => event.type === 'state').length, 1);

  advance(59000);
  recorder.observeState({ ...seqLess, uptime_s: 70 }, { retain: false });
  const states = events.filter((event) => event.type === 'state');
  assert.deepEqual(states.map((event) => event.reason), ['initial', 'sample']);
  assert.equal(Object.hasOwn(states[0], 'seq'), false);
  assert.equal(Object.hasOwn(states[0], 'dedupeKey'), false);
});

test('records a seq-less state reversal even when the earlier fingerprint is recent', () => {
  const { recorder, events, advance } = createRecorder();
  const initial = state();
  delete initial.seq;
  const disconnected = {
    ...initial,
    wifi: { sta: 'disconnected', ip: null }
  };

  recorder.observeState(initial, { retain: false });
  advance(500);
  recorder.observeState(disconnected, { retain: false });
  advance(500);
  recorder.observeState(initial, { retain: false });

  assert.deepEqual(events.filter((event) => event.type === 'state').map((event) => event.reason), [
    'initial', 'change', 'change'
  ]);
});

test('starts a new local dedupe epoch when the all-zero session sequence and uptime reset', () => {
  const { recorder, events, advance } = createRecorder();
  const sentinel = state({
    session_id: '0000000000000000',
    remote_control_available: false,
    seq: 1,
    uptime_s: 1
  });

  recorder.observeState(sentinel, { retain: false });
  advance(10000);
  recorder.observeState({ ...sentinel, seq: 7, uptime_s: 11 }, { retain: false });
  advance(10000);
  recorder.observeState(sentinel, { retain: false });
  recorder.observeState(sentinel, { retain: false });

  const states = events.filter((event) => event.type === 'state');
  assert.equal(states.length, 2);
  assert.equal(states[0].reason, 'initial');
  assert.equal(states[1].reason, 'change');
  assert.equal(Object.hasOwn(states[0], 'dedupeKey'), false);
  assert.equal(Object.hasOwn(states[1], 'dedupeKey'), false);
});

test('hydrates sentinel sequence state to recognize a reboot without reusing persistent dedupe keys', async () => {
  let now = 1000;
  const stored = [];
  const store = {
    list: async () => stored,
    record: (event) => { stored.push(structuredClone(event)); }
  };
  const firstRun = create({ store, now: () => now });
  await firstRun.ready;
  firstRun.observeState(state({
    session_id: '0000000000000000',
    remote_control_available: false,
    seq: 7,
    uptime_s: 70
  }), { retain: false });

  now += 1000;
  const afterRestart = create({ store, now: () => now });
  await afterRestart.ready;
  afterRestart.observeState(state({
    session_id: '0000000000000000',
    remote_control_available: false,
    seq: 1,
    uptime_s: 1
  }), { retain: false });

  assert.equal(stored.filter((event) => event.type === 'state').length, 2);
  assert.equal(stored.at(-1).reason, 'change');
  assert.equal(Object.hasOwn(stored.at(-1), 'dedupeKey'), false);
});

test('does not let a legacy all-zero-session dedupe key hide a changed state with reused seq and uptime', async () => {
  let now = 1000;
  const stored = [];
  const store = {
    list: async () => stored,
    record: (event) => { stored.push(structuredClone(event)); }
  };
  const firstRun = create({ store, now: () => now });
  await firstRun.ready;
  firstRun.observeState(state({
    session_id: '0000000000000000',
    remote_control_available: false,
    seq: 1,
    uptime_s: 1
  }), { retain: false });
  stored[0].dedupeKey = 'state:f407-touchscreen-01:0000000000000000:1:1';

  now += 1000;
  const afterRestart = create({ store, now: () => now });
  await afterRestart.ready;
  afterRestart.observeState(state({
    session_id: '0000000000000000',
    remote_control_available: false,
    seq: 1,
    uptime_s: 1,
    wifi: { sta: 'disconnected', ip: null }
  }), { retain: false });

  assert.equal(stored.filter((event) => event.type === 'state').length, 2);
  assert.equal(stored.at(-1).reason, 'change');
  assert.equal(Object.hasOwn(stored.at(-1), 'dedupeKey'), false);
});
