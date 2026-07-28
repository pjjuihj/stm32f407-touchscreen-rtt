# Keil MDK 工程配置指南 (添加LVGL支持)

## 目录

1. [当前工程配置](#1-当前工程配置)
2. [需要修改的内容](#2-需要修改的内容)
3. [详细配置步骤](#3-详细配置步骤)
4. [添加LVGL源文件](#4-添加lvgl源文件)
5. [添加GUI文件](#5-添加gui文件)
6. [编译选项配置](#6-编译选项配置)
7. [内存配置](#7-内存配置)
8. [常见编译错误](#8-常见编译错误)

---

## 1. 当前工程配置

### 1.1 工程文件位置

```
Project/TOUCH.uvprojx
```

### 1.2 当前配置

| 配置项 | 当前值 |
|--------|--------|
| **MCU** | STM32F407ZG |
| **编译器** | ARMCC V5.06 |
| **预处理宏** | `USE_HAL_DRIVER,STM32F407xx` |
| **头文件路径** | `..\Common;..\Main;..\Startup_config;..\STM32F4xx_HAL_Driver\inc;..\USER\LED;..\USER\LCD;..\USER\BEEP;..\USER\KEY;..\USER\LCD;..\USER\TOUCH` |

### 1.3 当前工程分组

```
TOUCH (Target 1)
├── User          # 用户代码 (main.c等)
├── StdPeriph     # HAL库
├── Startup       # 启动文件
└── ...
```

---

## 2. 需要修改的内容

### 2.1 需要修改的配置

| 配置项 | 修改内容 |
|--------|----------|
| **预处理宏** | 添加 `LV_CONF_INCLUDE_SIMPLE` |
| **头文件路径** | 添加 LVGL 和 GUI 路径 |
| **源文件分组** | 添加 LVGL 和 GUI 分组 |

### 2.2 需要添加的文件

| 文件/目录 | 说明 |
|-----------|------|
| `LVGL/src/*.c` | LVGL核心源码 |
| `LVGL/src/drivers/display/ili9341/*.c` | ILI9341驱动 |
| `LVGL/src/drivers/display/lcd/*.c` | 通用LCD驱动 |
| `GUI/gui_driver.c` | 驱动适配层 |

---

## 3. 详细配置步骤

### 步骤1: 修改预处理宏

**操作路径:**
```
Project → Options for Target → C/C++ → Preprocessor Symbols → Define
```

**当前值:**
```
USE_HAL_DRIVER,STM32F407xx
```

**修改为:**
```
USE_HAL_DRIVER,STM32F407xx,LV_CONF_INCLUDE_SIMPLE
```

**说明:**
- `LV_CONF_INCLUDE_SIMPLE`: 告诉LVGL使用简化的头文件包含方式

---

### 步骤2: 添加头文件路径

**操作路径:**
```
Project → Options for Target → C/C++ → Include Paths
```

**当前值:**
```
..\Common
..\Main
..\Startup_config
..\STM32F4xx_HAL_Driver\inc
..\USER\LED
..\USER\LCD
..\USER\BEEP
..\USER\KEY
..\USER\TOUCH
```

**添加以下路径:**
```
..\LVGL
..\LVGL\src
..\LVGL\src\drivers\display\ili9341
..\LVGL\src\drivers\display\lcd
..\GUI
```

**完整路径列表:**
```
..\Common
..\Main
..\Startup_config
..\STM32F4xx_HAL_Driver\inc
..\USER\LED
..\USER\LCD
..\USER\BEEP
..\USER\KEY
..\USER\TOUCH
..\LVGL
..\LVGL\src
..\LVGL\src\drivers\display\ili9341
..\LVGL\src\drivers\display\lcd
..\GUI
```

---

## 4. 添加LVGL源文件

### 步骤3: 创建LVGL分组

**操作路径:**
```
Project → Manage Project Items
```

**操作步骤:**
1. 在 "Groups" 列表中，点击 "New (Insert)" 按钮
2. 输入分组名称: `LVGL`
3. 点击 "OK"

---

### 步骤4: 添加LVGL核心源文件

**操作路径:**
```
Project → Manage Project Items → LVGL → Add Files
```

**添加以下文件:**

```
LVGL/src/lv_init.c
LVGL/src/core/*.c (所有.c文件)
LVGL/src/display/*.c
LVGL/src/draw/*.c
LVGL/src/font/*.c
LVGL/src/indev/*.c
LVGL/src/layouts/*.c
LVGL/src/libs/*.c
LVGL/src/misc/*.c
LVGL/src/osal/*.c
LVGL/src/others/*.c
LVGL/src/stdlib/*.c
LVGL/src/themes/*.c
LVGL/src/tick/*.c
LVGL/src/widgets/*.c
```

**快捷方式:**
- 使用文件过滤器 `*.c` 选择所有C文件
- 或者批量添加整个 `LVGL/src` 目录

---

### 步骤5: 添加ILI9341驱动文件

**操作路径:**
```
Project → Manage Project Items → LVGL → Add Files
```

**添加以下文件:**
```
LVGL/src/drivers/display/ili9341/lv_ili9341.c
LVGL/src/drivers/display/lcd/lv_lcd_generic_mipi.c
```

---

## 5. 添加GUI文件

### 步骤6: 创建GUI分组

**操作路径:**
```
Project → Manage Project Items
```

**操作步骤:**
1. 在 "Groups" 列表中，点击 "New (Insert)" 按钮
2. 输入分组名称: `GUI`
3. 点击 "OK"

---

### 步骤7: 添加GUI源文件

**操作路径:**
```
Project → Manage Project Items → GUI → Add Files
```

**添加以下文件:**
```
GUI/gui_driver.c
```

---

## 6. 编译选项配置

### 步骤8: 设置优化级别

**操作路径:**
```
Project → Options for Target → C/C++ → Optimization
```

**推荐设置:**
- **Level**: -O1 (Level 1)
- **Optimize for**: Time (速度优先)

**说明:**
- LVGL建议使用 -O1 优化
- -O2 可能导致编译警告
- 不要使用 -O0 (无优化，代码太大)

---

### 步骤9: 设置C99模式

**操作路径:**
```
Project → Options for Target → C/C++ → Language Conversions
```

**推荐设置:**
- **C99 Mode**: 勾选 ✓

**说明:**
- LVGL v9.x 使用C99语法
- 必须启用C99模式

---

## 7. 内存配置

### 步骤10: 配置内存布局

**操作路径:**
```
Project → Options for Target → Target
```

**STM32F407ZG内存配置:**

| 区域 | 起始地址 | 大小 | 说明 |
|------|----------|------|------|
| **IROM1** | 0x08000000 | 0x100000 (1MB) | Flash |
| **IRAM1** | 0x20000000 | 0x20000 (128KB) | SRAM1 |
| **IRAM2** | 0x10000000 | 0x10000 (64KB) | CCM SRAM |

**配置步骤:**
1. 在 "IROM1" 栏输入: `0x08000000`, Size: `0x100000`
2. 在 "IRAM1" 栏输入: `0x20000000`, Size: `0x20000`
3. 勾选 "Use Memory Layout from Target Dialog"

---

## 8. 常见编译错误

### 错误1: "lv_conf.h: No such file or directory"

**原因:** 头文件路径未正确配置

**解决:**
1. 确保 `LVGL/lv_conf.h` 文件存在
2. 在 Include Paths 中添加 `..\LVGL`
3. 确保预处理宏包含 `LV_CONF_INCLUDE_SIMPLE`

---

### 错误2: "lv_ili9341.h: No such file or directory"

**原因:** ILI9341驱动头文件路径未添加

**解决:**
1. 在 Include Paths 中添加 `..\LVGL\src\drivers\display\ili9341`
2. 或者使用相对路径包含

---

### 错误3: "undefined reference to lv_ili9341_create"

**原因:** ILI9341驱动源文件未添加到工程

**解决:**
1. 在 Manage Project Items 中添加 `LVGL/src/drivers/display/ili9341/lv_ili9341.c`
2. 确保 `LV_USE_ILI9341` 定义为1

---

### 错误4: "undefined reference to lv_lcd_generic_mipi_create"

**原因:** 通用LCD驱动源文件未添加到工程

**解决:**
1. 在 Manage Project Items 中添加 `LVGL/src/drivers/display/lcd/lv_lcd_generic_mipi.c`
2. 确保 `LV_USE_GENERIC_MIPI` 定义为1

---

### 错误5: "error: #5: cannot open source input file "lv_conf.h""

**原因:** 预处理宏配置错误

**解决:**
1. 确保预处理宏包含 `LV_CONF_INCLUDE_SIMPLE`
2. 或者将 `lv_conf.h` 放在与 `lvgl.h` 同级目录

---

### 错误6: "error: C99 mode not enabled"

**原因:** 编译器未启用C99模式

**解决:**
1. 在 Language Conversions 中启用 C99 Mode
2. 或者添加编译选项 `--c99`

---

### 错误7: "error: cannot open source input file "src/core/lv_obj.h""

**原因:** LVGL源文件路径问题

**解决:**
1. 确保 `LVGL/src` 路径正确
2. 检查文件是否真的存在

---

## 9. 完整配置清单

### 9.1 预处理宏

```
USE_HAL_DRIVER,STM32F407xx,LV_CONF_INCLUDE_SIMPLE
```

### 9.2 头文件路径

```
..\Common
..\Main
..\Startup_config
..\STM32F4xx_HAL_Driver\inc
..\USER\LED
..\USER\LCD
..\USER\BEEP
..\USER\KEY
..\USER\TOUCH
..\LVGL
..\LVGL\src
..\LVGL\src\drivers\display\ili9341
..\LVGL\src\drivers\display\lcd
..\GUI
```

### 9.3 工程分组

```
TOUCH (Target 1)
├── User          # 用户代码 (main.c等)
├── StdPeriph     # HAL库
├── Startup       # 启动文件
├── LVGL          # 新增: LVGL库
└── GUI           # 新增: 驱动适配层
```

### 9.4 LVGL分组文件

```
LVGL
├── LVGL/src/lv_init.c
├── LVGL/src/core/*.c
├── LVGL/src/display/*.c
├── LVGL/src/draw/*.c
├── LVGL/src/font/*.c
├── LVGL/src/indev/*.c
├── LVGL/src/layouts/*.c
├── LVGL/src/libs/*.c
├── LVGL/src/misc/*.c
├── LVGL/src/osal/*.c
├── LVGL/src/others/*.c
├── LVGL/src/stdlib/*.c
├── LVGL/src/themes/*.c
├── LVGL/src/tick/*.c
├── LVGL/src/widgets/*.c
├── LVGL/src/drivers/display/ili9341/lv_ili9341.c
└── LVGL/src/drivers/display/lcd/lv_lcd_generic_mipi.c
```

### 9.5 GUI分组文件

```
GUI
└── GUI/gui_driver.c
```

### 9.6 编译选项

| 选项 | 设置 |
|------|------|
| **优化级别** | -O1 (Level 1) |
| **C99模式** | 启用 |
| **警告级别** | 默认 |

---

## 10. 配置验证

### 10.1 编译测试

1. 点击 "Build" 按钮 (或按 F7)
2. 检查编译输出
3. 确保没有错误

### 10.2 预期输出

```
*** Target 'Target 1' of Project 'TOUCH' - Date: ...
...
compiling lv_init.c...
compiling lv_obj.c...
compiling lv_ili9341.c...
...
linking...
Program Size: Code=xxx RO-data=xxx RW-data=xxx ZI-data=xxx
FromELF: creating hex file...
".\OBJ\TOUCH.axf" - 0 Error(s), 0 Warning(s).
Build Time Elapsed: ...
```

### 10.3 内存使用检查

```
Program Size:
  Code:      约 50-100KB (LVGL核心)
  RO-data:   约 200-300KB (字体等)
  RW-data:   约 10-20KB (全局变量)
  ZI-data:   约 50-100KB (堆栈+缓冲区)

Flash使用: 约 300-400KB / 1MB (足够)
RAM使用:   约 60-120KB / 192KB (足够)
```

---

## 附录

### A. 文件创建检查清单

在编译前，确保以下文件已创建:

- [ ] `LVGL/lv_conf.h` (从模板修改)
- [ ] `GUI/gui_driver.h`
- [ ] `GUI/gui_driver.c`
- [ ] `Main/main.c` (已修改)

### B. 配置截图参考

**预处理宏配置:**
```
Project → Options for Target → C/C++ → Preprocessor Symbols → Define
输入: USE_HAL_DRIVER,STM32F407xx,LV_CONF_INCLUDE_SIMPLE
```

**头文件路径配置:**
```
Project → Options for Target → C/C++ → Include Paths
点击 "..." 按钮，添加新路径
```

**源文件添加:**
```
Project → Manage Project Items
选择分组 → 点击 "Add Files" → 选择文件
```

---

**文档版本:** v1.0
**创建日期:** 2026-07-28
**适用软件:** Keil MDK-ARM V5.x
**适用芯片:** STM32F407ZG
