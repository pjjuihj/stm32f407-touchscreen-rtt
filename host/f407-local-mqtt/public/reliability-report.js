(() => {
  'use strict';

  const RESULTS = ['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'];

  function isIPv4(value) {
    return typeof value === 'string' && value.split('.').length === 4 &&
      value.split('.').every((part) => /^\d{1,3}$/.test(part) && Number(part) <= 255);
  }

  function wifiStatus(row) {
    const wifi = row.data?.wifi;
    if (wifi?.sta === 'disconnected') return 'offline';
    if (wifi?.sta === 'connected') return isIPv4(wifi.ip) ? 'online' : 'offline';
    return 'unknown';
  }

  function summarizeChannel(points, fromAt, toAt, maxGapMs) {
    const durationMs = { online: 0, offline: 0 };
    let unknownMs = points.length ? points[0].receivedAt - fromAt +
      toAt - points[points.length - 1].receivedAt : toAt - fromAt;
    let outageCount = 0;
    let completedOutageCount = 0;
    let completedOutageMs = 0;
    let outageMs = null;

    for (let index = 0; index < points.length - 1; index += 1) {
      const previous = points[index];
      const next = points[index + 1];
      const gapMs = next.receivedAt - previous.receivedAt;
      const startsOutage = previous.status === 'online' && next.status === 'offline';
      if (startsOutage) outageCount += 1;
      if (gapMs > maxGapMs || previous.status === 'unknown' || next.status === 'unknown') {
        unknownMs += gapMs;
        outageMs = null;
        continue;
      }

      durationMs[previous.status] += gapMs;
      if (startsOutage) outageMs = 0;
      if (previous.status === 'offline' && outageMs !== null) {
        outageMs += gapMs;
        if (next.status === 'online') {
          completedOutageCount += 1;
          completedOutageMs += outageMs;
          outageMs = null;
        }
      }
    }

    const knownMs = durationMs.online + durationMs.offline;
    return {
      onlineMs: durationMs.online, offlineMs: durationMs.offline, unknownMs, knownMs,
      coveragePercent: knownMs / (toAt - fromAt) * 100,
      onlinePercent: knownMs ? durationMs.online / knownMs * 100 : null,
      outageCount, completedOutageCount, completedOutageMs
    };
  }

  function summarize(events, { now = Date.now(), windowMs, maxGapMs = 120_000 } = {}) {
    if (!Number.isSafeInteger(windowMs) || windowMs <= 0) throw new RangeError('windowMs must be a positive safe integer');
    if (!Number.isSafeInteger(now) || !Number.isSafeInteger(now - windowMs)) throw new RangeError('Invalid time window');
    if (!Number.isSafeInteger(maxGapMs) || maxGapMs < 0) throw new RangeError('maxGapMs must be a nonnegative safe integer');
    const fromAt = now - windowMs;
    const rows = events.filter((row) => row.countsAsLive === true && row.retained !== true &&
      Number.isSafeInteger(row.receivedAt) && row.receivedAt >= fromAt && row.receivedAt <= now)
      .slice().sort((a, b) => a.receivedAt - b.receivedAt);
    const wifi = [];
    const mqtt = [];
    const ack = { receivedCount: 0, resultCounts: Object.fromEntries(RESULTS.map((result) => [result, 0])),
      matchedLatencyCount: 0, latencyP95Ms: null };
    const latencies = [];
    for (const row of rows) {
      if (row.type === 'state') {
        wifi.push({ receivedAt: row.receivedAt, status: wifiStatus(row) });
        mqtt.push({ receivedAt: row.receivedAt, status: 'online' });
      } else if (row.type === 'availability') {
        mqtt.push({ receivedAt: row.receivedAt,
          status: ['online', 'offline'].includes(row.data?.value) ? row.data.value : 'unknown' });
      } else if (row.type === 'ack') {
        ack.receivedCount += 1;
        if (RESULTS.includes(row.data?.result)) ack.resultCounts[row.data.result] += 1;
        if (typeof row.data?.latency_ms === 'number' && Number.isFinite(row.data.latency_ms) && row.data.latency_ms >= 0) {
          latencies.push(row.data.latency_ms);
        }
      }
    }
    latencies.sort((a, b) => a - b);
    ack.matchedLatencyCount = latencies.length;
    if (latencies.length) ack.latencyP95Ms = latencies[Math.ceil(0.95 * latencies.length) - 1];
    return { fromAt, toAt: now,
      wifi: summarizeChannel(wifi, fromAt, now, maxGapMs),
      mqtt: summarizeChannel(mqtt, fromAt, now, maxGapMs), ack };
  }

  globalThis.F407ReliabilityReport = { summarize };
})();
