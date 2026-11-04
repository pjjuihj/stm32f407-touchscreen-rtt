import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const appSource = await readFile(new URL('../public/app.js', import.meta.url), 'utf8');

function makeElement() {
  return {
    textContent: '',
    className: '',
    disabled: false,
    hidden: false,
    value: '500',
    dataset: { index: '0', value: 'on' },
    children: [],
    listeners: {},
    addEventListener(name, handler) { this.listeners[name] = handler; },
    append(...nodes) { this.children.push(...nodes); },
    prepend(node) { this.children.unshift(node); },
    replaceChildren(...nodes) { this.children = nodes; },
    remove() {},
    get lastElementChild() { return this.children.at(-1); }
  };
}

async function startDashboard() {
  const elements = new Map();
  const ledButtons = Array.from({ length: 3 }, () => makeElement());
  const clientHandlers = {};
  const mqttClient = {
    connected: false,
    handlers: clientHandlers,
    on(name, handler) { clientHandlers[name] = handler; },
    subscribe(_topics, _options, callback) { callback(null); },
    publish() {}
  };
  const historyStore = {
    ready: Promise.resolve(),
    async list() { return []; },
    async getStatus() { return { persistent: true, count: 0, oldestAt: null, coverageMs: 0, capacityDropped: 0 }; },
    async record() { return true; }
  };
  const context = {
    document: {
      getElementById(id) {
        if (!elements.has(id)) elements.set(id, makeElement());
        return elements.get(id);
      },
      querySelectorAll() { return ledButtons; },
      createElement() { return makeElement(); }
    },
    location: { host: '127.0.0.1:9000' },
    performance: { now: () => 1 },
    setTimeout: () => 1,
    clearTimeout() {},
    setInterval() {},
    crypto: { getRandomValues(bytes) { bytes.fill(1); return bytes; } },
    F407HistoryStore: { create: () => historyStore },
    F407HistoryRecorder: {
      create: () => ({
        ready: Promise.resolve(),
        observeAvailability() {},
        observeState() {},
        observeAck() {},
        observeStorage() {}
      })
    },
    mqtt: { connect: () => mqttClient }
  };
  context.globalThis = context;
  vm.runInNewContext(appSource, context, { filename: 'app.js' });
  mqttClient.connected = true;
  clientHandlers.connect();
  clientHandlers.message('f407/f407-touchscreen-01/availability', Buffer.from('online'), { retain: false });

  const baseState = {
    schema_version: 1,
    device_id: 'f407-touchscreen-01',
    session_id: '0123456789abcdef',
    uptime_s: 10,
    wifi: { sta: 'connected', ip: '192.168.137.10' },
    bt_connected: false,
    led: [false, false, false],
    buzzer_gpio_high: false,
    dropped_snapshots: 0,
    remote_control_available: true
  };
  clientHandlers.message('f407/f407-touchscreen-01/state', Buffer.from(JSON.stringify(baseState)), { retain: false });
  return { clientHandlers, elements, ledButtons, mqttClient, baseState };
}

test('displays seq-less state but keeps remote commands disabled until a valid seq arrives', async () => {
  const { clientHandlers, elements, ledButtons, baseState } = await startDashboard();

  assert.equal(elements.get('wifi-state').textContent, '已连接');
  assert.equal(elements.get('state-sequence').textContent, '未知');
  assert.equal(elements.get('remote-state').textContent, '缺少序号，已禁用');
  assert.equal(ledButtons.every((button) => button.disabled), true);
  assert.equal(elements.get('buzzer-button').disabled, true);

  clientHandlers.message('f407/f407-touchscreen-01/state',
    Buffer.from(JSON.stringify({ ...baseState, seq: 1 })), { retain: false });
  assert.equal(elements.get('state-sequence').textContent, '1');
  assert.equal(elements.get('remote-state').textContent, '可用');
  assert.equal(ledButtons.every((button) => button.disabled), false);
  assert.equal(elements.get('buzzer-button').disabled, false);
});
