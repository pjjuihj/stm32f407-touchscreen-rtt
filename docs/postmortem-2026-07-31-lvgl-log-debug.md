# 复盘：LVGL 日志启用调试

**日期**: 2026-07-31 | **耗时**: ~6小时 | **严重度**: 工程停滞

## 发生了什么

尝试为 STM32F407 LVGL 项目启用串口日志输出，导致工程反复崩溃，最终未能解决。

## 时间线

| 时间 | 事件 |
|------|------|
| 开始 | 目标：启用 LVGL 日志输出到串口 |
| +1h | 发现 SysTick 频率异常（~16MHz vs 168MHz） |
| +2h | 尝试 NOP 循环延时替代 SysTick |
| +3h | 发现栈溢出（CFSR STKOF=1） |
| +4h | 修改 scatter 文件增加栈大小，但 Keil 不生效 |
| +5h | 发现 Keil 生成的 scatter 文件缺少 Stack/Heap 段 |
| +6h | 工程损坏，git 回退 |

## 根因分析（5 Whys）

### Why #1: LVGL 日志为什么导致崩溃？

**答**: 栈溢出（CFSR STKOF=1）。LVGL 日志函数 `lv_log_add()` 在栈上分配 768 字节（512+256 字节缓冲区），超出默认栈大小。

### Why #2: 默认栈为什么这么小？

**答**: ARM Compiler 5 启动文件 `startup_stm32f407xx.s` 中 `Stack_Size = 0x400`（1024字节），但 Keil 有自己的栈分配策略，忽略启动文件的设置。

### Why #3: Keil 为什么忽略启动文件的栈大小？

**答**: Keil 项目启用了 "Use Memory Layout from Target Dialog"，自动生成 scatter 文件，但生成的 scatter 文件**不包含 Stack/Heap 段**。链接器使用默认小栈。

### Why #4: 为什么修改 scatter 文件不生效？

**答**: Keil 在每次构建时会重新生成 scatter 文件，覆盖手动修改。需要在 Linker 标签页取消勾选 "Use Memory Layout from Target Dialog"，或在 Misc controls 中手动指定 `--stack` 和 `--heap`。

### Why #5: 为什么修改 startup 文件也不生效？

**答**: Keil 可能缓存了编译后的启动文件对象（.o），或者使用了 pack 目录中的启动文件而非项目目录中的版本。

## 错误的修复尝试

| 尝试 | 结果 | 原因 |
|------|------|------|
| 增大 Stack_Size in startup .s | ❌ 不生效 | Keil 不使用该文件的 Stack_Size |
| 修改 scatter 文件 | ❌ 被覆盖 | "Use Memory Layout from Target Dialog" 勾选 |
| NOP 循环延时 | ❌ MCU 极慢 | SysTick 频率问题未解决 |
| DWT 周期计数器 | ❌ 不工作 | DWT 配置问题 |
| 增大 LV_MEM_SIZE | ❌ 无效 | 不是内存问题 |
| 调整初始化顺序 | ❌ 无效 | 不是时序问题 |

## 正确的修复方案

在 **Keil IDE** 中执行：

1. **Project → Options for Target → Linker**
2. **取消勾选** "Use Memory Layout from Target Dialog"
3. 在 **Misc controls** 中输入：`--stack 0x2000 --heap 0x1000`
4. **Build → Rebuild All**

## 经验教训

1. **不要盲目修改 scatter 文件** — Keil 会覆盖它
2. **不要修改 startup 文件中的 Stack_Size** — Keil 可能忽略它
3. **先确认编译输出** — 检查 hex 文件大小和初始 SP 值
4. **用 pyodc 读取初始 SP** — 验证栈配置是否生效
5. **栈溢出症状** — CFSR STKOF=1, MCU 卡在 delay_us
6. **LVGL 日志栈消耗** — `lv_log_add()` 一次调用消耗 768 字节栈

## 未解决的问题

1. **SysTick 频率异常** — 配置为168MHz 但实际 ~16MHz
2. **Keil 栈配置** — 需要在 IDE 中手动设置
3. **LVGL 日志启用** — 需要足够的栈空间

## 下次执行步骤

1. 在 Keil IDE 中设置 stack=8KB, heap=4KB
2. Rebuild All
3. 烧录测试
4. 如果正常，启用 LV_LOG_LEVEL_ERROR
5. 逐步提高日志级别
