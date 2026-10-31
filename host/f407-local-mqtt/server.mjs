import { createServer as createHttpServer } from 'node:http';
import { createServer as createMqttServer } from 'node:net';
import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { Aedes } from 'aedes';
import { createWebSocketStream, WebSocketServer } from 'ws';

const DEVICE_ID = 'f407-touchscreen-01';
const TOPIC_ROOT = 'f407/' + DEVICE_ID + '/';
const TOPICS = {
  availability: TOPIC_ROOT + 'availability',
  state: TOPIC_ROOT + 'state',
  command: TOPIC_ROOT + 'cmd',
  acknowledgement: TOPIC_ROOT + 'ack'
};
const MAX_PAYLOAD_BYTES = 1024;
const LOCAL_HOST = '127.0.0.1';
const HTTP_PORT = readPort(process.env.F407_DASHBOARD_PORT, 9000);
const MQTT_PORT = readPort(process.env.F407_MQTT_PORT, 1883);
const MQTT_BIND_ADDRESS = process.env.F407_MQTT_BIND_ADDRESS || '192.168.137.1';
const ROOT_DIR = fileURLToPath(new URL('.', import.meta.url));
const PUBLIC_DIR = join(ROOT_DIR, 'public');
const MQTT_BUNDLE = join(ROOT_DIR, 'node_modules', 'mqtt', 'dist', 'mqtt.min.js');

if (!isPrivateOrLoopbackIPv4(MQTT_BIND_ADDRESS)) {
  throw new Error('Broker bind address must be a private or loopback IPv4 address.');
}

function readPort(value, fallback) {
  if (value === undefined || value === '') return fallback;
  const port = Number(value);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('Port must be an integer from 1 to 65535.');
  }
  return port;
}

function isPrivateOrLoopbackIPv4(address) {
  if (!/^\d{1,3}(?:\.\d{1,3}){3}$/.test(address)) return false;
  const parts = address.split('.').map(Number);
  if (parts.some((part) => part < 0 || part > 255)) return false;
  const [first, second] = parts;
  return first === 10 ||
    (first === 172 && second >= 16 && second <= 31) ||
    (first === 192 && second === 168) ||
    first === 127;
}

function isObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function isSessionId(value) {
  return typeof value === 'string' && /^[0-9a-fA-F]{16}$/.test(value);
}

function parsePayload(payload) {
  if (!Buffer.isBuffer(payload) || payload.length === 0 || payload.length > MAX_PAYLOAD_BYTES) {
    return null;
  }
  try {
    return JSON.parse(payload.toString('utf8'));
  } catch {
    return null;
  }
}

function hasVersionAndDevice(value) {
  return isObject(value) && value.schema_version === 1 && value.device_id === DEVICE_ID;
}

function validState(value) {
  if (!hasVersionAndDevice(value) ||
      !isSessionId(value.session_id) ||
      !Number.isSafeInteger(value.seq) || value.seq < 1 ||
      !Number.isSafeInteger(value.uptime_s) || value.uptime_s < 0 ||
      !isObject(value.wifi) ||
      !['connected', 'disconnected', 'unknown'].includes(value.wifi.sta) ||
      !(value.wifi.ip === null || isIPv4(value.wifi.ip)) ||
      typeof value.bt_connected !== 'boolean' ||
      !Array.isArray(value.led) || value.led.length !== 3 ||
      !value.led.every((item) => typeof item === 'boolean') ||
      typeof value.buzzer_gpio_high !== 'boolean' ||
      !Number.isSafeInteger(value.dropped_snapshots) || value.dropped_snapshots < 0 ||
      typeof value.remote_control_available !== 'boolean') {
    return false;
  }
  return true;
}

function isIPv4(value) {
  if (typeof value !== 'string') return false;
  const parts = value.split('.');
  return parts.length === 4 &&
    parts.every((part) => /^\d{1,3}$/.test(part) && Number(part) <= 255);
}

function validCommand(value) {
  if (!hasVersionAndDevice(value) ||
      !isSessionId(value.session_id) ||
      typeof value.command_id !== 'string' ||
      !/^[0-9a-fA-F]{16}$/.test(value.command_id) ||
      typeof value.command !== 'string') {
    return false;
  }
  const fields = Object.keys(value).sort().join(',');
  if (value.command === 'led.set') {
    return fields === 'command,command_id,device_id,led_index,schema_version,session_id,value' &&
      Number.isInteger(value.led_index) && value.led_index >= 0 && value.led_index <= 2 &&
      (value.value === 'on' || value.value === 'off');
  }
  if (value.command === 'buzzer.pulse') {
    return fields === 'command,command_id,device_id,duration_ms,schema_version,session_id' &&
      Number.isInteger(value.duration_ms) && value.duration_ms >= 100 && value.duration_ms <= 1000;
  }
  if (value.command === 'system.get') {
    return fields === 'command,command_id,device_id,schema_version,session_id';
  }
  return false;
}

function validAcknowledgement(value) {
  if (!hasVersionAndDevice(value) ||
      !isSessionId(value.session_id) ||
      typeof value.command_id !== 'string' ||
      !/^[0-9a-fA-F]{16}$/.test(value.command_id) ||
      !['led.set', 'buzzer.pulse', 'system.get'].includes(value.command) ||
      !['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'].includes(value.result)) {
    return false;
  }
  if (value.readback === undefined) return true;
  if (!isObject(value.readback)) return false;
  if (value.readback.led !== undefined &&
      (!Array.isArray(value.readback.led) ||
       value.readback.led.length !== 3 ||
       !value.readback.led.every((item) => typeof item === 'boolean'))) {
    return false;
  }
  return value.readback.buzzer_gpio_high === undefined ||
    typeof value.readback.buzzer_gpio_high === 'boolean';
}

function authorizationError(message) {
  return new Error(message);
}

const broker = await Aedes.createBroker({
  maxInflightInbound: 32,
  maxTopicLevels: 6,
  drainTimeout: 10000,
  queueLimit: 16
});

broker.preConnect = (client, packet, callback) => {
  const clientId = packet?.clientId;
  const isDevice = clientId === DEVICE_ID;
  const isDashboard = typeof clientId === 'string' &&
    /^dash-[0-9a-fA-F]{16}$/.test(clientId);
  if ((!isDevice && !isDashboard) || packet.clean !== true) {
    return callback(authorizationError('Only known clients with clean sessions are allowed.'), false);
  }
  callback(null, true);
};

broker.authorizePublish = (client, packet, callback) => {
  if (packet.topic.startsWith('$SYS/')) {
    return callback(authorizationError('Reserved system topic.'));
  }
  if (!Object.values(TOPICS).includes(packet.topic) ||
      !Buffer.isBuffer(packet.payload) ||
      packet.payload.length > MAX_PAYLOAD_BYTES ||
      packet.qos !== 1) {
    return callback(authorizationError('Unsupported topic, payload size, or QoS.'));
  }

  if (packet.topic === TOPICS.command) {
    if (!client?.id.startsWith('dash-') || packet.retain || !validCommand(parsePayload(packet.payload))) {
      return callback(authorizationError('Invalid dashboard command.'));
    }
    return callback(null);
  }

  if (packet.topic === TOPICS.availability) {
    const state = packet.payload.toString('utf8');
    const isLastWill = client === null && state === 'offline' && packet.retain;
    const isDevicePublish = client?.id === DEVICE_ID &&
      (state === 'online' || state === 'offline') &&
      packet.retain;
    if (!isLastWill && !isDevicePublish) {
      return callback(authorizationError('Invalid availability publication.'));
    }
    return callback(null);
  }

  if (packet.topic === TOPICS.state) {
    if (client?.id !== DEVICE_ID || !packet.retain || !validState(parsePayload(packet.payload))) {
      return callback(authorizationError('Invalid retained state publication.'));
    }
    return callback(null);
  }

  if (packet.topic === TOPICS.acknowledgement) {
    if (client?.id !== DEVICE_ID || packet.retain ||
        !validAcknowledgement(parsePayload(packet.payload))) {
      return callback(authorizationError('Invalid acknowledgement publication.'));
    }
    return callback(null);
  }

  return callback(authorizationError('Publication is not allowed.'));
};

broker.authorizeSubscribe = (client, subscription, callback) => {
  const topic = subscription.topic;
  const browserTopics = [TOPICS.availability, TOPICS.state, TOPICS.acknowledgement];
  const isDashboard = client.id.startsWith('dash-') && browserTopics.includes(topic);
  const isDevice = client.id === DEVICE_ID && topic === TOPICS.command;
  if (!isDashboard && !isDevice) {
    return callback(authorizationError('Subscription is not allowed.'));
  }
  callback(null, subscription);
};

broker.on('clientReady', (client) => {
  console.log('MQTT client connected:', safeClientId(client.id));
});
broker.on('clientDisconnect', (client) => {
  console.log('MQTT client disconnected:', safeClientId(client.id));
});
broker.on('clientError', (client, error) => {
  console.warn('MQTT client error:', safeClientId(client?.id), safeErrorCode(error));
});
broker.on('connectionError', (client, error) => {
  console.warn('MQTT connection error:', safeClientId(client?.id), safeErrorCode(error));
});

function safeClientId(clientId) {
  if (clientId === DEVICE_ID || /^dash-[0-9a-fA-F]{16}$/.test(clientId)) return clientId;
  return 'other-client';
}

function safeErrorCode(error) {
  return typeof error?.code === 'string' ? error.code : 'CLIENT_ERROR';
}

const mqttServer = createMqttServer(broker.handle);
const webSocketServer = new WebSocketServer({ noServer: true, maxPayload: MAX_PAYLOAD_BYTES });
const httpServer = createHttpServer(async (request, response) => {
  const url = new URL(request.url || '/', 'http://' + LOCAL_HOST + ':' + HTTP_PORT);
  if (request.method !== 'GET' && request.method !== 'HEAD') {
    response.writeHead(405, { Allow: 'GET, HEAD' });
    response.end();
    return;
  }

  const route = url.pathname === '/' ? '/index.html' : url.pathname;
  const files = {
    '/index.html': join(PUBLIC_DIR, 'index.html'),
    '/app.js': join(PUBLIC_DIR, 'app.js'),
    '/styles.css': join(PUBLIC_DIR, 'styles.css'),
    '/vendor/mqtt.min.js': MQTT_BUNDLE
  };
  const filePath = files[route];
  if (!filePath) {
    response.writeHead(404);
    response.end();
    return;
  }

  try {
    const content = await readFile(filePath);
    const contentType = route.endsWith('.html') ? 'text/html; charset=utf-8' :
      route.endsWith('.js') ? 'text/javascript; charset=utf-8' :
        route.endsWith('.css') ? 'text/css; charset=utf-8' : 'application/octet-stream';
    response.writeHead(200, {
      'Content-Type': contentType,
      'Content-Length': content.length,
      'Cache-Control': route === '/vendor/mqtt.min.js' ? 'public, max-age=3600' : 'no-store',
      'Content-Security-Policy': "default-src 'self'; connect-src 'self' ws://127.0.0.1:" + HTTP_PORT +
        "; style-src 'self'; script-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
      'Referrer-Policy': 'no-referrer',
      'X-Content-Type-Options': 'nosniff',
      'X-Frame-Options': 'DENY'
    });
    response.end(request.method === 'HEAD' ? undefined : content);
  } catch {
    response.writeHead(503);
    response.end('Dashboard dependency is unavailable. Run npm install in this folder.');
  }
});

httpServer.on('upgrade', (request, socket, head) => {
  const url = new URL(request.url || '/', 'http://' + LOCAL_HOST + ':' + HTTP_PORT);
  const expectedOrigin = 'http://' + LOCAL_HOST + ':' + HTTP_PORT;
  if (url.pathname !== '/mqtt' || request.headers.origin !== expectedOrigin) {
    socket.destroy();
    return;
  }

  webSocketServer.handleUpgrade(request, socket, head, (websocket) => {
    webSocketServer.emit('connection', websocket, request);
  });
});

webSocketServer.on('connection', (websocket, request) => {
  const stream = createWebSocketStream(websocket);
  broker.handle(stream, request);
});

function listen(server, port, address) {
  return new Promise((resolve, reject) => {
    const onError = (error) => {
      server.off('listening', onListening);
      reject(error);
    };
    const onListening = () => {
      server.off('error', onError);
      resolve();
    };
    server.once('error', onError);
    server.once('listening', onListening);
    server.listen(port, address);
  });
}

await listen(mqttServer, MQTT_PORT, MQTT_BIND_ADDRESS);
try {
  await listen(httpServer, HTTP_PORT, LOCAL_HOST);
} catch (error) {
  mqttServer.close();
  await closeBroker();
  throw error;
}

console.log('Local dashboard: http://' + LOCAL_HOST + ':' + HTTP_PORT);
console.log('Device MQTT broker: ' + MQTT_BIND_ADDRESS + ':' + MQTT_PORT);
console.log('Dashboard HTTP and WebSocket listeners are bound to loopback only.');

let shuttingDown = false;
async function shutdown() {
  if (shuttingDown) return;
  shuttingDown = true;
  await closeBroker();
  await Promise.all([
    closeServer(httpServer),
    closeServer(mqttServer),
    new Promise((resolve) => webSocketServer.close(resolve))
  ]);
}

function closeServer(server) {
  return new Promise((resolve) => {
    if (!server.listening) return resolve();
    server.close(() => resolve());
  });
}

function closeBroker() {
  return new Promise((resolve) => broker.close(() => resolve()));
}

process.once('SIGINT', () => {
  void shutdown().finally(() => process.exit(0));
});
process.once('SIGTERM', () => {
  void shutdown().finally(() => process.exit(0));
});
