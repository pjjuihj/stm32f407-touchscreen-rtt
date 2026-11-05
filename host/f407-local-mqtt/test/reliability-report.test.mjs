import test from 'node:test';
import assert from 'node:assert/strict';
import '../public/reliability-report.js';

const { summarize } = globalThis.F407ReliabilityReport;
function stateRow(receivedAt, sta, ip, { retained = false, countsAsLive = true } = {}) {
  return { type: retained ? 'state-snapshot' : 'state', receivedAt, retained,
    countsAsLive: countsAsLive && !retained, data: { wifi: { sta, ip } } };
}
function availabilityRow(receivedAt, value, { retained = false } = {}) {
  return { type: 'availability', receivedAt, retained, countsAsLive: !retained, data: { value } };
}
const online = (at, options) => stateRow(at, 'connected', '192.168.137.20', options);
const offline = (at) => stateRow(at, 'disconnected', null);
const options = { now: 180_000, windowMs: 180_000 };

test('reports only observed time and closes a fully observed outage', () => {
  const report = summarize([online(0), offline(60_000), online(120_000)], options);
  assert.deepEqual(report.wifi, { onlineMs: 60_000, offlineMs: 60_000, unknownMs: 60_000,
    knownMs: 120_000, coveragePercent: 120_000 / 180_000 * 100, onlinePercent: 50,
    outageCount: 1, completedOutageCount: 1, completedOutageMs: 60_000 });
});

for (const gap of [119_999, 120_000, 120_001]) {
  test(`gap boundary ${gap}`, () => {
    for (const make of [online, offline]) {
      const channel = summarize([make(0), make(gap)], { now: gap, windowMs: gap }).wifi;
      assert.equal(channel.knownMs, gap <= 120_000 ? gap : 0);
      assert.equal(channel.unknownMs, gap > 120_000 ? gap : 0);
    }
  });
}

test('unknown endpoints make the entire interval unknown', () => {
  const wifi = summarize([online(0), stateRow(60_000, 'unknown', null), offline(120_000)], options).wifi;
  assert.equal(wifi.onlineMs, 0);
  assert.equal(wifi.offlineMs, 0);
  assert.equal(wifi.unknownMs, 180_000);
  assert.equal(wifi.onlinePercent, null);
});

test('invalid IP is not ready', () => {
  for (const ip of [null, '', '256.1.1.1', '1.2.3', '1.2.3.x']) {
    const wifi = summarize([stateRow(0, 'connected', ip), online(60_000)], options).wifi;
    assert.equal(wifi.offlineMs, 60_000);
    assert.equal(wifi.completedOutageCount, 0);
  }
});

test('window edges, retained and non-live rows never extend observations', () => {
  const rows = [online(-1), online(30_000), online(90_000), online(180_001),
    online(180_000, { retained: true }), online(160_000, { countsAsLive: false })];
  const wifi = summarize(rows, options).wifi;
  assert.equal(wifi.onlineMs, 60_000);
  assert.equal(wifi.unknownMs, 120_000);
  const snapshot = summarize([online(0, { retained: true })], options);
  assert.equal(snapshot.wifi.unknownMs, 180_000);
  assert.equal(snapshot.mqtt.knownMs, 0);
});

test('only fully bounded outage episodes have durations', () => {
  assert.equal(summarize([offline(0), online(60_000)], options).wifi.completedOutageCount, 0);
  const open = summarize([online(0), offline(60_000), offline(120_000)], options).wifi;
  assert.equal(open.outageCount, 1);
  assert.equal(open.completedOutageCount, 0);
  const broken = summarize([online(0), offline(1), online(120_002)], options).wifi;
  assert.equal(broken.outageCount, 1);
  assert.equal(broken.completedOutageMs, 0);
});

test('unknown interrupts an episode and later complete episodes still accumulate', () => {
  const wifi = summarize([online(0), offline(10), stateRow(20, 'unknown', null),
    offline(30), online(40), offline(50), offline(60), online(70), offline(80), online(90)],
  { now: 100, windowMs: 100 }).wifi;
  assert.equal(wifi.outageCount, 3);
  assert.equal(wifi.completedOutageCount, 2);
  assert.equal(wifi.completedOutageMs, 30);
  assert.equal(wifi.onlineMs + wifi.offlineMs + wifi.unknownMs, 100);
});

test('MQTT uses live state and availability independently of Wi-Fi', () => {
  const rows = [offline(0), availabilityRow(60_000, 'offline'),
    availabilityRow(90_000, 'online', { retained: true }), online(120_000)];
  const report = summarize(rows, options);
  assert.equal(report.mqtt.onlineMs, 60_000);
  assert.equal(report.mqtt.offlineMs, 60_000);
  assert.equal(report.mqtt.completedOutageMs, 60_000);
  const broken = summarize([availabilityRow(0, 'online'), availabilityRow(1, 'offline'),
    availabilityRow(120_002, 'online')], options).mqtt;
  assert.equal(broken.completedOutageCount, 0);
});

test('ACK counts all six results and nearest-rank numeric P95 in window only', () => {
  const results = ['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'];
  const ack = (receivedAt, result, latency_ms) => ({ type: 'ack', receivedAt,
    retained: false, countsAsLive: true, data: { result, latency_ms } });
  const rows = Array.from({ length: 20 }, (_, i) => ack(i, 'OK', i + 1));
  rows.push(...results.map((result) => ack(30, result, null)), ack(-1, 'ERROR', 999), ack(101, 'ERROR', 999));
  const report = summarize(rows, { now: 100, windowMs: 100 });
  assert.equal(report.ack.receivedCount, 26);
  assert.deepEqual(report.ack.resultCounts, { OK: 21, INVALID: 1, INVALID_PARAM: 1, RATE_LIMITED: 1, ERROR: 1, STALE_SESSION: 1 });
  assert.equal(report.ack.matchedLatencyCount, 20);
  assert.equal(report.ack.latencyP95Ms, 19);
  assert.equal(summarize([], options).ack.latencyP95Ms, null);
});

test('sorts a copy, pins the gap policy, and validates options', () => {
  const rows = [online(120_000), offline(60_000), online(0)];
  const original = structuredClone(rows);
  assert.equal(summarize(rows, options).wifi.completedOutageCount, 1);
  assert.deepEqual(rows, original);
  assert.equal(summarize(rows, { ...options, maxGapMs: 30_000 }).wifi.knownMs, 120_000);
  for (const windowMs of [0, -1, NaN, 1.5]) assert.throws(() => summarize([], { windowMs }), RangeError);
});
