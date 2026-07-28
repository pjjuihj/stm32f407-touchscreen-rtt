# 串口日志系统设计文档

## 项目概述

为 STM32F407ZG 触摸屏项目添加综合日志系统，通过串口指令触发，支持触摸屏事件、LCD显示和系统状态日志记录。

## 需求总结

1. **日志类型**：综合日志系统（触摸屏、LCD、系统状态）
2. **命令格式**：简单文本命令（如 `TOUCH ON`、`LCD OFF`、`TEST RUN`）
3. **日志格式**：详细格式日志（包含时间戳和模块名称）
4. **存储方式**：RAM 缓冲区存储，可通过指令导出
5. **串口配置**：USART1，波特率 115200

## 架构设计

### 系统架构图

```
┌─────────────────────────────────────────────────┐
│                  应用层                          │
├─────────────────────────────────────────────────┤
│  触摸屏模块  │  LCD模块  │  系统模块  │  测试模块  │
└─────────────────────────────────────────────────┘
                    │ 调用日志API
                    ▼
┌─────────────────────────────────────────────────┐
│              日志系统 (log.h/log.c)             │
│  • 日志级别控制 (ERROR/WARNING/INFO/DEBUG)     │
│  • 模块名称标记                                 │
│  • 时间戳生成                                   │
│  • RAM 缓冲区管理                               │
└─────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────┐
│              串口命令解析器                      │
│  • TOUCH ON/OFF                                │
│  • LCD ON/OFF                                  │
│  • SYSTEM ON/OFF                               │
│  • LEVEL <1-4>                                 │
│  • DUMP (导出缓冲区)                           │
│  • CLEAR (清空缓冲区)                          │
│  • TEST RUN (运行测试)                         │
└─────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────┐
│              USART1 串口输出                     │
└─────────────────────────────────────────────────┘
```

## 数据结构定义

### 日志级别

```c
typedef enum {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4
} LogLevel;
```

### 日志模块

```c
typedef enum {
    LOG_MODULE_TOUCH = 0,
    LOG_MODULE_LCD,
    LOG_MODULE_SYSTEM,
    LOG_MODULE_TEST,      // 测试模块
    LOG_MODULE_MAX
} LogModule;
```

### 日志条目

```c
typedef struct {
    uint32_t timestamp;      // 时间戳 (ms)
    LogLevel level;          // 日志级别
    LogModule module;        // 模块名称
    char message[64];        // 日志内容
} LogEntry;
```

### 日志配置

```c
typedef struct {
    bool enabled[LOG_MODULE_MAX];     // 各模块开关
    LogLevel level[LOG_MODULE_MAX];   // 各模块级别
    uint16_t buffer_size;             // 缓冲区大小
    uint16_t buffer_count;            // 当前条目数
    LogEntry *buffer;                 // 日志缓冲区
    bool test_mode;                   // 测试模式标志
} LogConfig;
```

## API 接口定义

### 日志系统 API

```c
// 初始化日志系统
void Log_Init(uint16_t buffer_size);

// 记录日志
void Log_Write(LogModule module, LogLevel level, const char *fmt, ...);

// 设置模块日志级别
void Log_SetLevel(LogModule module, LogLevel level);

// 启用/禁用模块日志
void Log_Enable(LogModule module, bool enable);

// 导出日志缓冲区
void Log_Dump(void);

// 清空日志缓冲区
void Log_Clear(void);

// 处理串口命令
void Log_ProcessCommand(const char *cmd);

// 启用测试模式
void Log_TestMode(bool enable);

// 运行测试用例
void Log_RunTests(void);

// 测试日志写入
void Log_TestWrite(void);

// 测试缓冲区溢出
void Log_TestBufferOverflow(void);

// 测试日志级别过滤
void Log_TestLevelFilter(void);
```

### 串口命令格式

```
命令格式: <MODULE> <ACTION> [参数]

示例:
  TOUCH ON          - 开启触摸屏日志
  TOUCH OFF         - 关闭触摸屏日志
  TOUCH LEVEL 3     - 设置触摸屏日志级别为 INFO
  LCD ON            - 开启 LCD 日志
  SYSTEM ON         - 开启系统日志
  LEVEL 4           - 设置所有模块日志级别为 DEBUG
  DUMP              - 导出日志缓冲区
  CLEAR             - 清空日志缓冲区
  TEST RUN          - 运行测试用例
  STATUS            - 显示当前日志配置
```

## RAM 缓冲区管理

### 缓冲区配置

```c
#define LOG_BUFFER_SIZE  64   // 默认缓冲区大小（条目数）
#define LOG_MESSAGE_LEN  64   // 每条日志最大长度
```

### 缓冲区结构

```c
typedef struct {
    LogEntry entries[LOG_BUFFER_SIZE];
    uint16_t head;           // 写入位置
    uint16_t tail;           // 读取位置
    uint16_t count;          // 当前条目数
    bool overflow;           // 溢出标志
} LogBuffer;
```

### 缓冲区操作

```c
void Buffer_Init(LogBuffer *buf);
bool Buffer_Write(LogBuffer *buf, LogEntry *entry);
bool Buffer_Read(LogBuffer *buf, LogEntry *entry);
void Buffer_Clear(LogBuffer *buf);
uint16_t Buffer_Count(LogBuffer *buf);
bool Buffer_IsFull(LogBuffer *buf);
```

### 缓冲区管理策略

1. **循环缓冲区**：使用 head 和 tail 指针实现 FIFO
2. **溢出处理**：当缓冲区满时，覆盖最旧的日志
3. **内存管理**：静态分配，避免动态内存碎片
4. **线程安全**：在中断上下文中使用时需要禁用中断

### 缓冲区状态监控

```c
typedef struct {
    uint16_t total_writes;    // 总写入次数
    uint16_t overflow_count;  // 溢出次数
    uint16_t current_usage;   // 当前使用率 (%)
} BufferStats;

BufferStats Buffer_GetStats(LogBuffer *buf);
```

## 时间戳生成

### 时间戳配置

```c
typedef enum {
    TIMESTAMP_SYSTICK,    // 使用 SysTick 计数器
    TIMESTAMP_TIMER,      // 使用硬件定时器
    TIMESTAMP_NONE        // 不使用时间戳
} TimestampSource;

typedef struct {
    TimestampSource source;
    uint32_t offset;
    uint32_t (*get_time)(void);
} TimestampConfig;
```

### 时间戳实现

- **SysTick 计数器**（推荐）：使用 HAL_GetTick()，精度 1ms
- **硬件定时器**：使用 TIM2/TIM5，精度可配置

### 时间戳格式

```
[000012345ms]  - 系统运行时间
```

## 模拟测试环境

### 测试结构

```c
typedef struct {
    const char *name;
    const char *description;
    bool (*test_func)(void);
    bool passed;
} TestCase;

typedef struct {
    uint8_t total;
    uint8_t passed;
    uint8_t failed;
    char failed_names[8][32];
} TestResult;

typedef struct {
    TestCase cases[16];
    uint8_t count;
    TestResult result;
} TestSuite;
```

### 测试用例列表

1. **基础功能测试**
   - Test_Log_Init：测试日志初始化
   - Test_Log_Write：测试日志写入
   - Test_Log_Enable：测试日志启用/禁用
   - Test_Log_SetLevel：测试日志级别设置

2. **缓冲区测试**
   - Test_Buffer_Write：测试缓冲区写入
   - Test_Buffer_Read：测试缓冲区读取
   - Test_Buffer_Overflow：测试缓冲区溢出
   - Test_Buffer_Clear：测试缓冲区清空

3. **命令解析测试**
   - Test_Parse_OnOff：测试 ON/OFF 命令
   - Test_Parse_Level：测试 LEVEL 命令
   - Test_Parse_Dump：测试 DUMP 命令
   - Test_Parse_Clear：测试 CLEAR 命令

4. **时间戳测试**
   - Test_Timestamp_Get：测试时间戳获取
   - Test_Timestamp_Format：测试时间戳格式化

5. **集成测试**
   - Test_FullCycle：测试完整流程

### 测试命令格式

```
命令格式: TEST <ACTION>

示例:
  TEST RUN          - 运行所有测试用例
  TEST WRITE        - 测试日志写入功能
  TEST OVERFLOW     - 测试缓冲区溢出处理
  TEST FILTER       - 测试日志级别过滤
  TEST CLEAR        - 清空测试日志
  TEST STATUS       - 显示测试状态
```

### 测试执行流程

```
串口命令: TEST RUN
    │
    ▼
┌─────────────────────────────────────────────┐
│           测试执行器                         │
│  1. 初始化测试环境                           │
│  2. 逐个执行测试用例                         │
│  3. 记录测试结果                             │
│  4. 输出测试报告                             │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│           测试报告                           │
│  [PASS] Test_Log_Init                       │
│  [PASS] Test_Log_Write                      │
│  [FAIL] Test_Buffer_Overflow - 缓冲区未满    │
│  [PASS] Test_Parse_OnOff                    │
│  ...                                        │
│  总计: 15  通过: 14  失败: 1                 │
└─────────────────────────────────────────────┘
```

## 日志格式示例

```
[TOUCH][000012345ms][INFO] Touch event: x=100, y=200
[LCD][000012346ms][DEBUG] Screen refresh: 30fps
[SYSTEM][000012347ms][WARNING] Memory usage: 75%
[TEST][000012348ms][INFO] Test case passed: Test_Log_Init
```

## 实现计划

### 第一阶段：基础框架
1. 创建日志系统模块 (`log.h`, `log.c`)
2. 实现日志级别控制
3. 实现 RAM 缓冲区管理
4. 实现时间戳生成

### 第二阶段：命令解析
1. 实现串口命令解析器
2. 添加 ON/OFF 命令支持
3. 添加 LEVEL 命令支持
4. 添加 DUMP/CLEAR 命令支持

### 第三阶段：模块集成
1. 在触摸屏模块中添加日志调用
2. 在 LCD 模块中添加日志调用
3. 在系统模块中添加日志调用

### 第四阶段：测试环境
1. 实现测试框架
2. 编写测试用例
3. 添加测试命令支持

### 第五阶段：验证和优化
1. 运行所有测试用例
2. 验证日志输出格式
3. 优化性能和内存使用

## 验收标准

1. **功能验收**
   - 日志系统能够记录触摸屏事件
   - 日志系统能够记录 LCD 显示信息
   - 日志系统能够记录系统状态
   - 串口命令能够控制日志开关和级别
   - 日志缓冲区能够正确存储和导出

2. **性能验收**
   - 日志写入不影响主循环性能
   - 缓冲区溢出时能够正确处理
   - 时间戳精度达到 1ms

3. **测试验收**
   - 所有测试用例通过
   - 测试报告输出正确
   - 测试环境稳定可靠

## 文件结构

```
├── USER/
│   ├── LOG/
│   │   ├── log.h              // 日志系统头文件
│   │   └── log.c              // 日志系统实现
│   └── TEST/
│       ├── test_log.h         // 测试模块头文件
│       └── test_log.c         // 测试模块实现
├── Main/
│   ├── main.c                 // 主程序（集成日志系统）
│   └── usart.c                // 串口配置实现
└── Common/
    └── usart.h                // 串口配置头文件
```
