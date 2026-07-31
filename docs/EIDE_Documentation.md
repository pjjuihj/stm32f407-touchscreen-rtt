# EIDE (Embedded IDE For VSCode) 完整文档

> 来源：https://em-ide.com/docs/intro
> 抓取时间：2026-07-31

---

## 目录

- [简介](#1-简介)
- [平台支持](#2-平台支持)
- [开始上手](#3-开始上手)
  - [安装](#31-安装)
  - [项目结构](#32-项目结构)
  - [新建项目](#33-新建项目)
  - [导入项目](#34-导入项目)
- [功能模块](#4-功能模块)
  - [项目资源](#41-项目资源)
  - [芯片支持包](#42-芯片支持包)
  - [构建器配置](#43-构建器配置)
  - [烧录器配置](#44-烧录器配置)
  - [项目属性](#45-项目属性)
  - [项目设置](#46-项目设置)
- [进阶功能](#5-进阶功能)
  - [调试项目](#51-调试项目)
  - [插件设置](#52-插件设置)
  - [项目 Target](#53-项目-target)
  - [多项目工作区](#54-多项目工作区)
  - [文件路径变量](#55-文件路径变量)
  - [导出项目](#56-导出项目)
  - [CMSIS 头文件配置向导](#57-cmsis-头文件配置向导)
  - [实用功能](#58-实用功能)
  - [串口监视器](#59-串口监视器)
  - [安装实用工具](#510-安装实用工具)
  - [实用终端](#511-实用终端)
- [注意事项](#6-注意事项)
  - [版本兼容性](#61-版本兼容性)
  - [导入限制](#62-导入限制)
  - [C/C++ 配置](#63-cc-配置)
- [相关链接](#7-相关链接)

---

## 1. 简介

**EIDE** 是一个 VSCode 插件，用来开发单片机项目，支持：

- `8051`
- `STM8`
- `STM32`
- `Cortex-M` 系列 MCU
- `RISC-V`
- 其他 GCC 兼容芯片

### 为什么选择 EIDE？

| 优势 | 说明 |
|------|------|
| **更好的编码体验** | 借助 VSCode，提高效率，减少程序潜在错误 |
| **统一的开发环境** | 支持多种编译器、烧录工具，适用于绝大多数单片机开发 |
| **跨平台开发** | 支持 Windows x64 和 Linux x64 |
| **更好的社区生态** | VSCode 有大量插件可用 |
| **更好的版本管理** | VSCode 内置 GIT 功能 |

### 与 Keil/IAR 的区别

- 功能上基本一致，但基于 VSCode 插件
- **⚠️ 不具备调试功能**（需借助其他插件）

---

## 2. 平台支持

| 平台 | 状态 |
|------|------|
| `Windows 10+ x64` | ✅ 支持 |
| `Linux X64` | ✅ 支持 |
| `macOS` | 仅 macOS 10.15 x64 测试过 |

---

## 3. 开始上手

### 3.1 安装

#### 先决条件

- 熟悉 VSCode
- 熟悉 C/C++ 项目的基本构建过程

#### 安装步骤

1. 打开 VSCode 插件市场，搜索 `eide`
2. 选择 **Embedded IDE** 并安装
3. 插件自动下载 `eide-binaries` 和 **.NET6 X64 运行时**
4. 安装完成后**重启 VSCode**
5. 打开输出面板查看 `eide-log`

#### 离线安装

前往 [Github Release](https://github.com/github0null/eide/releases) 下载离线安装包

#### Linux 平台安装 .NET 运行时

```bash
# 1. 下载并执行 dotnet-install.sh 脚本
curl -sSL https://dot.net/v1/dotnet-install.sh | bash

# 2. 创建软链接
ln -s ~/.dotnet/dotnet ~/.local/bin/dotnet

# 3. 登出并重新登入，重启 VSCode
```

#### 未找到运行时

```bash
# 关闭所有 VSCode 实例，然后在终端执行
code .
```

#### 运行时版本要求

必须安装**主版本号为 6** 的版本（如：v6.0.5）

下载地址：[dotnet 下载](https://dotnet.microsoft.com/download/dotnet/6.0)

### 功能区域

| 区域 | 说明 |
|------|------|
| **EIDE 项目** | 项目视图，显示当前所有打开的项目 |
| **OPERATIONS** | 操作视图，显示所有可用的快捷命令 |

### 配置工具链

#### 手动安装

1. 在操作视图中点击 **设置工具链**
2. 选择要使用的编译器
3. 设置其安装路径（**根目录**）

#### 工具链安装目录

**工具链安装目录**是指编译器的**根目录**。

示例：
```
GCC 编译器位置：d:/software/my_toolchain/arm_gcc_10/bin/arm-none-eabi-gcc.exe
工具链安装目录：d:/software/my_toolchain/arm_gcc_10
```

#### 让 EIDE 自动安装编译器

Windows 平台可通过 **安装实用工具** 功能自动安装

#### 自动探测可用的工具链

将对应的插件设置值**设为空**，插件会从环境变量中自动探测。

| 工具链 | 插件设置 | 探测命令 |
|--------|----------|----------|
| IAR Arm Compiler | `EIDE.IAR.ARM.Toolchain.InstallDirectory` | - |
| GNU Toolchain For Arm | `EIDE.ARM.GCC.InstallDirectory` | `${EIDE.ARM.GCC.Prefix}gcc` |
| LLVM For Arm | `EIDE.ARM.LLVM.InstallDirectory` | `clang` |
| Arm Compiler V5 | `EIDE.ARM.ARMCC5.InstallDirectory` | `armcc` |
| Arm Compiler V6 | `EIDE.ARM.ARMCC6.InstallDirectory` | `armclang` |
| Small Device C Compiler | `EIDE.SDCC.InstallDirectory` | `sdcc` |
| GNU Toolchain For RISCV | `EIDE.RISCV.InstallDirectory` | `${EIDE.RISCV.ToolPrefix}gcc` |
| GNU Toolchain For MIPS | `EIDE.MIPS.InstallDirectory` | - |

---

### 3.2 项目结构

#### 文件夹结构

```
项目根目录/
├── .eide/                    # EIDE 配置文件夹
│   └── eide.json             # 项目描述文件（勿手动修改！）
├── <workspace>.code-workspace  # VSCode 工作空间文件
└── 源代码文件
```

**必须存在的文件**：
- `.eide/` 文件夹
- `.eide/eide.json`
- `<workspace>.code-workspace`

#### .gitignore 模板

```gitignore
# dot files
.vscode/
launch.json
.eide/
log/
.eide.usr.ctx.json
.settings

# project out
build/
out/
bin/
obj/

# eide template
*.ept
*.eide-template
```

#### 建议的行为

1. **源文件放在工作区内**：C++ IntelliSense、Code Completion 等功能需要在工作区搜索文件
2. **路径只包含 ASCII 字符**：避免乱码和工具兼容性问题

---

### 3.3 新建项目

#### 支持的创建方式

| 类型 | 说明 |
|------|------|
| **Empty Project** | 空项目，从零配置 |
| **Internal Template** | 使用内置模板 |
| **Local Template** | 本地 `.ept` 模板文件 |
| **From Remote Repository** | 从 Git 仓库下载模板 |

#### 从项目模板创建

1. 访问 https://templates.em-ide.com/ 下载模板（`.ept` 文件）
2. 打开 VSCode → 新建项目 → 本地项目模板
3. 选择下载的 `.ept` 文件

#### 从空项目创建

1. 新建项目 → 空项目
2. 选择芯片类型
3. 手动添加源文件、头文件、配置包含路径、宏定义、编译参数

---

### 3.4 导入项目

#### 支持的导入类型

- Keil 项目（`.uvproj` / `.uvprojx`）
- Eclipse 项目

#### 导入流程

1. 操作视图 → 导入项目
2. 选择项目类型
3. 选择 `.uvproj` 或 `.uvprojx` 文件
4. 点击 Import 按钮
5. 选择是否与原 Keil 项目共存

**注意**：导入后 EIDE 项目与原项目**失去关联**

---

## 4. 功能模块

### 4.1 项目资源

管理项目中的源文件、源文件夹、输出文件。

#### 支持的文件类型

| 类型 | 扩展名 |
|------|--------|
| C Files | `.c` |
| C++ Files | `.cpp`, `.cxx`, `.cc`, `.c++` |
| ASM Files | `.s`, `.asm`, `.a51` |
| Obj Files | `.obj`, `.o` |
| Lib Files | `.lib`, `.a` |

#### 操作命令

- 添加/移除源文件
- 添加/移除源文件夹
- 排除/包含源文件
- 修改源文件路径

#### 文件夹类型

| 类型 | 说明 |
|------|------|
| **Virtual Folder** | 虚拟文件夹，磁盘上不存在 |
| **Normal Folder** | 实际文件夹 |

---

### 4.2 芯片支持包

仅支持 **Cortex-M** 项目类型。

#### 什么是 CMSIS-Pack

CMSIS-Pack 定义了标准化的软件组件交付方式，包括：
- 设备参数
- 板支持信息
- 代码

#### 安装 CMSIS-Pack 的优势

| 功能 | 优化内容 |
|------|----------|
| **编译配置** | 自动根据芯片类型选择 CPU Type |
| **烧录配置** | JLink 自动完成型号选择和烧录算法文件安装 |
| **调试配置** | 自动生成调试配置（含芯片型号、SVD 路径等） |
| **RAM/ROM Layout** | 自动填写 RAM/ROM 地址和大小 |

---

### 4.3 构建器配置

为项目配置编译器选项。

#### 编译器配置

- 为项目选择合适的编译工具
- 可单独配置 toolchain path 和 toolchain prefix
- 配置仅在当前工作区生效

#### 基本配置项

| 配置项 | 说明 |
|--------|------|
| **CPU 类型** | CPU 系列名称（如 Cortex-M3, Cortex-M4） |
| **硬件浮点选项** | 浮点类型（sp, dp, none） |
| **链接脚本文件路径** | 存储器布局描述文件（`.sct`, `.lds`, `.ld`） |

---

### 4.4 烧录器配置

#### 支持的烧录器

| 项目类型 | 烧录器 |
|----------|--------|
| 8BIT Project | stvp (STM8), stcgal, any shell command |
| Arm Project | jlink, stlink, pyocd, openocd, any shell command |
| RISCV Project | jlink, pyocd, openocd, any shell command |

#### 安装烧录工具

烧录工具未集成到 EIDE，需单独安装。

#### 烧录器设置

| Flasher | VSCode Settings ID |
|---------|---------------------|
| STVP | `EIDE.STM8.STVP.CliExePath` |
| JLink | `EIDE.JLink.InstallDirectory` |
| STLink | `EIDE.STLink.ExePath` |
| Openocd | `EIDE.OpenOCD.ExePath` |

**提示**：可将 flasher 可执行文件路径设置到系统环境变量

---

### 4.5 项目属性

#### 可用的属性

| 属性 | 说明 |
|------|------|
| **包含目录** | C 头文件（`.h`）的搜索路径 |
| **库目录** | `-l` 选项的库文件搜索路径（仅 GCC） |
| **预处理器定义** | 全局宏定义（仅 C/C++ 源文件） |

#### 汇编器宏定义格式

| 汇编器类型 | 格式 |
|------------|------|
| ARMCC 5/6 | `"<key> SETA <value>"` |
| ARMCC 6 (asm-clang) | `<key>=<value>` |
| ARM GCC | `-D<key>=<value>` |

---

### 4.6 项目设置

#### 可用的设置项

| 设置项 | 说明 |
|--------|------|
| **项目名** | 输出文件名，`${ProjectName}` 变量的值 |
| **输出文件夹名** | 默认 `build` |

#### 环境变量

支持在项目中添加环境变量，可用于：
- 源文件路径
- 编译器参数
- 构建流程（进程的环境变量）
- 烧录器命令

#### 特殊的 EIDE 变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `COMPILER_CMD_PREFIX` | string | 向编译器传递固定参数（如许可证参数） |
| `MCU_RAM_SIZE` | number/hex | 打印内存使用情况（需配合 MCU_ROM_SIZE） |
| `MCU_ROM_SIZE` | number/hex | 打印内存使用情况（需配合 MCU_RAM_SIZE） |
| `EIDE_BUILD_ORDER` | number | 多项目工作区中的项目构建优先级 |

---

## 5. 进阶功能

### 5.1 调试项目

#### 准备工作

1. 安装 **Cortex-Debug** 插件（无需额外配置）
2. 确保工程可正常烧录
3. 安装 `arm-none-eabi-gcc` 工具链

#### 调试器选择

| 调试器 | 所需工具 |
|--------|----------|
| **STLink** | STM32 Cube Programmer CLI + STLink GDB Server |
| **JLink** | JLink GDB Server |

---

### 5.2 插件设置

#### 修改插件设置

VSCode 设置页面 → EIDE 设置

#### 3 个作用域

| 作用域 | 说明 |
|--------|------|
| **User** | 当前用户的全局设置 |
| **Workspace** | 当前 VSCode 工作区设置 |
| **Folder** | 当前打开的文件夹设置 |

---

### 5.3 项目 Target

- 一个项目可添加多个 Target
- 每个 Target 有独立的：
  - 构建器选项
  - 项目属性
  - 烧录器属性

**使用场景**：同一份源码需要多个编译配置

---

### 5.4 多项目工作区

创建包含多个相互关联项目的 VSCode 工作区。

#### 配置示例

```json
{
  "folders": [
    {"name": "project1", "path": "project1"},
    {"name": "project2", "path": "project2"}
  ]
}
```

---

### 5.5 文件路径变量

| 变量 | 说明 |
|------|------|
| `${workspaceFolder}` | 工作区文件夹完整路径 |
| `${workspaceFolderBasename}` | 工作区文件夹名称 |
| `${OutDir}` | 输出目录完整路径 |
| `${OutDirRoot}` | 输出目录根名称 |
| `${OutDirBase}` | 输出目录基础名 |
| `${ProjectName}` | EIDE 项目名称 |
| `${ConfigName}` | Target 名称（如 debug, release） |
| `${ExecutableName}` | 输出文件完整路径（不含后缀） |
| `${ProjectRoot}` | 项目根目录完整路径 |

---

### 5.6 导出项目

#### 导出为模板

点击 **导出为 ... → EIDE 项目模板**，eide 将使用 7z 压缩工作区并保存为 `.ept` 文件

#### 忽略文件

压缩时自动忽略：
- `.git`
- `<projectOutputFolder>`
- `*.eide-template`
- `*.log`

可创建 `.eideignore` 文件自定义忽略列表

#### 自动运行的钩子

| 脚本 | 阶段 |
|------|------|
| `pre-install.sh` | 加载项目前 |
| `post-install.sh` | 项目完成加载后 |

---

### 5.7 CMSIS 头文件配置向导

- 支持 CMSIS Configuration Annotations 格式
- 自动生成 GUI 配置向导修改头文件配置

---

### 5.8 实用功能

| 功能 | 说明 |
|------|------|
| **反汇编代码** | 查看 C 源文件的反汇编（GCC/ARMCC） |
| **ELF 反汇编** | 查看整个程序的反汇编 |
| **ELF 查看器** | 检查构建输出的 ELF/AXF 文件 |

---

### 5.9 串口监视器

- 自 v3.11.0 起不再内置
- 推荐使用 `ms-vscode.vscode-serial-monitor`

#### 串口设置

| 设置项 | 说明 |
|--------|------|
| `EIDE.SerialPortMonitor.DefaultPort` | 默认端口 |
| `EIDE.SerialPortMonitor.BaudRate` | 波特率 |
| `EIDE.SerialPortMonitor.DataBits` | 数据位 |
| `EIDE.SerialPortMonitor.Parity` | 校验位 |
| `EIDE.SerialPortMonitor.StopBits` | 停止位 |

---

### 5.10 安装实用工具

工具安装到 `<user_home>/.eide/tools`，并自动导出 bin 路径到系统环境变量。

#### 查看工具安装位置

**Windows (PowerShell)**：
```powershell
ls env:
```

**Windows (CMD)**：
```cmd
set
```

**Linux**：
```bash
env
```

---

### 5.11 实用终端

#### EIDE 终端

导出了变量和程序路径，可直接执行内置程序。

#### Msys 终端

Windows 上的 Linux Shell 环境，支持：
- `awk`, `bash`, `cat`, `curl`, `date`, `grep`, `ls`, `make`, `sed`, `vim` 等

---

## 6. 注意事项

### 6.1 版本兼容性

- 版本迭代**向后兼容**
- v3.8.3 移除了源文件夹自动搜索功能
  - 临时兼容设置：
    - `EIDE.SourceTree.AutoSearchIncludePath`
    - `EIDE.SourceTree.AutoSearchObjFile`

---

### 6.2 导入限制

#### Keil 项目限制

| 限制 | 说明 |
|------|------|
| **版本限制** | 只支持 v5 |
| **RTE 组件** | 不支持导入（CMSIS 组件除外） |

**强制导入**：需手动添加缺失的源文件和包含目录

---

### 6.3 C/C++ 配置

- **不要手动**创建/编写 `c_cpp_properties.json`
- EIDE 使用 `CustomConfigurationProvider` API 自动配置
- 如存在该文件，C/C++ 插件会优先使用，可能导致补全/感知异常

---

## 7. 相关链接

| 资源 | 地址 |
|------|------|
| VSCode 插件 | [CL.eide](https://marketplace.visualstudio.com/items?itemName=CL.eide) |
| GitHub | [github0null/eide](https://github.com/github0null/eide) |
| 论坛 | [discuss.em-ide.com](https://discuss.em-ide.com) |
| 项目模板 | [templates.em-ide.com](https://templates.em-ide.com/) |
| 更新日志 | [VSCode Marketplace](https://marketplace.visualstudio.com/items/CL.eide/changelog) |
| SourceForge | [em-ide.sourceforge.io](https://em-ide.sourceforge.io/) |
| GCC 文档 | [gcc.gnu.org](https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gcc/) |
