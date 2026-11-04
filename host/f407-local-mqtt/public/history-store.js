(() => {
  'use strict';

  const DATABASE_NAME = 'f407-dashboard-history';
  const DATABASE_VERSION = 1;
  const EVENTS_STORE = 'events';
  const METADATA_STORE = 'metadata';
  const MAX_AGE_MS = 24 * 60 * 60 * 1000;
  const MAX_EVENTS = 5000;
  const SAFE_TYPES = new Set(['state', 'state-snapshot', 'availability', 'ack', 'storage']);
  const SAFE_REASONS = new Set([
    'initial', 'change', 'sample', 'change+sample', 'availability', 'ack',
    'storage', 'dropped-snapshots'
  ]);

  function isObject(value) {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
  }

  function isInteger(value, minimum = 0) {
    return Number.isSafeInteger(value) && value >= minimum;
  }

  function isSessionId(value) {
    return typeof value === 'string' && /^[0-9a-fA-F]{16}$/.test(value);
  }

  function isIPv4(value) {
    if (typeof value !== 'string') return false;
    const parts = value.split('.');
    return parts.length === 4 && parts.every((part) => /^\d{1,3}$/.test(part) && Number(part) <= 255);
  }

  function safeDeviceId(value) {
    return typeof value === 'string' && /^[a-zA-Z0-9._-]{1,64}$/.test(value) ? value : null;
  }

  function createId(receivedAt) {
    createId.counter = (createId.counter || 0) + 1;
    const random = Math.random().toString(36).slice(2, 10) || '0';
    return 'e-' + receivedAt + '-' + String(createId.counter).padStart(10, '0') + '-' + random;
  }

  function sanitizeData(type, value) {
    const data = isObject(value) ? value : {};
    if (type === 'state' || type === 'state-snapshot') {
      const result = {};
      if (isInteger(data.uptime_s)) result.uptime_s = data.uptime_s;
      if (isObject(data.wifi) && ['connected', 'disconnected', 'unknown'].includes(data.wifi.sta)) {
        result.wifi = {
          sta: data.wifi.sta,
          ip: isIPv4(data.wifi.ip) ? data.wifi.ip : null
        };
      }
      if (typeof data.bt_connected === 'boolean') result.bt_connected = data.bt_connected;
      if (Array.isArray(data.led) && data.led.length === 3 && data.led.every((item) => typeof item === 'boolean')) {
        result.led = data.led.slice();
      }
      if (typeof data.buzzer_gpio_high === 'boolean') result.buzzer_gpio_high = data.buzzer_gpio_high;
      if (isInteger(data.dropped_snapshots)) result.dropped_snapshots = data.dropped_snapshots;
      if (isInteger(data.droppedDelta, 1)) result.droppedDelta = data.droppedDelta;
      if (typeof data.remote_control_available === 'boolean') {
        result.remote_control_available = data.remote_control_available;
      }
      return result;
    }

    if (type === 'availability') {
      return { value: ['online', 'offline'].includes(data.value) ? data.value : 'unknown' };
    }

    if (type === 'ack') {
      const result = {};
      if (['led.set', 'buzzer.pulse', 'system.get'].includes(data.command)) result.command = data.command;
      if (typeof data.command_id === 'string' && /^[0-9a-fA-F]{16}$/.test(data.command_id)) {
        result.command_id = data.command_id.toLowerCase();
      }
      if (['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'].includes(data.result)) {
        result.result = data.result;
      }
      if (data.latency_ms === null || (typeof data.latency_ms === 'number' &&
          Number.isFinite(data.latency_ms) && data.latency_ms >= 0)) {
        result.latency_ms = data.latency_ms;
      }
      if (isObject(data.readback)) {
        const readback = {};
        if (Array.isArray(data.readback.led) && data.readback.led.length === 3 &&
            data.readback.led.every((item) => typeof item === 'boolean')) {
          readback.led = data.readback.led.slice();
        }
        if (typeof data.readback.buzzer_gpio_high === 'boolean') {
          readback.buzzer_gpio_high = data.readback.buzzer_gpio_high;
        }
        if (Object.keys(readback).length) result.readback = readback;
      }
      return result;
    }

    return {
      persistent: false,
      reason: ['unavailable', 'write-failed'].includes(data.reason) ? data.reason : 'unavailable'
    };
  }

  function sanitizeEvent(event, fallbackNow, keepId = false) {
    if (!isObject(event) || !SAFE_TYPES.has(event.type)) return null;
    const receivedAt = isInteger(event.receivedAt) ? event.receivedAt : fallbackNow;
    if (!isInteger(receivedAt)) return null;
    const row = {
      id: keepId && typeof event.id === 'string' && /^e-[a-zA-Z0-9_-]{1,120}$/.test(event.id)
        ? event.id
        : createId(receivedAt),
      type: event.type,
      receivedAt,
      data: sanitizeData(event.type, event.data)
    };
    const deviceId = safeDeviceId(event.device_id);
    if (deviceId) row.device_id = deviceId;
    if (isSessionId(event.session_id)) row.session_id = event.session_id.toLowerCase();
    if (isInteger(event.seq, 1)) row.seq = event.seq;
    row.retained = event.retained === true;
    row.countsAsLive = event.countsAsLive === true && !row.retained;
    row.reason = SAFE_REASONS.has(event.reason) ? event.reason : '';
    if (typeof event.dedupeKey === 'string' && /^[a-zA-Z0-9._:-]{1,256}$/.test(event.dedupeKey)) {
      row.dedupeKey = event.dedupeKey;
    }
    return row;
  }

  function sortRows(rows) {
    rows.sort((left, right) => left.receivedAt - right.receivedAt || left.id.localeCompare(right.id));

    const sequenceGroups = new Map();
    rows.forEach((row, index) => {
      if (!['state', 'state-snapshot'].includes(row.type) || !row.device_id || !row.session_id ||
          !isInteger(row.seq, 1)) return;
      const key = row.device_id + '|' + row.session_id;
      if (!sequenceGroups.has(key)) sequenceGroups.set(key, []);
      sequenceGroups.get(key).push(index);
    });

    for (const indexes of sequenceGroups.values()) {
      if (indexes.length < 2) continue;
      const ordered = indexes.map((index) => rows[index]).sort((left, right) =>
        left.seq - right.seq || left.receivedAt - right.receivedAt || left.id.localeCompare(right.id));
      indexes.forEach((index, offset) => { rows[index] = ordered[offset]; });
    }
    return rows;
  }

  function cleanRows(rows, now) {
    const cutoff = now - MAX_AGE_MS;
    const valid = rows.map((row) => sanitizeEvent(row, now, true)).filter((row) =>
      row && row.receivedAt >= cutoff && row.receivedAt <= now + MAX_AGE_MS
    );
    return sortRows(valid);
  }

  function createIndexedDbBackend(indexedDb) {
    let databasePromise;

    function open() {
      if (!indexedDb) return Promise.reject(new Error('IndexedDB unavailable'));
      if (databasePromise) return databasePromise;
      databasePromise = new Promise((resolve, reject) => {
        let request;
        try {
          request = indexedDb.open(DATABASE_NAME, DATABASE_VERSION);
        } catch (error) {
          reject(error);
          return;
        }
        request.onupgradeneeded = () => {
          const database = request.result;
          if (!database.objectStoreNames.contains(EVENTS_STORE)) {
            const events = database.createObjectStore(EVENTS_STORE, { keyPath: 'id' });
            events.createIndex('receivedAt', 'receivedAt', { unique: false });
          }
          if (!database.objectStoreNames.contains(METADATA_STORE)) {
            database.createObjectStore(METADATA_STORE, { keyPath: 'key' });
          }
        };
        request.onsuccess = () => {
          const database = request.result;
          database.onversionchange = () => database.close();
          resolve(database);
        };
        request.onerror = () => reject(request.error || new Error('IndexedDB open failed'));
        request.onblocked = () => reject(new Error('IndexedDB open blocked'));
      });
      return databasePromise;
    }

    async function transaction(storeName, mode, operation) {
      const database = await open();
      return new Promise((resolve, reject) => {
        let tx;
        try {
          tx = database.transaction(storeName, mode);
          operation(tx.objectStore(storeName), tx);
        } catch (error) {
          reject(error);
          return;
        }
        tx.oncomplete = () => resolve();
        tx.onerror = () => reject(tx.error || new Error('IndexedDB transaction failed'));
        tx.onabort = () => reject(tx.error || new Error('IndexedDB transaction aborted'));
      });
    }

    return {
      open,
      async getAll() {
        const database = await open();
        return new Promise((resolve, reject) => {
          let tx;
          let request;
          try {
            tx = database.transaction(EVENTS_STORE, 'readonly');
            request = tx.objectStore(EVENTS_STORE).getAll();
          } catch (error) {
            reject(error);
            return;
          }
          request.onsuccess = () => resolve(request.result || []);
          request.onerror = () => reject(request.error || new Error('IndexedDB read failed'));
        });
      },
      async put(event) {
        return transaction(EVENTS_STORE, 'readwrite', (store) => store.put(event));
      },
      async delete(ids) {
        if (!ids.length) return;
        return transaction(EVENTS_STORE, 'readwrite', (store) => {
          for (const id of ids) store.delete(id);
        });
      },
      async getMeta(key) {
        const database = await open();
        return new Promise((resolve, reject) => {
          let tx;
          let request;
          try {
            tx = database.transaction(METADATA_STORE, 'readonly');
            request = tx.objectStore(METADATA_STORE).get(key);
          } catch (error) {
            reject(error);
            return;
          }
          request.onsuccess = () => resolve(request.result ? request.result.value : null);
          request.onerror = () => reject(request.error || new Error('IndexedDB metadata read failed'));
        });
      },
      async putMeta(key, value) {
        return transaction(METADATA_STORE, 'readwrite', (store) => store.put({ key, value }));
      }
    };
  }

  function create(options = {}) {
    const now = typeof options.now === 'function' ? options.now : Date.now;
    const backend = options.backend || createIndexedDbBackend(globalThis.indexedDB);
    const onStatus = typeof options.onStatus === 'function' ? options.onStatus : () => {};
    let rows = [];
    let persistent = true;
    let capacityDropped = 0;
    let storageNotified = false;
    let writeQueue = Promise.resolve();
    let dedupeKeys = new Set();

    function refreshDedupeKeys() {
      dedupeKeys = new Set(rows.map((row) => row.dedupeKey).filter(Boolean));
    }

    function notifyStorage(reason) {
      if (storageNotified) return;
      storageNotified = true;
      try { onStatus({ persistent: false, reason }); } catch { /* UI reporting must not affect storage. */ }
    }

    function failStorage(reason) {
      if (persistent) {
        persistent = false;
        notifyStorage(reason);
      }
    }

    function pruneInMemory(currentTime) {
      const cutoff = currentTime - MAX_AGE_MS;
      const expiredRows = rows.filter((row) => row.receivedAt < cutoff);
      let survivors = rows.filter((row) => row.receivedAt >= cutoff);
      const expiredIds = expiredRows.map((row) => row.id);
      const capacityIds = [];
      if (survivors.length > MAX_EVENTS) {
        survivors.sort((left, right) => left.receivedAt - right.receivedAt || left.id.localeCompare(right.id));
        const evicted = survivors.splice(0, survivors.length - MAX_EVENTS);
        capacityIds.push(...evicted.map((row) => row.id));
      }
      rows = sortRows(survivors);
      capacityDropped += capacityIds.length;
      return { expiredIds, capacityIds };
    }

    const ready = (async () => {
      try {
        await backend.open();
        const [stored, storedCapacityDropped] = await Promise.all([
          backend.getAll(), backend.getMeta('capacityDropped')
        ]);
        capacityDropped = isInteger(storedCapacityDropped) ? storedCapacityDropped : 0;
        const currentTime = now();
        const loaded = cleanRows(Array.isArray(stored) ? stored : [], currentTime);
        const byReceiveTime = [...loaded].sort((left, right) =>
          left.receivedAt - right.receivedAt || left.id.localeCompare(right.id));
        const capEvictions = Math.max(0, byReceiveTime.length - MAX_EVENTS);
        rows = sortRows(byReceiveTime.slice(-MAX_EVENTS));
        refreshDedupeKeys();
        capacityDropped += capEvictions;
        const keptIds = new Set(rows.map((row) => row.id));
        const removeIds = [...new Set((Array.isArray(stored) ? stored : [])
          .filter((row) => isObject(row) && typeof row.id === 'string' && !keptIds.has(row.id))
          .map((row) => row.id))];
        if (removeIds.length) await backend.delete(removeIds);
        if (capEvictions || !isInteger(storedCapacityDropped)) {
          await backend.putMeta('capacityDropped', capacityDropped);
        }
      } catch {
        rows = [];
        refreshDedupeKeys();
        failStorage('unavailable');
      }
    })();

    async function record(event) {
      await ready;
      const receivedAt = now();
      const row = sanitizeEvent(event, receivedAt);
      if (!row) return false;
      if (row.dedupeKey && dedupeKeys.has(row.dedupeKey)) return false;
      rows.push(row);
      const removed = pruneInMemory(receivedAt);
      refreshDedupeKeys();
      const currentCapacityDropped = capacityDropped;
      writeQueue = writeQueue.then(async () => {
        if (!persistent) return;
        try {
          await backend.put(row);
          await backend.delete([...removed.expiredIds, ...removed.capacityIds]);
          if (removed.capacityIds.length) {
            await backend.putMeta('capacityDropped', currentCapacityDropped);
          }
        } catch {
          failStorage('write-failed');
        }
      }).catch(() => failStorage('write-failed'));
      await writeQueue;
      return true;
    }

    async function list() {
      await ready;
      await writeQueue;
      return rows.map((row) => structuredClone(row));
    }

    async function getStatus() {
      await ready;
      await writeQueue;
      const oldestAt = rows.length ? Math.min(...rows.map((row) => row.receivedAt)) : null;
      const newestAt = rows.length ? Math.max(...rows.map((row) => row.receivedAt)) : null;
      return {
        persistent,
        count: rows.length,
        oldestAt,
        coverageMs: oldestAt === null ? 0 : Math.max(0, newestAt - oldestAt),
        capacityDropped
      };
    }

    return { ready, record, list, getStatus };
  }

  function escapeCsvField(value) {
    const text = value === null || value === undefined ? '' : String(value);
    return /[",\r\n]/.test(text) ? '"' + text.replace(/"/g, '""') + '"' : text;
  }

  function toCsv(events) {
    const header = [
      'type', 'device_id', 'received_at_utc', 'session_id', 'seq', 'retained', 'reason', 'data'
    ];
    const lines = [header.map(escapeCsvField).join(',')];
    for (const event of Array.isArray(events) ? events : []) {
      const row = sanitizeEvent(event, 0, true);
      if (!row) continue;
      const data = JSON.stringify(row.data);
      lines.push([
        row.type,
        row.device_id || '',
        new Date(row.receivedAt).toISOString(),
        row.session_id || '',
        row.seq ?? '',
        row.retained ? 'true' : 'false',
        row.reason,
        data
      ].map(escapeCsvField).join(','));
    }
    return lines.join('\r\n') + '\r\n';
  }

  globalThis.F407HistoryStore = Object.freeze({ create, toCsv, escapeCsvField });
})();
