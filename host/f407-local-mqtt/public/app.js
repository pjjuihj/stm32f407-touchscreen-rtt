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
    'ack-id', 'ack-time', 'event-count', 'event-list', 'history-persistence',
    'history-wifi', 'history-mqtt', 'history-count', 'history-coverage',
    'history-capacity-dropped', 'history-device-dropped', 'history-last-live',
    'history-ack-count', 'history-ack-min', 'history-ack-median', 'history-ack-max',
    'history-list', 'history-empty', 'history-load-more', 'history-export'
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
  let availabilityLiveAt = 0;
  let latestState = null;
  let lastLiveStateAt = 0;
  let activeSessionId = null;
  let pendingCommand = null;
  let eventTotal = 0;
  let localCryptoReady = false;
  let historyRows = [];
  let historyVisibleCount = 100;
  let historyPersistent = true;
  let historyRefreshTimer = null;
  let historyRecorder = null;
  let historyLiveState = null;
  let historyKnownState = null;
  let historyKnownAvailability = null;

  const historyStore = globalThis.F407HistoryStore.create({
    onStatus: (status) => {
      historyPersistent = status.persistent === true;
      updateHistoryPersistence();
      if (historyRecorder) historyRecorder.observeStorage(status);
      scheduleHistoryRefresh();
    }
  });
  historyRecorder = globalThis.F407HistoryRecorder.create({ store: historyStore });
  void historyStore.ready.then(async () => {
    await historyRecorder.ready;
    historyRecorder.observeStorage(await historyStore.getStatus());
    await refreshHistory();
  });

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
      (!Object.hasOwn(value, 'seq') || (Number.isSafeInteger(value.seq) && value.seq > 0)) &&
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
    if (packet?.retain !== true) availabilityLiveAt = Date.now();
    historyRecorder.observeAvailability(value, { retain: packet?.retain === true });
    scheduleHistoryRefresh();
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
      historyRecorder.observeState(state, packet);
      scheduleHistoryRefresh();
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
      historyRecorder.observeState(state, packet);
      scheduleHistoryRefresh();
      if (pendingCommand) markPendingUnknown('设备随机会话 ID 不可用；旧命令结果未知。');
      activeSessionId = state.session_id;
      latestState = state;
      lastLiveStateAt = packet?.retain ? 0 : Date.now();
      render();
      return;
    }

    const liveConfirmationOfRetainedState = lastLiveStateAt === 0 &&
      latestState?.session_id === state.session_id &&
      Number.isSafeInteger(latestState.seq) && Number.isSafeInteger(state.seq) &&
      latestState.seq === state.seq;
    if (liveConfirmationOfRetainedState) {
      historyRecorder.observeState(state, packet);
      scheduleHistoryRefresh();
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

    historyRecorder.observeState(state, packet);
    scheduleHistoryRefresh();
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
    const latencyMs = matched && Number.isFinite(pendingCommand.firstPublishedAt)
      ? Math.max(0, performance.now() - pendingCommand.firstPublishedAt)
      : null;
    historyRecorder.observeAck(acknowledgement, {
      matched: Boolean(matched),
      latencyMs,
      receivedAt: Date.now()
    });
    scheduleHistoryRefresh();
    showAcknowledgement(acknowledgement, Boolean(matched));
    if (matched) clearPending();
  }

  function handleMessage(topic, payload, packet) {
    if (topic === TOPICS.availability) return parseAvailability(payload, packet);
    if (topic === TOPICS.state) return handleState(payload, packet);
    if (topic === TOPICS.acknowledgement) return handleAcknowledgement(payload);
  }

  function scheduleHistoryRefresh() {
    clearTimeout(historyRefreshTimer);
    historyRefreshTimer = setTimeout(() => { void refreshHistory(); }, 100);
  }

  async function refreshHistory() {
    try {
      const [rows, status] = await Promise.all([historyStore.list(), historyStore.getStatus()]);
      historyRows = rows;
      historyPersistent = status.persistent;
      historyLiveState = newestByReceivedAt(rows,
        (row) => row.type === 'state' && row.countsAsLive);
      historyKnownState = newestByReceivedAt(rows,
        (row) => (row.type === 'state' && row.countsAsLive) || row.type === 'state-snapshot');
      historyKnownAvailability = newestByReceivedAt(rows, (row) => row.type === 'availability');
      updateHistoryPersistence();
      renderHistorySummary(status);
      renderHistoryRows();
    } catch {
      historyPersistent = false;
      updateHistoryPersistence();
    }
  }

  function newestByReceivedAt(rows, predicate) {
    let newest = null;
    for (const row of rows) {
      if (predicate(row) && (!newest || row.receivedAt > newest.receivedAt)) newest = row;
    }
    return newest;
  }

  function updateHistoryPersistence() {
    setText(dom['history-persistence'], historyPersistent
      ? '历史记录保存在此浏览器的本机数据中，最多保留 24 小时或 5,000 条。'
      : '历史记录未能持久保存。当前只保留本页内存中的有限记录；刷新后可能丢失。');
    dom['history-persistence'].className = 'history-persistence ' +
      (historyPersistent ? 'persistent' : 'warning');
  }

  function formatHistoryDuration(milliseconds) {
    const minutes = Math.floor(Math.max(0, milliseconds) / 60000);
    if (minutes < 60) return minutes + ' 分钟';
    const hours = Math.floor(minutes / 60);
    const remainingMinutes = minutes % 60;
    if (hours < 24) return hours + ' 小时 ' + remainingMinutes + ' 分钟';
    return Math.floor(hours / 24) + ' 天 ' + (hours % 24) + ' 小时';
  }

  function availabilitySummary(event) {
    if (!event) return '尚无 MQTT availability 记录';
    const isFresh = availabilityLiveAt > 0 && Date.now() - availabilityLiveAt <= HEARTBEAT_STALE_MS;
    const value = isFresh ? availability : event.data?.value;
    const label = value === 'online' ? '在线' : value === 'offline' ? '离线' : '未知';
    return (isFresh ? '实时 · ' : '最近已知 · ') + label;
  }

  function renderHistorySummary(status) {
    const stateEvent = historyKnownState;
    if (!stateEvent) {
      setText(dom['history-wifi'], '尚无 Wi‑Fi/IP 记录');
      setText(dom['history-device-dropped'], '尚无设备丢弃计数');
    } else {
      const data = stateEvent.data || {};
      const currentIsFresh = appIsFresh() && latestState !== null;
      const currentStateData = currentIsFresh ? latestState : data;
      const wifi = currentStateData.wifi || {};
      const source = currentIsFresh ? '实时 · ' : '最近已知 · ';
      setText(dom['history-wifi'], source + 'STA ' + (wifi.sta || '未知') +
        ' · IP ' + (wifi.ip || '未分配'));
      setText(dom['history-device-dropped'], Number.isSafeInteger(currentStateData.dropped_snapshots)
        ? currentStateData.dropped_snapshots
        : '未知');
    }

    setText(dom['history-mqtt'], availabilitySummary(historyKnownAvailability));
    setText(dom['history-count'], status.count + ' 条');
    setText(dom['history-coverage'], status.oldestAt === null
      ? '暂无记录'
      : new Date(status.oldestAt).toLocaleString() + ' · ' + formatHistoryDuration(status.coverageMs));
    setText(dom['history-capacity-dropped'], status.capacityDropped);
    setText(dom['history-last-live'], historyLiveState
      ? new Date(historyLiveState.receivedAt).toLocaleString() +
        ' · ' + Math.max(0, Math.floor((Date.now() - historyLiveState.receivedAt) / 1000)) + ' 秒前'
      : '尚无实时状态');

    const latencies = historyRows
      .filter((row) => row.type === 'ack' && typeof row.data?.latency_ms === 'number')
      .map((row) => row.data.latency_ms)
      .sort((left, right) => left - right);
    setText(dom['history-ack-count'], latencies.length);
    if (!latencies.length) {
      for (const id of ['history-ack-min', 'history-ack-median', 'history-ack-max']) setText(dom[id], '未知');
    } else {
      const middle = Math.floor(latencies.length / 2);
      const median = latencies.length % 2
        ? latencies[middle]
        : (latencies[middle - 1] + latencies[middle]) / 2;
      setText(dom['history-ack-min'], latencies[0].toFixed(1) + ' ms');
      setText(dom['history-ack-median'], median.toFixed(1) + ' ms');
      setText(dom['history-ack-max'], latencies[latencies.length - 1].toFixed(1) + ' ms');
    }
  }

  function describeHistoryEvent(event) {
    const data = event.data || {};
    if (event.type === 'state-snapshot') return 'Broker 保留的最近已知状态快照';
    if (event.type === 'state') {
      const wifi = data.wifi || {};
      const led = Array.isArray(data.led) ? data.led.map((value) => value ? '开' : '关').join('/') : '未知';
      const reason = event.reason === 'sample' ? '每分钟采样' :
        event.reason === 'initial' ? '本会话首条状态' :
          event.reason === 'change+sample' ? '状态变化与定时采样' : '状态变化';
      const dropped = data.droppedDelta ? ' · 本次新增丢弃 ' + data.droppedDelta : '';
      return reason + ' · Wi‑Fi ' + (wifi.sta || '未知') +
        ' / IP ' + (wifi.ip || '未分配') + ' · LED ' + led + dropped;
    }
    if (event.type === 'availability') {
      return (event.retained ? '最近已知 MQTT availability · ' : 'MQTT availability · ') +
        (data.value === 'online' ? '在线' : data.value === 'offline' ? '离线' : '未知');
    }
    if (event.type === 'ack') {
      return 'ACK · ' + (data.command || '命令') + ' · ' + (data.result || '未知') +
        (typeof data.latency_ms === 'number' ? ' · ' + data.latency_ms.toFixed(1) + ' ms' : ' · 未匹配');
    }
    return '本地历史改为仅内存保存';
  }

  function renderHistoryRows() {
    const rows = [...historyRows].reverse().slice(0, historyVisibleCount);
    dom['history-list'].replaceChildren();
    for (const event of rows) {
      const row = document.createElement('li');
      const time = document.createElement('time');
      const body = document.createElement('span');
      time.dateTime = new Date(event.receivedAt).toISOString();
      time.textContent = new Date(event.receivedAt).toLocaleString();
      body.textContent = describeHistoryEvent(event);
      row.append(time, body);
      dom['history-list'].append(row);
    }
    dom['history-empty'].hidden = historyRows.length > 0;
    dom['history-load-more'].hidden = historyRows.length <= historyVisibleCount;
    dom['history-load-more'].disabled = historyRows.length <= historyVisibleCount;
  }

  function exportHistoryCsv() {
    void historyStore.list().then((rows) => {
      const blob = new Blob([globalThis.F407HistoryStore.toCsv(rows)], {
        type: 'text/csv;charset=utf-8'
      });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = 'f407-reliability-history.csv';
      document.body.append(link);
      link.click();
      link.remove();
      setTimeout(() => URL.revokeObjectURL(url), 0);
    }).catch(() => {
      addEvent('CSV 导出未完成；历史仍可在本页查看。');
    });
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
      Number.isSafeInteger(latestState?.seq) && latestState.seq > 0 &&
      !pendingCommand);
  }

  function render() {
    const fresh = appIsFresh();
    if (latestState) {
      setText(dom['wifi-state'], labelWifi(latestState.wifi.sta, fresh));
      setText(dom['wifi-ip'], latestState.wifi.ip || '尚无可用 IP');
      setText(dom['bluetooth-state'], latestState.bt_connected ? '已连接' : '未连接');
      setText(dom['session-id'], latestState.session_id);
      setText(dom['state-sequence'], Number.isSafeInteger(latestState.seq) ? latestState.seq : '未知');
      setText(dom['uptime'], formatUptime(latestState.uptime_s));
      setText(dom['dropped-count'], latestState.dropped_snapshots);
      setText(dom['remote-state'], latestState.remote_control_available
        ? Number.isSafeInteger(latestState.seq) && latestState.seq > 0 ? '可用' : '缺少序号，已禁用'
        : '关闭');
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
    renderHistoryFreshness();
  }

  function renderHistoryFreshness() {
    if (historyKnownState) {
      const data = historyKnownState.data || {};
      const fresh = appIsFresh() && latestState !== null;
      const currentStateData = fresh ? latestState : data;
      const wifi = currentStateData.wifi || {};
      setText(dom['history-wifi'], (fresh ? '实时 · ' : '最近已知 · ') +
        'STA ' + (wifi.sta || '未知') + ' · IP ' + (wifi.ip || '未分配'));
    }
    if (historyKnownAvailability) {
      setText(dom['history-mqtt'], availabilitySummary(historyKnownAvailability));
    }
    setText(dom['history-last-live'], historyLiveState
      ? new Date(historyLiveState.receivedAt).toLocaleString() +
        ' · ' + Math.max(0, Math.floor((Date.now() - historyLiveState.receivedAt) / 1000)) + ' 秒前'
      : '尚无实时状态');
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
    pendingCommand = { command, sends: 0, timer: null, unknown: false, firstPublishedAt: null };
    publishPendingCommand();
  }

  function publishPendingCommand() {
    if (!pendingCommand || !mqttClient?.connected || !appIsFresh() || availability !== 'online') {
      markPendingUnknown('链路或应用心跳中断；为避免离线排队，停止重发。');
      return;
    }
    const current = pendingCommand;
    current.sends += 1;
    if (current.firstPublishedAt === null) current.firstPublishedAt = performance.now();
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
    dom['history-load-more'].addEventListener('click', () => {
      historyVisibleCount = Math.min(historyRows.length, historyVisibleCount + 100);
      renderHistoryRows();
    });
    dom['history-export'].addEventListener('click', exportHistoryCsv);
  }

  localCryptoReady = randomHex64() !== null;
  if (!localCryptoReady) addEvent('浏览器不支持安全随机数；远程控制不可用。');
  attachControls();
  render();
  connectDashboard();
  setInterval(render, 1000);
})();
