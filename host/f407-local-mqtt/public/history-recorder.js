(() => {
  'use strict';

  const MAX_DEDUPE_KEYS = 6000;
  const MAX_SNAPSHOT_KEYS = 500;
  const SAMPLE_INTERVAL_MS = 60 * 1000;
  const NO_SEQ_DEDUPE_WINDOW_MS = 2000;

  function boundedRemember(set, queue, key, limit) {
    if (set.has(key)) return false;
    set.add(key);
    queue.push(key);
    while (queue.length > limit) set.delete(queue.shift());
    return true;
  }

  function projectState(state) {
    return {
      uptime_s: state.uptime_s,
      wifi: {
        sta: state.wifi.sta,
        ip: state.wifi.ip
      },
      bt_connected: state.bt_connected,
      led: state.led.slice(),
      buzzer_gpio_high: state.buzzer_gpio_high,
      dropped_snapshots: state.dropped_snapshots,
      remote_control_available: state.remote_control_available
    };
  }

  function stateFingerprint(state) {
    return JSON.stringify({
      session_id: state.session_id.toLowerCase(),
      wifi: state.wifi,
      bt_connected: state.bt_connected,
      led: state.led,
      buzzer_gpio_high: state.buzzer_gpio_high,
      dropped_snapshots: state.dropped_snapshots,
      remote_control_available: state.remote_control_available
    });
  }

  function fingerprintHash(value) {
    let hash = 2166136261;
    for (let index = 0; index < value.length; index += 1) {
      hash ^= value.charCodeAt(index);
      hash = Math.imul(hash, 16777619);
    }
    return (hash >>> 0).toString(16).padStart(8, '0');
  }

  function safeReadback(readback) {
    if (!readback || typeof readback !== 'object' || Array.isArray(readback)) return undefined;
    const result = {};
    if (Array.isArray(readback.led) && readback.led.length === 3 &&
        readback.led.every((value) => typeof value === 'boolean')) {
      result.led = readback.led.slice();
    }
    if (typeof readback.buzzer_gpio_high === 'boolean') {
      result.buzzer_gpio_high = readback.buzzer_gpio_high;
    }
    return Object.keys(result).length ? result : undefined;
  }

  function create({ store, now = Date.now } = {}) {
    const pageInstanceId = now().toString(36) + '-' + Math.random().toString(36).slice(2, 10);
    const liveStateKeys = new Set();
    const liveStateQueue = [];
    const snapshotKeys = new Set();
    const snapshotQueue = [];
    const ackKeys = new Set();
    const ackQueue = [];
    let lastLiveState = null;
    let lastStateRecordAt = null;
    let lastUnsequencedFingerprint = null;
    let lastUnsequencedAt = null;
    let lastLiveAvailability = null;
    let lastRetainedAvailability = null;
    let availabilitySnapshotGeneration = 0;
    let stateSnapshotGeneration = 0;
    let storageFallbackRecorded = false;
    const droppedBySession = new Map();
    const sentinelBoots = new Map();
    let hydrated = !(store && typeof store.list === 'function');
    const pendingObservations = [];

    function updateSentinelBoot(state, receivedAt) {
      const deviceId = typeof state.device_id === 'string' ? state.device_id : '';
      const seq = Number.isSafeInteger(state.seq) && state.seq > 0 ? state.seq : null;
      const uptime = Number.isSafeInteger(state.uptime_s) ? state.uptime_s : null;
      let boot = sentinelBoots.get(deviceId);
      let restarted = false;
      if (!boot) {
        boot = {
          seq,
          uptime,
          receivedAt
        };
      } else {
        const sequenceRegressed = seq !== null && boot.seq !== null && seq < boot.seq;
        const uptimeRegressed = uptime !== null && boot.uptime !== null && uptime < boot.uptime;
        restarted = sequenceRegressed || uptimeRegressed;
        if (restarted) {
          boot = {
            seq,
            uptime,
            receivedAt
          };
        } else {
          if (seq !== null && (boot.seq === null || seq >= boot.seq)) boot.seq = seq;
          if (uptime !== null && (boot.uptime === null || uptime >= boot.uptime)) boot.uptime = uptime;
          boot.receivedAt = Math.max(boot.receivedAt, receivedAt);
        }
      }
      sentinelBoots.set(deviceId, boot);
      return { restarted };
    }

    function dispatchObservation(observation) {
      if (hydrated) {
        observation();
        return;
      }
      if (pendingObservations.length >= 128) pendingObservations.shift();
      pendingObservations.push(observation);
    }

    function persist(event) {
      if (!store || typeof store.record !== 'function') return;
      try {
        const pending = store.record(event);
        if (pending && typeof pending.catch === 'function') pending.catch(() => {});
      } catch {
        // A history write must never interrupt MQTT message handling.
      }
    }

    function processState(state, packet = {}) {
      if (!state || typeof state !== 'object' || !state.wifi || !Array.isArray(state.led)) return;
      const receivedAt = Number.isSafeInteger(packet.receivedAt) ? packet.receivedAt : now();
      const data = projectState(state);
      const sessionId = String(state.session_id || '').toLowerCase();
      const deviceId = typeof state.device_id === 'string' ? state.device_id : undefined;

      if (packet.retain === true) {
        const fingerprint = deviceId + '|' + stateFingerprint(state);
        if (!boundedRemember(snapshotKeys, snapshotQueue, fingerprint, MAX_SNAPSHOT_KEYS)) return;
        persist({
          type: 'state-snapshot',
          device_id: deviceId,
          session_id: sessionId,
          ...(Number.isSafeInteger(state.seq) && state.seq > 0 ? { seq: state.seq } : {}),
          receivedAt,
          retained: true,
          countsAsLive: false,
          reason: lastLiveState ? 'change' : 'initial',
          dedupeKey: 'snapshot:' + pageInstanceId + ':' + stateSnapshotGeneration + ':' +
            deviceId + ':' + fingerprintHash(fingerprint),
          data
        });
        return;
      }

      const sentinelSession = sessionId === '0000000000000000';
      const sentinelBoot = sentinelSession ? updateSentinelBoot(state, receivedAt) : null;
      const hasSequence = Number.isSafeInteger(state.seq) && state.seq > 0;
      const stateKey = deviceId + '|' + sessionId + '|' +
        (hasSequence ? String(state.seq) : 'fingerprint:' + fingerprintHash(stateFingerprint(state)));
      if (hasSequence && !sentinelSession) {
        if (!boundedRemember(liveStateKeys, liveStateQueue, stateKey, MAX_DEDUPE_KEYS)) return;
      }
      snapshotKeys.clear();
      snapshotQueue.length = 0;
      stateSnapshotGeneration += 1;

      const previous = lastLiveState;
      const isFirst = previous === null;
      const sessionChanged = Boolean(previous &&
        String(previous.session_id).toLowerCase() !== sessionId);
      const changed = isFirst || sessionChanged || sentinelBoot?.restarted === true ||
        stateFingerprint(previous) !== stateFingerprint(state);
      const sampleDue = isFirst || lastStateRecordAt === null ||
        receivedAt - lastStateRecordAt >= SAMPLE_INTERVAL_MS;
      const reason = isFirst
        ? 'initial'
        : changed && sampleDue
          ? 'change+sample'
          : changed
            ? 'change'
            : sampleDue
              ? 'sample'
              : '';

      if (!hasSequence) {
        const fingerprint = stateFingerprint(state);
        const repeated = fingerprint === lastUnsequencedFingerprint &&
          lastUnsequencedAt !== null && receivedAt >= lastUnsequencedAt &&
          receivedAt - lastUnsequencedAt < NO_SEQ_DEDUPE_WINDOW_MS;
        if (repeated && !changed && !sampleDue) return;
        lastUnsequencedFingerprint = fingerprint;
        lastUnsequencedAt = receivedAt;
      } else {
        lastUnsequencedFingerprint = null;
        lastUnsequencedAt = null;
      }

      let droppedDelta;
      if (sessionChanged || sentinelBoot?.restarted === true) {
        droppedBySession.delete(sessionId);
      }
      const previousDropped = droppedBySession.get(sessionId);
      if (previousDropped !== undefined && state.dropped_snapshots > previousDropped) {
        droppedDelta = state.dropped_snapshots - previousDropped;
      }
      droppedBySession.set(sessionId, state.dropped_snapshots);
      while (droppedBySession.size > 8) droppedBySession.delete(droppedBySession.keys().next().value);

      if (reason) {
        if (droppedDelta !== undefined) data.droppedDelta = droppedDelta;
        persist({
          type: 'state',
          device_id: deviceId,
          session_id: sessionId,
          ...(hasSequence ? { seq: state.seq } : {}),
          receivedAt,
          retained: false,
          countsAsLive: true,
          reason,
          ...(hasSequence && !sentinelSession ? {
            dedupeKey: 'state:' + deviceId + ':' + sessionId + ':' + String(state.seq)
          } : {}),
          data
        });
        lastStateRecordAt = receivedAt;
      }
      lastLiveState = state;
    }

    function processAvailability(value, packet = {}) {
      if (value !== 'online' && value !== 'offline') return;
      const receivedAt = Number.isSafeInteger(packet.receivedAt) ? packet.receivedAt : now();
      if (packet.retain === true) {
        if (lastRetainedAvailability === value) return;
        lastRetainedAvailability = value;
        if (lastLiveAvailability !== null && lastLiveAvailability !== value) {
          lastLiveAvailability = null;
        }
        persist({
          type: 'availability',
          receivedAt,
          retained: true,
          countsAsLive: false,
          reason: 'availability',
          dedupeKey: 'availability-snapshot:' + pageInstanceId + ':' +
            availabilitySnapshotGeneration + ':' + value,
          data: { value }
        });
        return;
      }
      lastRetainedAvailability = null;
      availabilitySnapshotGeneration += 1;
      if (lastLiveAvailability === value) return;
      lastLiveAvailability = value;
      persist({
        type: 'availability',
        receivedAt,
        retained: false,
        countsAsLive: true,
        reason: 'availability',
        data: { value }
      });
    }

    function processAck(ack, options = {}) {
      if (!ack || typeof ack !== 'object') return;
      const sessionId = typeof ack.session_id === 'string' ? ack.session_id.toLowerCase() : '';
      const commandId = typeof ack.command_id === 'string' ? ack.command_id.toLowerCase() : '';
      if (!sessionId || !commandId) return;
      const key = sessionId + '|' + commandId;
      if (!boundedRemember(ackKeys, ackQueue, key, MAX_DEDUPE_KEYS)) return;
      const matched = options.matched === true && Number.isFinite(options.latencyMs) && options.latencyMs >= 0;
      const data = {
        command: ['led.set', 'buzzer.pulse', 'system.get'].includes(ack.command) ? ack.command : undefined,
        command_id: commandId,
        result: ['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'].includes(ack.result)
          ? ack.result
          : undefined,
        latency_ms: matched ? Math.max(0, options.latencyMs) : null
      };
      const readback = safeReadback(ack.readback);
      if (readback) data.readback = readback;
      persist({
        type: 'ack',
        device_id: typeof ack.device_id === 'string' ? ack.device_id : undefined,
        session_id: sessionId,
        receivedAt: Number.isSafeInteger(options.receivedAt) ? options.receivedAt : now(),
        retained: false,
        countsAsLive: true,
        reason: 'ack',
        dedupeKey: 'ack:' + sessionId + ':' + commandId,
        data
      });
    }

    function processStorage(status) {
      if (!status || status.persistent !== false || storageFallbackRecorded) return;
      storageFallbackRecorded = true;
      persist({
        type: 'storage',
        receivedAt: now(),
        retained: false,
        countsAsLive: false,
        reason: 'storage',
        data: { persistent: false, reason: status.reason }
      });
    }

    function stateFromEvent(event) {
      return {
        device_id: event.device_id,
        session_id: event.session_id || '0000000000000000',
        seq: event.seq,
        ...event.data
      };
    }

    function seedFromHistory(rows) {
      const sorted = [...rows].sort((left, right) => left.receivedAt - right.receivedAt);
      for (const event of sorted) {
        if (event.type === 'ack') {
          const key = (event.session_id || '') + '|' + (event.data?.command_id || '');
          if (key !== '|') boundedRemember(ackKeys, ackQueue, key, MAX_DEDUPE_KEYS);
        }
        if (event.type === 'availability' && event.countsAsLive === true) {
          lastLiveAvailability = event.data?.value;
        }
        if (event.type === 'state' && event.countsAsLive === true) {
          const state = stateFromEvent(event);
          const sentinelSession = state.session_id === '0000000000000000';
          if (sentinelSession) updateSentinelBoot(state, event.receivedAt);
          if (Number.isSafeInteger(state.seq) && state.seq > 0) {
            if (!sentinelSession) {
              const sequenceKey = state.device_id + '|' + state.session_id + '|' + String(state.seq);
              boundedRemember(liveStateKeys, liveStateQueue, sequenceKey, MAX_DEDUPE_KEYS);
            }
          } else {
            lastUnsequencedFingerprint = stateFingerprint(state);
            lastUnsequencedAt = event.receivedAt;
          }
          lastLiveState = state;
          lastStateRecordAt = event.receivedAt;
          droppedBySession.set(String(state.session_id).toLowerCase(), state.dropped_snapshots || 0);
        }
      }
      while (droppedBySession.size > 8) droppedBySession.delete(droppedBySession.keys().next().value);
    }

    const ready = hydrated ? Promise.resolve() : Promise.resolve()
      .then(() => store.list())
      .then((rows) => seedFromHistory(Array.isArray(rows) ? rows : []))
      .catch(() => {})
      .then(() => {
        hydrated = true;
        while (pendingObservations.length) pendingObservations.shift()();
      });

    function observeState(state, packet = {}) {
      dispatchObservation(() => processState(state, packet));
    }

    function observeAvailability(value, packet = {}) {
      dispatchObservation(() => processAvailability(value, packet));
    }

    function observeAck(ack, options = {}) {
      dispatchObservation(() => processAck(ack, options));
    }

    function observeStorage(status) {
      dispatchObservation(() => processStorage(status));
    }

    return Object.freeze({ ready, observeState, observeAvailability, observeAck, observeStorage });
  }

  globalThis.F407HistoryRecorder = Object.freeze({ create });
})();
