import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { createServer } from 'node:net';
import { dirname, resolve } from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

import mqtt from 'mqtt';

const hostDirectory = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const deviceId = 'f407-touchscreen-01';
const topicRoot = `f407/${deviceId}/`;

async function reserveLoopbackPort() {
  const server = createServer();
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const port = server.address().port;
  await new Promise((resolveClose, rejectClose) => {
    server.close((error) => error ? rejectClose(error) : resolveClose());
  });
  return port;
}

function waitForConnect(client) {
  return new Promise((resolveConnect, rejectConnect) => {
    const timeout = setTimeout(() => rejectConnect(new Error('MQTT connect timed out')), 5000);
    client.once('connect', (packet) => {
      clearTimeout(timeout);
      resolveConnect(packet);
    });
    client.once('error', (error) => {
      clearTimeout(timeout);
      rejectConnect(error);
    });
  });
}

function nextMessage(client, expectedTopic) {
  return new Promise((resolveMessage, rejectMessage) => {
    const timeout = setTimeout(() => {
      client.off('message', onMessage);
      rejectMessage(new Error(`Timed out waiting for ${expectedTopic}`));
    }, 5000);
    const onMessage = (topic, payload, packet) => {
      if (topic !== expectedTopic) return;
      clearTimeout(timeout);
      client.off('message', onMessage);
      resolveMessage({ payload, packet });
    };
    client.on('message', onMessage);
  });
}

function subscribe(client, topics, options = { qos: 1 }) {
  return new Promise((resolveSubscribe, rejectSubscribe) => {
    client.subscribe(topics, options, (error, granted) => {
      if (error) rejectSubscribe(error);
      else resolveSubscribe(granted);
    });
  });
}

function publish(client, topic, payload, options) {
  return new Promise((resolvePublish, rejectPublish) => {
    client.publish(topic, payload, options, (error) => {
      if (error) rejectPublish(error);
      else resolvePublish();
    });
  });
}

function endClient(client) {
  if (!client) return Promise.resolve();
  return new Promise((resolveEnd) => client.end(true, {}, resolveEnd));
}

async function waitForDashboard(url, child, getOutput) {
  const deadline = Date.now() + 6000;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error(`Dashboard process exited early: ${getOutput()}`);
    }
    try {
      const response = await fetch(url);
      if (response.ok) return response;
    } catch {
      // The local listener may not have started yet.
    }
    await delay(50);
  }
  throw new Error(`Dashboard did not start: ${getOutput()}`);
}

async function stopServer(child) {
  if (!child || child.exitCode !== null) return;
  const exited = once(child, 'exit');
  child.kill('SIGINT');
  const result = await Promise.race([
    exited,
    delay(4000).then(() => null)
  ]);
  if (result === null && child.exitCode === null) {
    child.kill();
    await once(child, 'exit');
  }
}

test('local dashboard and MQTT broker interoperate with a simulated device', async () => {
  const mqttPort = await reserveLoopbackPort();
  let dashboardPort = await reserveLoopbackPort();
  while (dashboardPort === mqttPort) dashboardPort = await reserveLoopbackPort();

  let output = '';
  const child = spawn(process.execPath, ['server.mjs'], {
    cwd: hostDirectory,
    windowsHide: true,
    env: {
      ...process.env,
      F407_MQTT_BIND_ADDRESS: '127.0.0.1',
      F407_MQTT_PORT: String(mqttPort),
      F407_DASHBOARD_PORT: String(dashboardPort)
    },
    stdio: ['ignore', 'pipe', 'pipe']
  });
  child.stdout.setEncoding('utf8').on('data', (chunk) => { output += chunk; });
  child.stderr.setEncoding('utf8').on('data', (chunk) => { output += chunk; });

  let device;
  let dashboard;
  try {
    const dashboardUrl = `http://127.0.0.1:${dashboardPort}/`;
    const page = await waitForDashboard(dashboardUrl, child, () => output);
    const pageText = await page.text();
    assert.match(pageText, /F407 本地 IoT 控制台/);
    assert.match(page.headers.get('content-security-policy'), new RegExp(`ws://127\\.0\\.0\\.1:${dashboardPort}`));

    const bundle = await fetch(`${dashboardUrl}vendor/mqtt.min.js`);
    assert.equal(bundle.status, 200);
    assert.ok(Number(bundle.headers.get('content-length')) > 10000);
    assert.equal((await fetch(`${dashboardUrl}not-found`)).status, 404);
    assert.equal((await fetch(dashboardUrl, { method: 'POST' })).status, 405);

    const brokerUrl = `mqtt://127.0.0.1:${mqttPort}`;
    device = mqtt.connect(brokerUrl, {
      clientId: deviceId,
      clean: true,
      protocolVersion: 4,
      reconnectPeriod: 0,
      connectTimeout: 3000
    });
    await waitForConnect(device);
    const commandTopic = `${topicRoot}cmd`;
    const commandGranted = await subscribe(device, commandTopic);
    assert.equal(commandGranted[0].qos, 1);

    const sessionId = '0123456789abcdef';
    const state = {
      schema_version: 1,
      device_id: deviceId,
      session_id: sessionId,
      seq: 1,
      uptime_s: 10,
      wifi: { sta: 'connected', ip: '192.168.137.10' },
      bt_connected: false,
      led: [false, false, false],
      buzzer_gpio_high: false,
      dropped_snapshots: 0,
      remote_control_available: true
    };
    await publish(device, `${topicRoot}availability`, 'online', { qos: 1, retain: true });
    await publish(device, `${topicRoot}state`, JSON.stringify(state), { qos: 1, retain: true });

    const dashboardClientId = `dash-${Math.random().toString(16).slice(2).padEnd(16, '0').slice(0, 16)}`;
    dashboard = mqtt.connect(`ws://127.0.0.1:${dashboardPort}/mqtt`, {
      clientId: dashboardClientId,
      clean: true,
      protocolVersion: 4,
      reconnectPeriod: 0,
      connectTimeout: 3000,
      wsOptions: { origin: `http://127.0.0.1:${dashboardPort}` }
    });
    await waitForConnect(dashboard);

    const availabilityMessage = nextMessage(dashboard, `${topicRoot}availability`);
    const stateMessage = nextMessage(dashboard, `${topicRoot}state`);
    const ackMessage = nextMessage(dashboard, `${topicRoot}ack`);
    const dashboardGranted = await subscribe(dashboard, [
      `${topicRoot}availability`,
      `${topicRoot}state`,
      `${topicRoot}ack`
    ]);
    assert.deepEqual(dashboardGranted.map((item) => item.qos), [1, 1, 1]);

    const [availability, retainedState] = await Promise.all([availabilityMessage, stateMessage]);
    assert.equal(availability.payload.toString('utf8'), 'online');
    assert.equal(availability.packet.retain, true);
    assert.deepEqual(JSON.parse(retainedState.payload.toString('utf8')), state);
    assert.equal(retainedState.packet.retain, true);

    const command = {
      schema_version: 1,
      device_id: deviceId,
      session_id: sessionId,
      command_id: 'fedcba9876543210',
      command: 'led.set',
      led_index: 1,
      value: 'on'
    };
    const receivedCommand = nextMessage(device, commandTopic);
    await publish(dashboard, commandTopic, JSON.stringify(command), { qos: 1, retain: false });
    const deliveredCommand = await receivedCommand;
    assert.equal(deliveredCommand.packet.retain, false);
    assert.equal(deliveredCommand.packet.qos, 1);
    assert.deepEqual(JSON.parse(deliveredCommand.payload.toString('utf8')), command);

    const ack = {
      schema_version: 1,
      device_id: deviceId,
      session_id: sessionId,
      command_id: command.command_id,
      command: command.command,
      result: 'OK',
      readback: { led: [false, true, false], buzzer_gpio_high: false }
    };
    await publish(device, `${topicRoot}ack`, JSON.stringify(ack), { qos: 1, retain: false });
    const receivedAck = await ackMessage;
    assert.deepEqual(JSON.parse(receivedAck.payload.toString('utf8')), ack);
    assert.equal(receivedAck.packet.retain, false);
  } finally {
    await endClient(dashboard);
    await endClient(device);
    await stopServer(child);
  }
});
