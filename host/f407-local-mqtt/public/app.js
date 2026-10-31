(() => {
  'use strict';

  const DEVICE_ID = 'f407-touchscreen-01';
  const ROOT = 'f407/' + DEVICE_ID + '/';
  const TOPICS = {
    availability: ROOT + 'availability',
    state: ROOT + 'state',
    command: ROOT + 'cmd',
    acknowledgement: ROOT + 'ack'
  };
  const HEARTBEAT_STALE_MS = 30000;
  const RETRY_DELAY_MS = 2000;
  const MAX_SENDS = 3;
  const MAX_EVENTS = 40;

  const dom = Object.fromEntries([
    'dashboard-dot', 'dashboard-connection', 'wifi-state', 'wifi-ip',
    'transport-state', 'transport-detail', 'application-state', 'heartbeat-detail',
    'device-name', 'freshness-badge', 'session-id', 'state-sequence', 'uptime',
    'bluetooth-state', 'dropped-count', 'remote-state', 'led-0', 'led-1', 'led-2',
    'buzzer-state', 'controls-state', 'buzzer-duration', 'buzzer-button',
    'get-state-button', 'pending-command', 'pending-title', 'pending-detail',
    'clear-unknown', 'ack-empty', 'latest-ack', 'ack-result', 'ack-command', 'ack-readback',
    'ack-id', 'ack-time', 'event-count', 'event-list'
  ].map((id) => [id, document.getElementById(id)]));

  const ledButtons = [...document.querySelectorAll('[data-command="led.set"]')];
  const commandButtons = [
    ...ledButtons,
    dom['buzzer-button'],
    dom['get-state-button']
  ];

  let mqttClient = null;
  let subscriptionsReady = false;
  let availability = 'unknown';
  let latestState = null;
  let lastLiveStateAt = 0;
  let activeSessionId = null;
  let pendingCommand = null;
  let eventTotal = 0;
  let localCryptoReady = false;

  function randomHex64() {
    if (!globalThis.crypto || typeof globalThis.crypto.getRandomValues !== 'function') return null;
    try {
      const bytes = new Uint8Array(8);
      globalThis.crypto.getRandomValues(bytes);
      return [...bytes].map((byte) => byte.toString(16).padStart(2, '0')).join('');
    } catch {
      return null;
    }
  }

  function isObject(value) {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
  }

  function isSessionId(value) {
    return typeof value === 'string' && /^[0-9a-fA-F]{16}$/.test(value);
  }

  function isIPv4(value) {
    if (typeof value !== 'string') return false;
    const parts = value.split('.');
    return parts.length === 4 &&
      parts.every((part) => /^\d{1,3}$/.test(part) && Number(part) <= 255);
  }

  function commonMessage(value) {
    return isObject(value) && value.schema_version === 1 && value.device_id === DEVICE_ID;
  }

  function validState(value) {
    return commonMessage(value) &&
      isSessionId(value.session_id) &&
      Number.isSafeInteger(value.seq) && value.seq > 0 &&
      Number.isSafeInteger(value.uptime_s) && value.uptime_s >= 0 &&
      isObject(value.wifi) &&
      ['connected', 'disconnected', 'unknown'].includes(value.wifi.sta) &&
      (value.wifi.ip === null || isIPv4(value.wifi.ip)) &&
      typeof value.bt_connected === 'boolean' &&
      Array.isArray(value.led) && value.led.length === 3 &&
      value.led.every((item) => typeof item === 'boolean') &&
      typeof value.buzzer_gpio_high === 'boolean' &&
      Number.isSafeInteger(value.dropped_snapshots) && value.dropped_snapshots >= 0 &&
      typeof value.remote_control_available === 'boolean';
  }

  function validAcknowledgement(value) {
    return commonMessage(value) &&
      isSessionId(value.session_id) &&
      typeof value.command_id === 'string' &&
      /^[0-9a-fA-F]{16}$/.test(value.command_id) &&
      ['led.set', 'buzzer.pulse', 'system.get'].includes(value.command) &&
      ['OK', 'INVALID', 'INVALID_PARAM', 'RATE_LIMITED', 'ERROR', 'STALE_SESSION'].includes(value.result);
  }

  function decodeJson(payload) {
    try {
      const value = JSON.parse(payload.toString());
      return isObject(value) ? value : null;
    } catch {
      return null;
    }
  }

  function setDashboardConnection(label, mode) {
    dom['dashboard-connection'].textContent = label;
    dom['dashboard-dot'].className = 'dot' + (mode ? ' ' + mode : '');
  }

  function setText(element, text) {
    element.textContent = String(text);
  }

  function addEvent(message) {
    const row = document.createElement('li');
    const time = document.createElement('time');
    const body = document.createElement('span');
    time.textContent = new Date().toLocaleTimeString();
    body.textContent = message;
    row.append(time, body);
    dom['event-list'].prepend(row);
    while (dom['event-list'].children.length > MAX_EVENTS) {
      dom['event-list'].lastElementChild.remove();
    }
    eventTotal += 1;
    setText(dom['event-count'], Math.min(eventTotal, MAX_EVENTS));
  }

  function parseAvailability(payload, packet) {
    const value = payload.toString().trim();
    if (value !== 'online' && value !== 'offline') {
      addEvent('忽略格式不符合约定的 availability 消息。');
      return;
    }
    availability = value;
    addEvent('设备 MQTT 会话：' + (value === 'online' ? '在线' : '离线') +
      (packet.retain ? '（Broker 保留状态）' : ''));
    render();
  }

  function handleState(payload, packet) {
    const state = decodeJson(payload);
    if (!state || !validState(state)) {
      const version = state && state.schema_version;
      addEvent(version !== 1
        ? '状态消息版本不受支持。'
        : '收到格式无效的状态消息，未更新设备视图。');
      return;
    }

    const isRetainedSnapshot = packet.retain === true;
    if (isRetainedSnapshot) {
      if (!latestState || lastLiveStateAt === 0) latestState = state;
      addEvent('载入 Broker 保留的最近已知状态。');
      render();
      return;
    }

    // The all-zero session is a reserved RNG-failure sentinel. It keeps
    // telemetry visible while remote control remains disabled; it is not a
    // unique boot session, so sequence resets must not make the dashboard stale.
    if (state.session_id === '0000000000000000' &&
        state.remote_control_available === false) {
      if (pendingCommand) markPendingUnknown('设备随机会话 ID 不可用；旧命令结果未知。');
      activeSessionId = state.session_id;
      latestState = state;
      lastLiveStateAt = packet?.retain ? 0 : Date.now();
      render();
      return;
    }

    const liveConfirmationOfRetainedState = lastLiveStateAt === 0 &&
      latestState?.session_id === state.session_id &&
      latestState.seq === state.seq;
    if (liveConfirmationOfRetainedState) {
      activeSessionId = state.session_id;
      latestState = state;
      lastLiveStateAt = Date.now();
      render();
      return;
    }

    if (activeSessionId && state.session_id !== activeSessionId) {
      if (pendingCommand) markPendingUnknown('设备会话改变，旧命令结果未知。');
      addEvent('检测到新的 F407 启动会话；当前会话序号重新开始。');
    } else if (latestState?.session_id === state.session_id &&
      Number.isSafeInteger(latestState.seq) && state.seq <= latestState.seq) {
      addEvent('忽略同一会话中重复或较旧的快照；不刷新应用心跳。');
      return;
    }

    activeSessionId = state.session_id;
    latestState = state;
    lastLiveStateAt = Date.now();
    render();
  }

  function showAcknowledgement(acknowledgement, matched) {
    dom['ack-empty'].hidden = true;
    dom['latest-ack'].hidden = false;
    setText(dom['ack-result'], acknowledgement.result);
    dom['ack-result'].className = acknowledgement.result === 'OK' ? 'success' : 'warning';
    setText(dom['ack-command'], acknowledgement.command);
    setText(dom['ack-id'], acknowledgement.command_id);
    setText(dom['ack-time'], new Date().toLocaleTimeString());
    const readback = acknowledgement.readback;
    const readbackText = readback
      ? [
          Array.isArray(readback.led)
            ? 'LED ' + readback.led.map((value) => value ? '开' : '关').join('/')
            : null,
          typeof readback.buzzer_gpio_high === 'boolean'
            ? '蜂鸣器 GPIO ' + (readback.buzzer_gpio_high ? '高' : '低')
            : null
        ].filter(Boolean).join(' · ')
      : '';
    setText(dom['ack-readback'], readbackText || '无 GPIO 回读字段');
    addEvent((matched ? '命令回执：' : '收到未匹配的回执：') +
      acknowledgement.command + ' → ' + acknowledgement.result);
  }

  function handleAcknowledgement(payload) {
    const acknowledgement = decodeJson(payload);
    if (!acknowledgement || !validAcknowledgement(acknowledgement)) {
      addEvent(acknowledgement?.schema_version !== 1
        ? '回执消息版本不受支持。'
        : '收到格式无效的回执。');
      return;
    }

    const matched = pendingCommand &&
      acknowledgement.command_id.toLowerCase() === pendingCommand.command.command_id.toLowerCase() &&
      acknowledgement.session_id.toLowerCase() === pendingCommand.command.session_id.toLowerCase();
    showAcknowledgement(acknowledgement, Boolean(matched));
    if (matched) clearPending();
  }

  function handleMessage(topic, payload, packet) {
    if (topic === TOPICS.availability) return parseAvailability(payload, packet);
    if (topic === TOPICS.state) return handleState(payload, packet);
    if (topic === TOPICS.acknowledgement) return handleAcknowledgement(payload);
  }

  function appIsFresh() {
    return lastLiveStateAt > 0 && Date.now() - lastLiveStateAt <= HEARTBEAT_STALE_MS;
  }

  function canControl() {
    return Boolean(mqttClient?.connected &&
      subscriptionsReady &&
      availability === 'online' &&
      appIsFresh() &&
      latestState?.remote_control_available === true &&
      isSessionId(latestState?.session_id) &&
      !pendingCommand);
  }

  function render() {
    const fresh = appIsFresh();
    if (latestState) {
      setText(dom['wifi-state'], labelWifi(latestState.wifi.sta, fresh));
      setText(dom['wifi-ip'], latestState.wifi.ip || '尚无可用 IP');
      setText(dom['bluetooth-state'], latestState.bt_connected ? '已连接' : '未连接');
      setText(dom['session-id'], latestState.session_id);
      setText(dom['state-sequence'], latestState.seq);
      setText(dom['uptime'], formatUptime(latestState.uptime_s));
      setText(dom['dropped-count'], latestState.dropped_snapshots);
      setText(dom['remote-state'], latestState.remote_control_available ? '可用' : '关闭');
      latestState.led.forEach((enabled, index) => {
        const led = dom['led-' + index];
        setText(led, enabled ? 'GPIO 高' : 'GPIO 低');
        led.className = 'actuator-value ' + (enabled ? 'on' : 'off');
      });
      setText(dom['buzzer-state'], latestState.buzzer_gpio_high ? 'GPIO 高' : 'GPIO 低');
      dom['buzzer-state'].className = 'actuator-value ' +
        (latestState.buzzer_gpio_high ? 'on' : 'off');
      dom['freshness-badge'].className = 'freshness-badge ' + (fresh ? 'fresh' : 'stale');
      setText(dom['freshness-badge'], fresh ? '实时' : (lastLiveStateAt ? '陈旧' : '最近已知'));
    }

    setText(dom['transport-state'], labelAvailability(availability));
    setText(dom['transport-detail'], availability === 'online'
      ? 'F407 应用与本地 Broker 会话已建立'
      : availability === 'offline'
        ? 'F407 应用 MQTT 会话已断开'
        : '等待设备 availability');
    setText(dom['application-state'], fresh
      ? '在线'
      : latestState
        ? (lastLiveStateAt ? '陈旧' : '最近已知')
        : '未收到状态');
    setText(dom['heartbeat-detail'], lastLiveStateAt
      ? '最近实时心跳 ' + secondsAgo(lastLiveStateAt) + ' 秒前'
      : '尚未收到当前会话的实时心跳');

    const controlsEnabled = canControl();
    for (const button of commandButtons) button.disabled = !controlsEnabled;
    setText(dom['controls-state'], controlsEnabled
      ? '设备与当前应用会话可用'
      : pendingCommand
        ? (pendingCommand.unknown ? '等待用户处理未知结果' : '等待当前命令 ACK')
        : '等待实时状态、链路或安全会话');

    renderPending();
  }

  function labelWifi(value, fresh) {
    const labels = {
      connected: '已连接',
      disconnected: '未连接',
      unknown: '未知'
    };
    return (fresh ? '' : '最近已知 · ') + (labels[value] || '未知');
  }

  function labelAvailability(value) {
    if (value === 'online') return '在线';
    if (value === 'offline') return '离线';
    return '未知';
  }

  function formatUptime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const remainingSeconds = seconds % 60;
    return hours + 'h ' + minutes + 'm ' + remainingSeconds + 's';
  }

  function secondsAgo(timestamp) {
    return Math.max(0, Math.floor((Date.now() - timestamp) / 1000));
  }

  function makeCommand(command, extra = {}) {
    if (!localCryptoReady || !latestState || !activeSessionId) {
      addEvent('浏览器随机数或当前设备会话不可用；没有发送命令。');
      return null;
    }
    const commandId = randomHex64();
    if (!commandId) {
      localCryptoReady = false;
      addEvent('浏览器随机数生成失败；远程命令已关闭。');
      render();
      return null;
    }
    return {
      schema_version: 1,
      device_id: DEVICE_ID,
      session_id: activeSessionId,
      command_id: commandId,
      command,
      ...extra
    };
  }

  function sendNewCommand(commandName, extra = {}) {
    if (!canControl()) {
      addEvent('设备当前不满足命令发送条件。');
      return;
    }
    const command = makeCommand(commandName, extra);
    if (!command) return;
    pendingCommand = { command, sends: 0, timer: null, unknown: false };
    publishPendingCommand();
  }

  function publishPendingCommand() {
    if (!pendingCommand || !mqttClient?.connected || !appIsFresh() || availability !== 'online') {
      markPendingUnknown('链路或应用心跳中断；为避免离线排队，停止重发。');
      return;
    }
    const current = pendingCommand;
    current.sends += 1;
    mqttClient.publish(TOPICS.command, JSON.stringify(current.command), {
      qos: 1,
      retain: false
    }, (error) => {
      if (pendingCommand !== current) return;
      if (error) {
        markPendingUnknown('Broker 未确认本次发布；命令结果未知。');
        return;
      }
      setText(dom['pending-title'], '等待设备回执');
      setText(dom['pending-detail'], commandLabel(current.command) +
        ' · 第 ' + current.sends + '/' + MAX_SENDS + ' 次发送 · ID ' +
        current.command.command_id);
      render();
    });

    clearTimeout(current.timer);
    current.timer = setTimeout(() => {
      if (pendingCommand !== current) return;
      if (current.sends < MAX_SENDS && canRetry()) {
        publishPendingCommand();
      } else {
        markPendingUnknown('已有限重发 ' + current.sends + ' 次，未收到设备 ACK。');
      }
    }, RETRY_DELAY_MS);
    render();
  }

  function canRetry() {
    return Boolean(mqttClient?.connected &&
      availability === 'online' &&
      appIsFresh() &&
      latestState?.session_id === activeSessionId);
  }

  function markPendingUnknown(reason) {
    if (!pendingCommand) return;
    clearTimeout(pendingCommand.timer);
    pendingCommand.timer = null;
    pendingCommand.unknown = true;
    pendingCommand.reason = reason;
    addEvent('命令结果未知：' + reason);
    render();
  }

  function clearPending() {
    if (pendingCommand?.timer) clearTimeout(pendingCommand.timer);
    pendingCommand = null;
    render();
  }

  function renderPending() {
    if (!pendingCommand) {
      dom['pending-command'].hidden = true;
      dom['clear-unknown'].hidden = true;
      return;
    }
    dom['pending-command'].hidden = false;
    dom['clear-unknown'].hidden = !pendingCommand.unknown;
    setText(dom['pending-title'], pendingCommand.unknown ? '结果未知' : '等待设备回执');
    setText(dom['pending-detail'], (pendingCommand.reason || commandLabel(pendingCommand.command)) +
      ' · ID ' + pendingCommand.command.command_id);
  }

  function commandLabel(command) {
    if (command.command === 'led.set') {
      return 'LED ' + (command.led_index + 1) + ' ' + (command.value === 'on' ? '开' : '关');
    }
    if (command.command === 'buzzer.pulse') return '蜂鸣器 ' + command.duration_ms + ' ms';
    return '查询设备状态';
  }

  function connectDashboard() {
    if (!globalThis.mqtt || typeof globalThis.mqtt.connect !== 'function') {
      setDashboardConnection('本机 MQTT 客户端未加载', 'offline');
      addEvent('找不到本地 MQTT.js 浏览器包；请在项目目录安装依赖。');
      return;
    }
    const clientId = randomHex64();
    if (!clientId) {
      setDashboardConnection('浏览器随机数不可用', 'offline');
      addEvent('无法生成 Dashboard 客户端 ID。');
      return;
    }

    mqttClient = globalThis.mqtt.connect('ws://' + location.host + '/mqtt', {
      clientId: 'dash-' + clientId,
      protocolVersion: 4,
      clean: true,
      reconnectPeriod: 1000,
      connectTimeout: 5000,
      resubscribe: false
    });

    mqttClient.on('connect', () => {
      subscriptionsReady = false;
      setDashboardConnection('本机 Broker 已连接', 'online');
      mqttClient.subscribe([
        TOPICS.availability,
        TOPICS.state,
        TOPICS.acknowledgement
      ], { qos: 1 }, (error) => {
        if (error) {
          addEvent('订阅设备主题失败。');
          setDashboardConnection('主题订阅失败', 'offline');
          return;
        }
        subscriptionsReady = true;
        addEvent('已订阅设备状态、availability 和 ACK。');
        render();
      });
    });
    mqttClient.on('message', handleMessage);
    mqttClient.on('reconnect', () => {
      subscriptionsReady = false;
      setDashboardConnection('正在重连本机 Broker…', '');
      if (pendingCommand && !pendingCommand.unknown) {
        markPendingUnknown('Dashboard 到 Broker 的链路中断；已停止重发。');
      }
      render();
    });
    mqttClient.on('close', () => {
      subscriptionsReady = false;
      setDashboardConnection('本机 Broker 未连接', 'offline');
      render();
    });
    mqttClient.on('error', () => {
      setDashboardConnection('本机 Broker 连接错误', 'offline');
    });
  }

  function attachControls() {
    for (const button of ledButtons) {
      button.addEventListener('click', () => {
        sendNewCommand('led.set', {
          led_index: Number(button.dataset.index),
          value: button.dataset.value
        });
      });
    }

    dom['buzzer-button'].addEventListener('click', () => {
      const duration = Number(dom['buzzer-duration'].value);
      if (!Number.isInteger(duration) || duration < 100 || duration > 1000) {
        addEvent('蜂鸣时长必须为 100–1000 ms 的整数。');
        return;
      }
      sendNewCommand('buzzer.pulse', { duration_ms: duration });
    });

    dom['get-state-button'].addEventListener('click', () => sendNewCommand('system.get'));
    dom['clear-unknown'].addEventListener('click', () => {
      if (pendingCommand?.unknown) {
        addEvent('用户解除未知命令锁定；之前命令是否执行仍未确认。');
        clearPending();
      }
    });
  }

  localCryptoReady = randomHex64() !== null;
  if (!localCryptoReady) addEvent('浏览器不支持安全随机数；远程控制不可用。');
  attachControls();
  render();
  connectDashboard();
  setInterval(render, 1000);
})();
