# STM32F407 触摸屏本地 MQTT 远程运维终端设计

- 日期：2026-10-29
- 状态：第一版 F407 MQTT 源码、主机 Broker 与仪表板已实现；离线 CMake 测试 18/18 通过，主机模拟设备集成测试 1/1 通过。最终 MQTT 镜像 attempt 69 已写入并校验，但板端 MQTT 验收未通过：Wi-Fi/IP 就绪，ESP 对三次自动 TCP 建连都返回通用 `ERROR`，没有设备 availability 或心跳。后续 90 秒诊断未定位根因。之后 attempt 70 已将隔离 MQTT 自动调度的诊断镜像写入并校验；当前板上镜像不是最终 MQTT 镜像。attempt 70 只确认 STA=CONNECTED、IP=READY、TCP=DISCONNECTED；TCP/MUX/模式查询未收到最终异步结果，所以条件 TCP 连接未执行。attempt 70 后屏幕、触摸和蓝牙尚未复核；第一版的板端 MQTT 验收仍未完成。
- 范围：复用现有 STM32F407 触摸屏板、ESP8266 和电脑；不增加硬件，不部署云端

## 目标

把触摸屏板扩展成一个可在可信局域网查看和控制的 IoT 节点。电脑运行本地 MQTT Broker 和只在本机打开的浏览器仪表板；设备上报运行与连接状态，用户可有限地控制三路 LED 和蜂鸣器。

项目展示重点是边缘设备的在线状态区分、断网状态补报、会话隔离和可核对的命令结果。Wi-Fi/IP、F407 的 MQTT 会话和 F407 应用心跳分别表示不同事实，仪表板不得把其中一个状态冒充成另一个。

## 现有基础与差异

- `IoT_Status` 已包含设备 ID、运行时间、Wi-Fi/蓝牙连接标志、三路 LED 和蜂鸣器状态；`board_io.c` 可读回 MCU GPIO 电平。`IoT_HandleCommand()` 提供 LED、蜂鸣器和系统查询接口，但也支持 MQTT 首版不开放的 toggle 动作。
- 主程序以 `IoT_Init(NULL)` 初始化；首版在空参数时使用逻辑 ID `f407-touchscreen-01`。它只是 MQTT 命名空间 ID，不是硬件身份认证。
- 蓝牙命令会改动与 MQTT 共用的 LED/蜂鸣器状态。USART1 控制台当前用于状态和诊断；本设计不把它列为执行器命令来源。多来源有效命令按设备接受顺序处理，最后接受的 `set` 决定输出状态。
- F407 MQTT 客户端现位于 `IoT/mqtt_client.c`、`IoT/mqtt_wire.c` 和 `IoT/mqtt_json.c`；主机 Broker 与仪表板位于 `host/f407-local-mqtt/`。此前串口曾报告 ESP AT `1.7.4.0`、SDK `3.0.5-dev`；该记录不证明当前定制镜像包含 MQTT AT 命令。
- STM32F407 有硬件 RNG；它用于生成每次启动的短期会话 ID。若 RNG 不可用，遥测使用保留的全零标记供页面显示故障，远程命令保持关闭；该标记不代表唯一启动会话，页面不得据此开放控制。[ST RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)

## 方案与实施门槛

F407 实现 MQTT 3.1.1 客户端，ESP8266 只提供现有 AT Wi-Fi/TCP 套接字。F407 通过既有 TCP connect/send 接口连接本机 Broker；ESP 的 `+IPD` 数据由有界解析器拆出后交给 MQTT 分帧器。该选择不依赖定制 ESP 镜像提供 MQTT AT 命令，也不自动刷写 ESP。

attempt 65 在 `AT` 探测成功后只读取一次 `AT+CMD?`，串口被动采集结果为 `UNKNOWN / QUERY_REJECTED_OR_UNSUPPORTED`。这既不能证明固件支持 MQTT AT，也不能证明不支持；因此第一版不把 ESP 的 AT 命令清单作为 F407 MQTT 客户端的实施门槛。若 TCP 收发或 `+IPD` 行为在板端不符合预期，先停止并核查现有套接字路径，不自动改刷 ESP。

## 组成与边界

1. **F407 应用**维护 MCU 侧 LED/蜂鸣器状态、Wi-Fi/IP 最近已知状态、蓝牙状态、启动会话和遥测队列；所有处理保持主循环有界，不能阻塞 LVGL 与触摸。
2. **ESP8266**通过 UART4 提供 Wi-Fi 和 AT TCP 套接字；MQTT 会话与协议状态由 F407 管理。F407 重启时 ESP 可能仍保持 Wi-Fi 连接，但新的 F407 会话仍须重新建立 MQTT 连接。
3. **本地 Broker**只接受来自受控热点的设备连接；**浏览器仪表板**只在 Broker 所在电脑的本地地址开放，不向热点上的其他客户端提供网页控制入口。Broker 和仪表板的具体软件在实施计划中依照现有环境选定。
4. 不配置云端、不改 Windows 防火墙或 ICS。若现有网络策略阻止板子访问本地 Broker，停止并报告，不自行改网络策略。连接本地 Broker 是指定的应用传输，不代表或触发通用 Internet 探测。
5. 首版假设热点由项目所有者控制且只有可信设备。由于不启用 MQTT 应用层认证，不能把此配置用于校园网、共享热点或任何不可信网络；扩大使用范围前先加入 Broker 身份验证与主题 ACL。

## 身份、状态与 MQTT 主题

设备逻辑 ID 固定为 `f407-touchscreen-01`。每次 F407 启动从硬件 RNG 取得两个 32 位随机值，组合成新的 64 位 `session_id`。每个会话的 `seq` 从 1 开始。若 RNG 状态错误，设备公布远程控制不可用，不沿用旧 `session_id`。

主题根为 `f407/f407-touchscreen-01/`：

| 主题 | 方向与交付 | 含义 |
|---|---|---|
| `availability` | F407/Broker，QoS 1，保留 | 表示 F407 MQTT TCP 会话；连接建立后为 `online`，意外断连由 Broker 遗嘱更新为 `offline`。状态心跳仍用于判断 F407 应用是否持续运行。 |
| `state` | 设备到 Broker，QoS 1，保留最新值 | F407 状态快照；含 `session_id`、`seq`、`uptime_s`、Wi-Fi/IP、蓝牙、三路 LED、蜂鸣器、丢弃计数和远程控制可用标志。 |
| `cmd` | Broker 到设备，QoS 1，不保留、不离线排队 | 当前会话的受限命令。设备不执行会话 ID 不匹配的消息；非持久 MQTT 会话确保离线期间不缓存订阅命令。 |
| `ack` | 设备到 Broker，QoS 1，不保留 | 按 `session_id` 和命令 ID 对应的处理结果。 |

每条 `state` 都是包含该时刻全部上报字段的完整快照，不是只带变化项的增量。接收端收到后可以直接替换当前视图；Broker 保留的最新消息也能独立恢复显示，不依赖先前快照。

## 消息编码与长度

所有 MQTT payload 使用 UTF-8 JSON，便于在本地 Broker 和仪表板中检查。发送端和接收端都必须执行字节长度上限和字段校验；格式错误、超长或字段非法的命令不得触发执行器动作。

`state`、`cmd`、`ack` JSON 均带 `schema_version: 1`；简单的 `availability` 状态值不使用 JSON。F407 只执行支持版本的命令；遇到未知命令版本时拒绝执行。仪表板遇到未知的状态或回执版本时显示“不支持的消息版本”，不得按旧格式猜测解析。

F407 侧 MQTT 报文和 JSON 都有固定缓冲上限；超长、非法或格式错误的输入不触发执行器。ESP AT 仍负责 TCP 封装和链路收发，因此最终可用长度也受该固件与现有 UART 缓冲能力限制，板端互通验收前不宣称已验证其上限。

F407 每 10 秒发布一次状态心跳，状态变化时立即补一份快照。仪表板分开显示 Wi-Fi/IP、F407 MQTT 会话 availability 和 F407 应用心跳。F407 应用连续 30 秒未上报当前会话心跳时显示应用状态陈旧。新订阅收到 Broker 重放的保留快照时，先显示为最近已知；收到当前会话的实时状态后才显示为新鲜数据，不能用“刚收到保留消息”的时间冒充设备刚上报。

## 命令模型与结果

首版只接受：

- `led.set`：LED 索引 0–2，明确设置开或关；不接受 toggle。
- `buzzer.pulse`：默认 500 ms，允许 100–1000 ms；两次响铃至少间隔 2 秒。冷却期内返回 `RATE_LIMITED`，不排队补响，也不提供持续鸣叫命令。
- `system.get`：只读返回当前状态。

每条命令包含当前 `session_id` 和新的随机 64 位命令 ID。MQTT QoS 1 的重复投递或仪表板重试必须复用原 ID；设备对最近 8 个命令在 RAM 中去重并重发原 ACK，不再次执行。去重状态在 F407 重启时清空，但新会话 ID 会使旧会话命令失效；不向 Flash 写入命令历史。

仪表板同一时间只允许一条待确认命令。推荐首版在 2 秒未收到 ACK 时重发同一 ID，最多重发 2 次；仍无 ACK 则显示“结果未知”，由用户决定下一步，不自动生成新 ID 或发出新动作。ACK 报告命令结果和 LED/蜂鸣器 GPIO 读回值；它不代表用传感器独立测得 LED 已发光或蜂鸣器已发声。

蓝牙与 MQTT 可作用于同一执行器；对同一执行器，设备接受顺序定义命令次序，最后一个有效 `set` 生效。仪表板不持有控制租约，MQTT 命令仅在 F407 MQTT 会话与当前 F407 应用会话均可用时发送。

## 断网与恢复

- MQTT 不可用时，本地显示和已有蓝牙功能继续运行；远程命令不保留、不排队，用户须等重新在线后重发。
- 在 RAM 中按顺序保留最近 16 条状态快照。链路恢复后先补报积压，再发布当前状态；队列满时丢弃最旧快照并递增丢弃计数。
- 该遥测积压只覆盖当前 F407 启动会话；F407 重启会清空它，新会话连上后只报告当前状态，不声称恢复了断电前历史。
- Broker 保留最新状态与 F407 MQTT availability，方便稍后打开仪表板；仪表板结合 availability 和当前会话心跳区分传输在线、应用陈旧及最近已知数据。命令主题永不保留。
- ESP 只负责 Wi-Fi 和 AT TCP 套接字；F407 的 MQTT 客户端状态机单独拥有 MQTT 连接重试。Wi-Fi 重新可用后，F407 按退避时间重新建立 TCP/MQTT 会话，不向 ESP 发送 MQTT AT 命令。

## 验收

### 离线主机侧

- 状态、命令、ACK 编码/解析覆盖长度边界、格式错误、未知主题和非法参数；输入只允许有限命令，不得解释为任意 AT 或控制台文本。
- 每条 `state` 都能独立还原完整设备视图，不要求先收到较早消息；`seq` 只用于识别会话内顺序。
- 会话 ID 不匹配时拒绝；同 ID 重发不重复执行；随机数失败关闭远程命令；每次最多一个待确认命令；ACK 有限重发后呈现未知结果。
- LED 索引、蜂鸣器范围和 2 秒冷却正确；GPIO 读回值准确进入 ACK。
- 16 条遥测队列覆盖 FIFO、满队列丢弃计数、补报后当前快照和 F407 重启丢弃语义。
- 传输 availability 与 F407 应用心跳独立；Broker 保留消息不会被仪表板误认作实时值。
- `state`、`cmd`、`ack` 均携带 schema version；不支持的命令版本不触发动作，仪表板不误读未知状态或回执版本。

### 局域网板端

- 核对 F407 目标身份、Broker 地址和消息方向；不因 MQTT AT 清单未知而刷写 ESP。先验证主机 Broker，再分别验证 F407 TCP/MQTT 握手与应用层消息。
- 设备上线后每 10 秒有当前会话状态；Broker 遗嘱只表示 F407 的 MQTT 连接，不冒充应用心跳。
- LED 设置、短蜂鸣和状态查询返回匹配会话及命令 ID 的 ACK；重复 ID 不再次触发，过快蜂鸣返回 `RATE_LIMITED`。
- Wi-Fi/MQTT 断开 60 秒期间，触摸屏保持响应；恢复后按约定补报，不执行离线命令。
- F407 单独复位时，ESP 的 Wi-Fi 可能仍连接，但 F407 会重建 MQTT 会话；仪表板在心跳过期后标记应用陈旧，新会话状态到达后恢复。
- 连续运行 10 分钟观察屏幕与触摸；硬件 GPIO 读回、屏幕显示及听觉体验分别报告，不把主机测试等同实机验收。

## 当前实现进度（2026-10-30）

电脑端第一版位于 `host/f407-local-mqtt/`：Aedes Broker 只绑定配置的热点地址（默认 `192.168.137.1:1883`），浏览器和 MQTT WebSocket 只绑定 `127.0.0.1`。Broker 限定设备与仪表板客户端、拒绝持久会话；命令主题不保留，Broker 的状态与 availability 保留在进程内存中。仪表板分开显示 Wi-Fi/IP、MQTT availability 和 F407 当前会话心跳，并提供受限命令及 ACK 状态。它不创建防火墙或 ICS 规则；无 MQTT 应用认证，只用于本人控制的可信热点。

F407 源码已包含 MQTT 3.1.1 有界报文/JSON处理、ESP AT TCP 收发、`+IPD` 接收分帧、QoS 1、遗嘱、状态补报、会话隔离、最近 8 条命令去重、GPIO 读回 ACK 和短蜂鸣限流。默认 CMake 选项启用 F407 MQTT 客户端，并关闭旧的自动 TCP 诊断；AT 清单诊断构建会关闭 MQTT 客户端源码，避免两种功能争用 ESP 命令通道。

**离线构建与测试证据：** 2026-10-30 使用 CMake 4.3.3、Ninja 1.13.0 和 Arm GNU Toolchain 15.2 Rel1，在新目录 `.ai/tmp/f407-mqtt-final-build-20261030/` 以 `RelWithDebInfo` 成功构建，`IOT_MQTT_ENABLE=ON`，Broker 默认地址为 `192.168.137.1:1883`。`TOUCH.hex` 为 1,489,093 字节，SHA-256 `DEFAFF834069CD8E12CAD895137BEBE19FBC8EE158CE05E3ADCC08EECC54E314`。映射文件显示 Flash `text + data` 为 529,368 / 1,048,576 字节（50.5%）；主 SRAM 已分配区间为 90,264 / 131,072 字节（68.9%，剩余 40,808 字节）；未分配 CCMRAM。该静态统计不代表运行时峰值或板上测量。

严格配置的主机离线套件 18/18 通过，包括 `mqtt_core`、`wifi_tcp_ipd_parser` 和 `wifi_takeover`。`app.js` 与 `server.mjs` 的 Node.js 语法检查通过。本机回环验证检查了 HTTP 页面与 MQTT.js 浏览器包、MQTT WebSocket 连接、三个 QoS 1 主题订阅、设备状态与保留快照、仪表板命令投递及设备 ACK 返回。该联动使用本机模拟设备，不等同于 F407 板端验收。

**仪表板订阅修复：** MQTT.js 订阅列表改为字符串主题数组，并通过独立选项指定 QoS 1。回环验证确认三个主题均获 QoS 1 授权。

**解析器修正：** 从 `FeedHeader` 私有辅助函数移除了未使用的回调参数，解决严格编译告警；解析器仍按 `+IPD` 声明长度提取 TCP 载荷，并将载荷后的 CRLF 透传到 AT 流。用例现在确认 `OK\r\n\r\n` 共 6 字节及尾随 CRLF。

**attempt 65 证据：** 使用 `.ai/tmp/mqtt-at-inventory-only-build-20261029/TOUCH.hex`（1,462,444 字节，SHA-256 `5B27F321ED2B3FA5B546A04483C11F8D0762DC573485CE56067EFD8FAA0689D0`）完成 PyOCD 直接 CLI 写入、两段读回校验和 SYSRESETREQ。复位前已打开 COM9 被动采集 45 秒；收到 `WIFI SYNC: START`、`WIFI SYNC: PROBE` 和 `WIFI MQTT AT INVENTORY: UNKNOWN REASON=QUERY_REJECTED_OR_UNSUPPORTED`。这表示清单查询结果未知，不表示 ESP 不支持 MQTT AT，也不验证 TCP 数据收发。对应记录为 `.ai/logs/f407-pyocd-direct-issue6-attempt65-20261029.jsonl`。屏幕与触摸在该次操作后未复核。

**attempt 69 板端验收记录：** `.ai/logs/issue6-attempt69-mqtt-hil-20261030.json` 记录最终镜像 SHA-256 `DEFAFF834069CD8E12CAD895137BEBE19FBC8EE158CE05E3ADCC08EECC54E314`、目标身份、两个分段读回匹配和 SYSRESETREQ。复位后 STA/IP 仍就绪；三次自动 TCP 建连均得到 `AT_ERROR` / `ERROR`，Broker 没有设备连接、availability 或心跳，MQTT 命令未执行。

**attempt 70 隔离诊断记录：** `.ai/logs/f407-issue6-attempt70-direct-pyocd-20261030.md` 记录诊断镜像 SHA-256 `D8AEFD9CCA3CC7BEB264D9AEAEBF869C9910FA873FF7C7DE2DAF70A02547798D` 已写入、两个段读回匹配并执行 SYSRESETREQ。复位后实时 Wi-Fi 状态为 STA=CONNECTED、IP=READY、TCP=DISCONNECTED；TCP 状态、MUX 和模式查询只得到 `QUERY_REQUESTED`，后续采集没有最终值。按授权门槛没有发起 TCP 连接，因此这次操作没有验证 Broker 连通性。显示、触摸和蓝牙未检查。当前板上运行的是隔离诊断镜像，不能据此验收最终 MQTT 固件。

**主机集成测试：** `host/f407-local-mqtt/test/server.test.mjs` 通过 `npm test` 在动态分配的回环端口启动 Broker/仪表板，检查页面与安全响应头、浏览器 MQTT WebSocket、设备 QoS 1 状态保留、非保留命令投递和 ACK 返回；2026-10-30 为 1/1 通过。测试使用模拟设备，不访问热点或 F407，不代替板端验收。

**后续同步诊断：** `.ai/logs/f407-mqtt-tcp1883-sync-20261030T112911Z.json` 记录安全串口再次确认 STA=CONNECTED、IP=READY、TCP=DISCONNECTED，并观察到固件自动连接失败。Broker 在 `192.168.137.1:1883` 监听。采集结果显示管理员权限、热点接口 ifIndex 8、过滤器已添加、采集已启动/停止且清理完成；ETL 位于 Windows PktMon 默认位置 `C:\Windows\System32\PktMon.etl`。ETL 时间窗为 19:23:14.165–19:24:44.435（本地时间），过滤器为 TCP、主机地址 `192.168.137.1`、端口 1883。转换为 Pcapng 后统计 0 个数据包、0 个丢包；采集窗口内未见匹配包。串口聚合记录没有逐条时间戳，无法确认任一 `AT_ERROR` 与窗口重合，因此这份抓包只证明该窗口没有匹配数据包，不能证明对应失败尝试没有发出 SYN，也不能定位到 ESP 或热点路径。该次没有单独发起裸 TCP 连接、写入固件或复位。此前 attempt 68 的“连接成功”其实是电脑主机连接自己的 Broker 监听端口，不是板端连接，已在 `.ai/logs/issue6-attempt68-broker-connect-20261030.json` 更正，不能作为板端 TCP 证据。

**最近一次同步核验（20:06）：** `.ai/logs/f407-mqtt-sync-once-20261030T120603452022Z.json` 记录 `WIFI LIVE STATUS` 被固件返回 `QUERY_REJECTED`；因此未取得实时 STA/TCP 状态，条件连接标记为 `NOT_SENT`，没有发起板端 TCP 连接。PktMon 因 `ACCESS_DENIED` 不可用，也没有开始抓包。`QUERY_REJECTED` 不能区分查询时状态机繁忙、AT 方言未知或接收缓冲仍有数据；固件正在自动重试时，繁忙是可能原因，但尚未证实。此结果既不证明连接成功，也不足以判断 MQTT 阻塞或热点转发故障。

**剩余验收边界：** TCP/MQTT 板端连接仍未解决，当前不应把第一版标记为实机完成。当前源码构造的单连接命令与 Espressif ESP8266 NON-OS AT 文档中的格式一致；单元测试也核对了生成的命令字符串。但目标报告的版本组合是 AT `1.7.4.0` 与 SDK `3.0.5-dev`；Espressif 发布记录将 AT `1.7.4.0` 列在 SDK `3.0.4`，SDK `3.0.5` 对应 AT `1.7.5`，所以目标镜像版本组合看起来是定制/开发构建，公开语法文档不能证明其运行时行为。ESP 返回通用 `ERROR`，但实际 UART TX 字节和更具体的模块错误未采集；PktMon 的 0 包窗口又没有与单次 UART 错误做时间戳对齐，目前根因仍未知。下一步应先解决同步采集的前置问题：避免在自动连接事务运行时请求实时状态，并取得可用的热点接口抓包权限；随后一次受控连接需同时保存完整 AT TX 字节、对应响应及抓包起止时间。若命令字节符合预期，再检查目标定制 AT 构建和热点路由。任何新烧录或串口/网络实机操作都需新的单次授权。TCP/MQTT 互通后还需验证 `+IPD` 接收、CONNACK/SUBACK、state/availability、命令 ACK、断网恢复及屏幕/触摸稳定性；主机模拟设备测试不能替代这些硬件验收。
## 参考资料

- Espressif, [ESP8266 AT Instruction Set: `AT+CIPSTART`](https://www.espressif.com/sites/default/files/4a-esp8266_at_instruction_set_en_v1.5.4_0.pdf) and [ESP8266 ESP-AT TCP/IP AT Commands](https://docs.espressif.com/projects/esp-at/en/release-v2.2.0.0_esp8266/AT_Command_Set/TCP-IP_AT_Commands.html)
- Microsoft, [PktMon start](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/pktmon-start), [PktMon filter add](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/pktmon-filter-add), and [PktMon ETL to Pcapng](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/pktmon-etl2pcap)
- Espressif, [ESP8266 ESP-AT v2.1 MQTT AT Commands](https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/MQTT_AT_Commands.html)
- Espressif, [ESP8266 ESP-AT v2.3 Basic AT Commands (`AT+CMD?` inventory)](https://docs.espressif.com/projects/esp-at/en/release-v2.3.0.0_esp8266/AT_Command_Set/Basic_AT_Commands.html)
- Espressif, [ESP8266 NONOS SDK release history](https://github.com/espressif/ESP8266_NONOS_SDK/releases) and [NONOS-AT / ESP-AT command comparison](https://docs.espressif.com/projects/esp-at/en/release-v2.2.0.0_esp32/AT_Command_Set/AT_Command_Set_Comparison.html)
- Aedes, [MQTT 3.1.1 broker and WebSocket support](https://github.com/moscajs/aedes/blob/main/README.md) and [WebSocket server example](https://github.com/moscajs/aedes/blob/main/docs/Examples.md)
- MQTT.js, [browser WebSocket transport and QoS behavior](https://github.com/mqttjs/MQTT.js)
- OASIS, [MQTT Version 3.1.1 specification](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html)
- STMicroelectronics, [RM0090 STM32F405/407 reference manual](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)
