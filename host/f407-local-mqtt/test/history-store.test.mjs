import assert from 'node:assert/strict';
import test from 'node:test';

await import('../public/history-store.js');

const { create, toCsv, escapeCsvField } = globalThis.F407HistoryStore || {};
const HOUR = 60 * 60 * 1000;

function createMemoryBackend({ failOpen = false, failPut = false } = {}) {
  const events = new Map();
  const metadata = new Map();
  return {
    async open() {
      if (failOpen) throw new Error('private path and exception must not escape');
    },
    async getAll() { return [...events.values()].map((event) => structuredClone(event)); },
    async put(event) {
      if (failPut) throw new Error('write failed');
      if (event.dedupeKey && [...events.values()].some((stored) => stored.dedupeKey === event.dedupeKey)) {
        return false;
      }
      events.set(event.id, structuredClone(event));
      return true;
    },
    async delete(ids) { for (const id of ids) events.delete(id); },
    async getMeta(key) { return metadata.get(key) ?? null; },
    async putMeta(key, value) { metadata.set(key, value); }
  };
}

function stateEvent(receivedAt, seq, extra = {}) {
  return {
    type: 'state',
    device_id: 'f407-touchscreen-01',
    receivedAt,
    session_id: '0123456789abcdef',
    seq,
    reason: 'change',
    data: {
      uptime_s: seq,
      wifi: { sta: 'connected', ip: '192.168.137.20' },
      bt_connected: false,
      led: [false, true, false],
      buzzer_gpio_high: false,
      dropped_snapshots: 0,
      remote_control_available: true,
      password: 'must-not-be-stored',
      rawPayload: 'must-not-be-stored'
    },
    ...extra
  };
}

test('exports the history store API', () => {
  assert.equal(typeof create, 'function');
  assert.equal(typeof toCsv, 'function');
  assert.equal(typeof escapeCsvField, 'function');
});

test('expires events older than 24 hours without counting them as capacity drops', async () => {
  let now = 30 * HOUR;
  const backend = createMemoryBackend();
  const history = create({ backend, now: () => now });
  await history.ready;

  await history.record(stateEvent(now - 24 * HOUR - 1, 1));
  await history.record(stateEvent(now - 24 * HOUR + 1, 2));
  const rows = await history.list();
  const status = await history.getStatus();

  assert.equal(rows.length, 1);
  assert.equal(rows[0].seq, 2);
  assert.equal(status.capacityDropped, 0);
  assert.equal(status.oldestAt, now - 24 * HOUR + 1);
});

test('keeps the newest 5000 rows and counts capacity evictions', async () => {
  let now = 1;
  const history = create({ backend: createMemoryBackend(), now: () => now });
  await history.ready;
  for (let index = 0; index < 5001; index += 1) {
    await history.record(stateEvent(now, index + 1));
    now += 1;
  }

  const rows = await history.list();
  assert.equal(rows.length, 5000);
  assert.equal(rows[0].seq, 2);
  assert.equal((await history.getStatus()).capacityDropped, 1);
});

test('lists rows in ascending receive-time order and reports actual coverage', async () => {
  let now = 3000;
  const history = create({ backend: createMemoryBackend(), now: () => now });
  await history.ready;
  await history.record(stateEvent(3000, 3));
  await history.record(stateEvent(1000, 1));
  await history.record(stateEvent(2000, 2));

  assert.deepEqual((await history.list()).map((row) => row.seq), [1, 2, 3]);
  assert.equal((await history.getStatus()).coverageMs, 2000);
});

test('orders same-session state rows by device sequence despite receive-time reordering', async () => {
  const history = create({ backend: createMemoryBackend(), now: () => 4000 });
  await history.ready;
  await history.record(stateEvent(3000, 1));
  await history.record(stateEvent(1000, 2));
  await history.record(stateEvent(2000, 3));

  assert.deepEqual((await history.list()).map((row) => row.seq), [1, 2, 3]);
  assert.equal((await history.getStatus()).oldestAt, 1000);
  assert.equal((await history.getStatus()).coverageMs, 2000);
});

test('evicts the oldest received row at capacity even when sequence ordering differs', async () => {
  const history = create({ backend: createMemoryBackend(), now: () => 6000 });
  await history.ready;
  for (let index = 0; index < 5001; index += 1) {
    await history.record(stateEvent(index + 1, 5001 - index));
  }

  const rows = await history.list();
  assert.equal(rows.length, 5000);
  assert.equal(rows.some((row) => row.receivedAt === 1), false);
  assert.equal((await history.getStatus()).capacityDropped, 1);
});

test('falls back to bounded memory when IndexedDB is unavailable and reports no exception details', async () => {
  const statuses = [];
  const history = create({
    backend: createMemoryBackend({ failOpen: true }),
    now: () => 100,
    onStatus: (status) => statuses.push(status)
  });
  await history.ready;
  await history.record(stateEvent(100, 1));

  assert.equal((await history.getStatus()).persistent, false);
  assert.equal((await history.list()).length, 1);
  assert.equal(JSON.stringify(statuses).includes('private path'), false);
  assert.ok(statuses.some((status) => status.persistent === false));
});

test('switches to memory fallback after a write failure without throwing', async () => {
  const history = create({ backend: createMemoryBackend({ failPut: true }), now: () => 100 });
  await history.ready;
  await assert.doesNotReject(history.record(stateEvent(100, 1)));
  assert.equal((await history.getStatus()).persistent, false);
  assert.equal((await history.list()).length, 1);
});

test('persists only the selected status and acknowledgement fields', async () => {
  const history = create({ backend: createMemoryBackend(), now: () => 100 });
  await history.ready;
  await history.record(stateEvent(100, 1));
  const serialized = JSON.stringify(await history.list());
  assert.equal(serialized.includes('must-not-be-stored'), false);
  assert.equal(serialized.includes('rawPayload'), false);
  assert.equal(serialized.includes('password'), false);
});

test('keeps dedupe keys across store instances so reloads do not duplicate retained events', async () => {
  const backend = createMemoryBackend();
  const firstPage = create({ backend, now: () => 100 });
  await firstPage.ready;
  await firstPage.record({ ...stateEvent(100, 1), dedupeKey: 'state:f407-touchscreen-01:0123456789abcdef:1' });

  const reloadedPage = create({ backend, now: () => 200 });
  await reloadedPage.ready;
  const accepted = await reloadedPage.record({ ...stateEvent(200, 1), dedupeKey: 'state:f407-touchscreen-01:0123456789abcdef:1' });

  assert.equal(accepted, false);
  assert.equal((await reloadedPage.list()).length, 1);
});

test('atomically rejects the same dedupe key written by two page instances', async () => {
  const backend = createMemoryBackend();
  const firstPage = create({ backend, now: () => 100 });
  const secondPage = create({ backend, now: () => 100 });
  await Promise.all([firstPage.ready, secondPage.ready]);
  const event = { ...stateEvent(100, 1), dedupeKey: 'state:f407-touchscreen-01:0123456789abcdef:1' };

  const accepted = await Promise.all([
    firstPage.record(event),
    secondPage.record(event)
  ]);
  const reloadedPage = create({ backend, now: () => 200 });
  await reloadedPage.ready;

  assert.deepEqual(accepted.sort(), [false, true]);
  assert.equal((await reloadedPage.list()).length, 1);
});

test('escapes commas, quotes, and line breaks in CSV cells', () => {
  assert.equal(escapeCsvField('plain'), 'plain');
  assert.equal(escapeCsvField('a,b'), '"a,b"');
  assert.equal(escapeCsvField('say "hi"'), '"say ""hi"""');
  assert.equal(escapeCsvField('first\nsecond'), '"first\nsecond"');
  const csv = toCsv([{
    type: 'state',
    device_id: 'f407-touchscreen-01',
    receivedAt: 100,
    session_id: '0123456789abcdef',
    seq: 1,
    reason: 'change',
    data: { led: [false, true, false] }
  }]);
  assert.match(csv, /^type,device_id,received_at_utc,session_id,seq,retained,reason,data\r?\n/);
  assert.match(csv, /"\{""led"":\[false,true,false\]\}"/);
});
