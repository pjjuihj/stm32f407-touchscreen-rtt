# 自动化闭环调试工作流设计文档

## 概述

设计一个**全自动闭环调试工作流**，用于 STM32 嵌入式开发。系统实现**零人工干预**的自动化流程：检测问题 → 分析根因 → 修复代码 → 烧录验证 → 复位测试 → 生成文档。

**核心理念：全自动化，无需人工干预**

### 自动化闭环流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                        全自动闭环工作流                               │
│                                                                     │
│  文件变化/编译错误/串口异常                                           │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动检测问题    │ ← 无需人工触发                                 │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动分析根因    │ ← 自动匹配历史模式                             │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动修复代码    │ ← 自动修改源文件                               │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动编译验证    │ ← 自动调用编译器                               │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动烧录固件    │ ← 自动检测调试器并烧录                         │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动复位芯片    │ ← 通过 SWD/DTR 自动复位                        │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动功能验证    │ ← 通过串口自动测试                             │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动记录文档    │ ← 自动生成结构化文档                           │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────┐                                                │
│  │ 自动版本控制    │ ← 自动创建 Git commit 和标签                    │
│  └─────────────────┘                                                │
│                                                                     │
│  全程无需人工干预！                                                  │
└─────────────────────────────────────────────────────────────────────┘
```

## 核心需求

| 需求 | 说明 | 自动化程度 |
|------|------|-----------|
| **自动触发** | 检测到文件变化、编译错误、串口异常时自动启动 | 100% 自动 |
| **自动检测** | 自动检测编译错误、运行时异常、硬件故障 | 100% 自动 |
| **自动分析** | 自动分析根因，匹配历史错误模式 | 100% 自动 |
| **自动修复** | 自动修改源代码修复问题 | 100% 自动 |
| **自动编译** | 自动调用编译器验证修复 | 100% 自动 |
| **自动烧录** | 自动检测调试器并烧录固件 | 100% 自动 |
| **自动复位** | 烧录后通过 SWD/DTR 信号自动复位芯片 | 100% 自动 |
| **自动验证** | 通过串口自动测试功能 | 100% 自动 |
| **自动记录** | 自动生成结构化文档（含复现步骤） | 100% 自动 |
| **自动版本控制** | 自动创建 Git commit 和标签 | 100% 自动 |
| **自动回滚** | 修复失败时自动回滚代码 | 100% 自动 |

### 与传统调试方式的对比

| 方面 | 传统方式 | 本方案 |
|------|---------|--------|
| **触发** | 手动发现错误 | 自动检测 |
| **分析** | 人工分析 | 自动分析 |
| **修复** | 人工修改代码 | 自动修改 |
| **验证** | 手动编译烧录 | 自动编译烧录 |
| **复位** | 手动按复位键 | 自动复位 |
| **测试** | 手动测试 | 自动测试 |
| **记录** | 人工写文档 | 自动生成文档 |
| **版本** | 手动 git commit | 自动 commit |
| **总时间** | 30-60 分钟 | 1-2 分钟 |

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        debug_loop.py 统一入口                       │
│  --auto . --port COM3 --watch --max-retries 3                       │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     DebugLoopEngine 核心引擎                         │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    EventLoop 事件循环                        │   │
│  │  - 文件监控 (watchdog)                                       │   │
│  │  - 编译状态监控                                              │   │
│  │  - 串口异常监控                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                │                                    │
│          ┌─────────────────────┼─────────────────────┐             │
│          ▼                     ▼                     ▼             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐         │
│  │  Detector    │    │  Analyzer    │    │  Fixer       │         │
│  │  检测器      │    │  分析器      │    │  修复器      │         │
│  │              │    │              │    │              │         │
│  │ - 编译错误   │    │ - 根因分析   │    │ - 代码补丁   │         │
│  │ - 运行时异常 │    │ - 影响范围   │    │ - 配置调整   │         │
│  │ - 硬件故障   │    │ - 修复建议   │    │ - 回滚机制   │         │
│  └──────────────┘    └──────────────┘    └──────────────┘         │
│          │                     │                     │             │
│          └─────────────────────┼─────────────────────┘             │
│                                ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Verifier 验证器                           │   │
│  │  - 重新编译验证                                              │   │
│  │  - 运行测试用例                                              │   │
│  │  - 串口输出验证                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                │                                    │
│                                ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Recorder 记录器                           │   │
│  │  - 问题元数据 (JSON)                                         │   │
│  │  - 分析过程 (Markdown)                                       │   │
│  │  - 修复代码快照                                              │   │
│  │  - 复现步骤                                                  │   │
│  │  - 验证结果                                                  │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                │                                    │
│                                ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    FeedbackLoop 反馈循环                     │   │
│  │  - 记录修复模式到 error-patterns.json                        │   │
│  │  - 更新错误模式库                                            │   │
│  │  - 下次遇到类似错误时更快修复                                │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  docs/debug-logs/                                                   │
│  ├── 2026-07-28_14-30-00/                                          │
│  │   ├── issue-001.json           # 问题元数据                      │
│  │   ├── issue-001-analysis.md    # 分析过程                        │
│  │   ├── issue-001-fix.patch      # 修复补丁                        │
│  │   ├── issue-001-verify.md      # 验证结果                        │
│  │   └── issue-001-reproduce.md   # 复现步骤                        │
│  └── error-patterns.json          # 错误模式库（长期积累）           │
└─────────────────────────────────────────────────────────────────────┘
```

## 核心组件

| 组件 | 职责 | 依赖 |
|------|------|------|
| `DebugLoopEngine` | 事件循环、状态管理、组件协调 | 无 |
| `Detector` | 检测问题（编译、运行时、硬件） | workflow.py, check_elf.py |
| `Analyzer` | 分析根因、影响范围、修复建议 | error_tracker.py |
| `Fixer` | 生成修复补丁、应用修改、回滚 | auto_fix.py |
| `Verifier` | 验证修复（编译、测试、串口） | workflow.py, serial_test.py |
| `Recorder` | 生成结构化文档、记录历史 | dev_log.py |
| `FeedbackLoop` | 反馈循环，持续改进错误模式库 | error_patterns.json |

## 闭环自动化核心概念

### 什么是闭环自动化？

闭环自动化是指系统能够**自我完善**的自动化流程：

```
┌─────────────────────────────────────────────────────────────────────┐
│                        闭环自动化定义                                │
│                                                                     │
│  传统自动化: A → B → C → D (单向流程)                               │
│                                                                     │
│  闭环自动化: A → B → C → D → E → A (循环流程)                       │
│            │                   │                                    │
│            └───────────────────┘                                    │
│                  反馈循环                                            │
│                                                                     │
│  关键特点:                                                          │
│  1. 每次执行都会产生反馈                                            │
│  2. 反馈会改进下一次执行                                            │
│  3. 系统越用越聪明                                                  │
└─────────────────────────────────────────────────────────────────────┘
```

### 本系统的闭环设计

```
┌─────────────────────────────────────────────────────────────────────┐
│                        闭环自动化流程                                │
└─────────────────────────────────────────────────────────────────────┘
                                │
          ┌─────────────────────┼─────────────────────┐
          │                     │                     │
          ▼                     ▼                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  第一次执行      │  │  第 N 次执行      │  │  长期积累        │
│                  │  │                  │  │                  │
│  - 检测到错误    │  │  - 检测到错误    │  │  - 错误模式库    │
│  - 分析根因      │  │  - 匹配历史模式  │  │  - 修复成功率    │
│  - 尝试修复      │  │  - 快速修复      │  │  - 自动化程度    │
│  - 记录结果      │  │  - 记录结果      │  │  - 知识积累      │
│  - 保存模式      │  │  - 更新模式库    │  │                  │
└──────────────────┘  └──────────────────┘  └──────────────────┘
          │                     │                     │
          └─────────────────────┼─────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        反馈循环机制                                  │
│                                                                     │
│  1. 错误检测 → 分析 → 修复 → 验证 → 记录                            │
│                          │                                          │
│                          ▼                                          │
│  2. 保存修复模式到 error-patterns.json                               │
│                          │                                          │
│                          ▼                                          │
│  3. 下次遇到类似错误时:                                             │
│     - 快速匹配历史模式                                              │
│     - 复用已验证的修复方案                                           │
│     - 修复时间从 30 分钟 → 1 分钟                                    │
│                          │                                          │
│                          ▼                                          │
│  4. 系统越用越聪明，修复越快                                         │
└─────────────────────────────────────────────────────────────────────┘
```

### 反馈循环详细设计

```python
class FeedbackLoop:
    """反馈循环 - 系统自我完善"""
    
    def __init__(self, config: ConfigManager):
        """初始化反馈循环"""
        self.config = config
        self.patterns_file = config.get("patterns.error_patterns_file")
        self.patterns = self._load_patterns()
    
    def record_success(self, issue: dict, fix: dict):
        """记录成功的修复
        
        当修复成功时，将修复模式保存到错误模式库
        """
        pattern = {
            "error_type": issue["type"],
            "error_message": issue["source"]["error_message"],
            "error_file": issue["source"]["file"],
            "error_line": issue["source"]["line"],
            "fix_type": fix["type"],
            "fix_description": fix["description"],
            "fix_changes": fix["changes"],
            "success_count": 1,
            "last_used": datetime.now().isoformat()
        }
        
        # 查找是否已有类似模式
        existing = self._find_similar_pattern(pattern)
        if existing:
            # 更新现有模式
            existing["success_count"] += 1
            existing["last_used"] = datetime.now().isoformat()
        else:
            # 添加新模式
            self.patterns.append(pattern)
        
        self._save_patterns()
    
    def find_fix_for_error(self, error_message: str) -> dict:
        """查找错误的修复方案
        
        根据错误信息，查找历史修复记录
        """
        for pattern in self.patterns:
            similarity = self._calculate_similarity(
                error_message, 
                pattern["error_message"]
            )
            if similarity > self.config.get("patterns.match_threshold"):
                return {
                    "found": True,
                    "pattern": pattern,
                    "similarity": similarity,
                    "suggested_fix": pattern["fix_description"]
                }
        
        return {"found": False}
    
    def get_statistics(self) -> dict:
        """获取反馈循环统计"""
        total_patterns = len(self.patterns)
        successful_fixes = sum(p["success_count"] for p in self.patterns)
        
        return {
            "total_patterns": total_patterns,
            "successful_fixes": successful_fixes,
            "average_success_rate": successful_fixes / total_patterns if total_patterns > 0 else 0
        }
```

### 闭环自动化的优势

| 优势 | 说明 |
|------|------|
| **自我完善** | 每次修复都会改进系统 |
| **知识积累** | 错误模式库持续增长 |
| **效率提升** | 重复错误修复时间大幅缩短 |
| **质量保证** | 已验证的修复方案被复用 |
| **零人工干预** | 全程无需人工参与 |

## 工作流程

### 阶段 0: 信息获取（最重要的第一步）

**核心原则：在任何操作前，先获取完整的信息！**

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 0: 信息获取（第一步，最重要的一步）                             │
│                                                                     │
│  ⚠️  重要：先获取信息，再操作！                                       │
│  错误的信息会导致错误的修复！                                        │
└─────────────────────────────────────────────────────────────────────┘
                                │
          ┌─────────────────────┼─────────────────────┐
          ▼                     ▼                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  0.1 硬件信息    │  │  0.2 设计要求    │  │  0.3 项目文档    │
│                  │  │                  │  │                  │
│  - MCU 型号      │  │  - 功能需求      │  │  - 设计文档      │
│  - 外设配置      │  │  - 性能指标      │  │  - 接口文档      │
│  - 调试器类型    │  │  - 约束条件      │  │  - 用户手册      │
│  - 串口端口      │  │  - 验收标准      │  │  - 原理图        │
└──────────────────┘  └──────────────────┘  └──────────────────┘
          │                     │                     │
          └─────────────────────┼─────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  0.4 项目当前状态                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  0.4.1 代码状态                                              │   │
│  │      - 当前代码版本 (git status/diff)                        │   │
│  │      - 最近修改的文件                                        │   │
│  │      - 未提交的修改                                          │   │
│  │                                                             │   │
│  │  0.4.2 编译状态                                              │   │
│  │      - 上一次编译结果                                        │   │
│  │      - 错误/警告列表                                         │   │
│  │      - 编译日志                                              │   │
│  │                                                             │   │
│  │  0.4.3 外设配置                                              │   │
│  │      - CubeMX 配置 (.ioc 文件)                               │   │
│  │      - 时钟配置                                              │   │
│  │      - GPIO 配置                                             │   │
│  │      - DMA 配置                                              │   │
│  │                                                             │   │
│  │  0.4.4 已知问题                                              │   │
│  │      - 之前发现的问题                                        │   │
│  │      - 未解决的 bug                                          │   │
│  │      - 待优化的功能                                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  0.5 硬件状态（需要串口连接）                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │      - 芯片 ID                                               │   │
│  │      - 外设寄存器状态                                        │   │
│  │      - 串口输出日志                                          │   │
│  │      - 调试器连接状态                                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  0.6 信息汇总                                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  将所有信息汇总到 ProjectContext 对象                         │   │
│  │  用于后续所有阶段的决策依据                                   │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 信息获取详细说明

#### 0.1 硬件信息获取

```python
class HardwareInfoCollector:
    """硬件信息收集器"""
    
    def collect(self) -> dict:
        """收集硬件信息"""
        return {
            "mcu": self._get_mcu_info(),
            "peripherals": self._get_peripheral_config(),
            "debugger": self._get_debugger_info(),
            "serial_port": self._get_serial_port()
        }
    
    def _get_mcu_info(self) -> dict:
        """获取 MCU 信息"""
        # 从 .ioc 文件或项目配置中读取
        return {
            "model": "STM32F407ZG",
            "flash_kb": 512,
            "ram_kb": 192,
            "series": "F4"
        }
    
    def _get_peripheral_config(self) -> dict:
        """获取外设配置"""
        # 从 CubeMX 配置中读取
        return {
            "gpio": [...],
            "dma": [...],
            "tim": [...],
            "adc": [...],
            "usart": [...]
        }
    
    def _get_debugger_info(self) -> dict:
        """获取调试器信息"""
        # 检测连接的调试器
        return {
            "type": "auto",
            "port": "COM3",
            "baudrate": 115200
        }
```

#### 0.2 设计要求获取

```python
class DesignRequirementsCollector:
    """设计要求收集器"""
    
    def collect(self) -> dict:
        """收集设计要求"""
        return {
            "features": self._get_features(),
            "performance": self._get_performance_reqs(),
            "constraints": self._get_constraints(),
            "acceptance": self._get_acceptance_criteria()
        }
    
    def _get_features(self) -> list:
        """获取功能需求"""
        # 从设计文档中读取
        return [
            "LCD 触摸屏显示",
            "触摸事件处理",
            "LED 状态指示",
            "蜂鸣器报警"
        ]
    
    def _get_performance_reqs(self) -> dict:
        """获取性能要求"""
        return {
            "touch_response_ms": 50,
            "lcd_refresh_fps": 30,
            "boot_time_ms": 500
        }
```

#### 0.3 项目文档获取

```python
class ProjectDocCollector:
    """项目文档收集器"""
    
    def collect(self) -> dict:
        """收集项目文档"""
        return {
            "design_docs": self._find_design_docs(),
            "schematics": self._find_schematics(),
            "user_manual": self._find_user_manual(),
            "api_docs": self._find_api_docs()
        }
    
    def _find_design_docs(self) -> list:
        """查找设计文档"""
        docs = []
        for pattern in ["**/*design*.md", "**/*spec*.md", "**/*doc*.pdf"]:
            docs.extend(Path(".").glob(pattern))
        return docs
```

#### 0.4 项目状态获取

```python
class ProjectStateCollector:
    """项目状态收集器"""
    
    def collect(self) -> dict:
        """收集项目状态"""
        return {
            "code_state": self._get_code_state(),
            "compile_state": self._get_compile_state(),
            "peripheral_config": self._get_peripheral_config(),
            "known_issues": self._get_known_issues()
        }
    
    def _get_code_state(self) -> dict:
        """获取代码状态"""
        # Git 状态
        git_status = subprocess.run(
            ["git", "status", "--porcelain"],
            capture_output=True, text=True
        )
        
        return {
            "branch": self._get_git_branch(),
            "commit": self._get_git_commit(),
            "modified_files": self._parse_git_status(git_status.stdout),
            "uncommitted_changes": len(git_status.stdout.strip().split('\n')) > 0
        }
    
    def _get_compile_state(self) -> dict:
        """获取编译状态"""
        # 读取 build.log
        build_log = Path("build.log")
        if build_log.exists():
            content = build_log.read_text()
            return {
                "last_compile": self._parse_compile_time(content),
                "errors": self._parse_errors(content),
                "warnings": self._parse_warnings(content)
            }
        return {"last_compile": None, "errors": [], "warnings": []}
    
    def _get_known_issues(self) -> list:
        """获取已知问题"""
        # 从 error-patterns.json 中读取
        patterns_file = Path("docs/debug-logs/error-patterns.json")
        if patterns_file.exists():
            return json.loads(patterns_file.read_text())
        return []
```

#### 0.5 硬件状态获取

```python
class HardwareStateCollector:
    """硬件状态收集器（需要串口连接）"""
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
    
    def collect(self) -> dict:
        """收集硬件状态"""
        if not self.port:
            return {"connected": False}
        
        try:
            import serial
            ser = serial.Serial(self.port, self.baudrate, timeout=1)
            
            # 发送查询命令
            ser.write(b"@STATUS\r\n")
            time.sleep(0.1)
            response = ser.readline().decode('utf-8', errors='ignore')
            
            ser.close()
            
            return {
                "connected": True,
                "status": response,
                "registers": self._parse_registers(response)
            }
        except Exception as e:
            return {"connected": False, "error": str(e)}
```

#### 0.6 信息汇总

```python
class ProjectContext:
    """项目上下文 - 汇总所有信息"""
    
    def __init__(self):
        self.hardware_info = None
        self.design_requirements = None
        self.project_docs = None
        self.project_state = None
        self.hardware_state = None
    
    def collect_all(self, port: str = None) -> dict:
        """收集所有信息"""
        print("📋 阶段 0: 获取项目信息...")
        
        # 0.1 硬件信息
        print("  - 收集硬件信息...")
        self.hardware_info = HardwareInfoCollector().collect()
        
        # 0.2 设计要求
        print("  - 收集设计要求...")
        self.design_requirements = DesignRequirementsCollector().collect()
        
        # 0.3 项目文档
        print("  - 收集项目文档...")
        self.project_docs = ProjectDocCollector().collect()
        
        # 0.4 项目状态
        print("  - 收集项目状态...")
        self.project_state = ProjectStateCollector().collect()
        
        # 0.5 硬件状态
        print("  - 收集硬件状态...")
        self.hardware_state = HardwareStateCollector(port).collect()
        
        print("✅ 信息收集完成")
        
        return {
            "hardware_info": self.hardware_info,
            "design_requirements": self.design_requirements,
            "project_docs": self.project_docs,
            "project_state": self.project_state,
            "hardware_state": self.hardware_state
        }
```

### 信息获取的重要性

| 信息类型 | 用途 | 缺失后果 |
|---------|------|---------|
| **硬件信息** | 确定芯片型号、外设配置 | 选择错误的寄存器、时钟配置 |
| **设计要求** | 确定功能和性能指标 | 修复方向错误、不符合需求 |
| **项目文档** | 了解设计意图和接口 | 误解原有设计、引入新问题 |
| **项目状态** | 了解当前代码和编译状态 | 基于错误的信息做决策 |
| **硬件状态** | 确认硬件实际工作状态 | 误判问题根源 |

### 信息获取后的决策

```
┌─────────────────────────────────────────────────────────────────────┐
│  信息获取 → 决策                                                     │
│                                                                     │
│  获取信息后，系统应该能够回答:                                        │
│                                                                     │
│  1. 这个项目是什么？                                                 │
│     - MCU 型号、外设配置                                             │
│                                                                     │
│  2. 这个项目要做什么？                                               │
│     - 功能需求、性能指标                                             │
│                                                                     │
│  3. 这个项目现在什么状态？                                           │
│     - 代码版本、编译状态、已知问题                                    │
│                                                                     │
│  4. 硬件现在什么状态？                                               │
│     - 芯片是否工作、外设是否正常                                      │
│                                                                     │
│  基于这些信息，系统才能做出正确的修复决策！                           │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 1: 问题检测

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 1: 问题检测                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  1.1 编译错误检测                                            │   │
│  │      - 调用 workflow.py --steps compile                      │   │
│  │      - 解析 build.log                                        │   │
│  │                                                             │   │
│  │  1.2 运行时异常检测                                          │   │
│  │      - 监控串口 HardFault 输出                               │   │
│  │      - 检测看门狗复位                                        │   │
│  │                                                             │   │
│  │  1.3 硬件故障检测                                            │   │
│  │      - I2C/SPI 通信超时                                      │   │
│  │      - ADC/DAC 值异常                                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 2: 根因分析

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 2: 根因分析                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  2.1 错误模式匹配                                            │   │
│  │      - 查找 error-patterns.json 历史记录                     │   │
│  │      - 匹配相似错误的修复方案                                │   │
│  │                                                             │   │
│  │  2.2 代码影响分析                                            │   │
│  │      - 定位错误源文件和行号                                  │   │
│  │      - 分析依赖关系                                          │   │
│  │                                                             │   │
│  │  2.3 生成修复建议                                            │   │
│  │      - 代码修改方案                                          │   │
│  │      - 配置调整方案                                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 3: 自动修复

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 3: 自动修复                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  3.1 创建 Git 快照                                           │   │
│  │      - git add -A                                            │   │
│  │      - git commit -m "debug-loop: snapshot"                 │   │
│  │                                                             │   │
│  │  3.2 应用修复                                                │   │
│  │      - 修改源代码                                            │   │
│  │      - 修改配置文件                                          │   │
│  │                                                             │   │
│  │  3.3 重新编译验证                                            │   │
│  │      - 调用 workflow.py --steps compile                      │   │
│  │      - 检查是否还有错误                                      │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 4: 烧录 + 自动复位

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 4: 烧录 + 自动复位                                            │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  4.1 烧录固件                                                │   │
│  │      - 检测调试器类型 (ST-LINK/DAP-Link/Keil)               │   │
│  │      - 调用对应烧录命令                                      │   │
│  │                                                             │   │
│  │  4.2 自动复位                                                │   │
│  │      - ST-LINK: STM32_Programmer_CLI.exe -rst               │   │
│  │      - DAP-Link: pyocd reset -t STM32F407ZG                 │   │
│  │      - 等待启动时间（200ms）                                 │   │
│  │                                                             │   │
│  │  4.3 复位验证                                                │   │
│  │      - 读取串口启动标记                                      │   │
│  │      - 确认固件版本                                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 5: 功能验证

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 5: 功能验证                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  5.1 串口测试                                                │   │
│  │      - 发送测试命令                                          │   │
│  │      - 验证响应                                              │   │
│  │                                                             │   │
│  │  5.2 外设验证                                                │   │
│  │      - LED 闪烁测试                                          │   │
│  │      - LCD 显示测试                                          │   │
│  │      - 触摸屏测试                                            │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 阶段 6: 记录文档

```
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 6: 记录文档                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  6.1 问题元数据 (issue-XXX.json)                             │   │
│  │      - 问题类型、严重度、状态                                 │   │
│  │      - 错误信息、文件位置                                     │   │
│  │      - 修复方案、验证结果                                     │   │
│  │                                                             │   │
│  │  6.2 分析过程 (issue-XXX-analysis.md)                        │   │
│  │      - 问题描述                                              │   │
│  │      - 根因分析过程                                          │   │
│  │      - 影响范围                                              │   │
│  │                                                             │   │
│  │  6.3 修复补丁 (issue-XXX-fix.patch)                           │   │
│  │      - git diff 格式的代码变更                               │   │
│  │      - 修复前后对比                                          │   │
│  │                                                             │   │
│  │  6.4 复现步骤 (issue-XXX-reproduce.md)                       │   │
│  │      - 触发条件                                              │   │
│  │      - 复现步骤                                              │   │
│  │      - 预期结果 vs 实际结果                                  │   │
│  │                                                             │   │
│  │  6.5 验证结果 (issue-XXX-verify.md)                          │   │
│  │      - 编译结果                                              │   │
│  │      - 烧录结果                                              │   │
│  │      - 功能测试结果                                          │   │
│  │                                                             │   │
│  │  6.6 创建 Git 标签                                           │   │
│  │      - git tag -a debug/fix-XXX -m "Fix issue XXX"          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

## 数据结构

### 问题元数据 (issue-XXX.json)

```json
{
  "id": "issue-001",
  "timestamp": "2026-07-28T14:30:00Z",
  "type": "compile|runtime|hardware",
  "severity": "critical|high|medium|low",
  "status": "detected|analyzing|fixing|verified|resolved|failed",
  "source": {
    "file": "USER/TOUCH/xpt2046.c",
    "line": 19,
    "function": "XPT2046_Scan",
    "error_message": "warning: variable 'x' may be used uninitialized"
  },
  "context": {
    "git_branch": "main",
    "git_commit": "abc123",
    "modified_files": ["USER/TOUCH/xpt2046.c"],
    "last_compile_time": "2026-07-28T14:25:00Z"
  },
  "analysis": {
    "root_cause": "全局变量未声明为 volatile",
    "impact_scope": ["XPT2046_Scan", "FT5426_Scan"],
    "related_patterns": ["GEN-006"]
  },
  "fix": {
    "type": "code_change",
    "description": "添加 volatile 关键字",
    "patch_file": "issue-001-fix.patch"
  },
  "verification": {
    "compile_result": "success",
    "flash_result": "success",
    "test_result": "pass"
  },
  "reproduce": {
    "trigger": "修改 xpt2046.c 后编译",
    "steps": ["1. 修改变量声明", "2. 运行编译"],
    "expected": "无警告",
    "actual": "有警告"
  }
}
```

## 配置文件详解

### debug-loop.json 完整配置

```json
{
  "version": "1.0.0",
  
  "project": {
    "name": "触摸屏_usart",
    "chip": "STM32F407ZG",
    "series": "F4",
    "flash_kb": 512,
    "ram_kb": 192
  },
  
  "debugger": {
    "type": "auto",
    "port": "COM3",
    "baudrate": 115200,
    "reset_method": "swd",
    "reset_delay_ms": 200,
    "timeout_ms": 5000
  },
  
  "workflow": {
    "compile_command": "python workflow.py --auto . --steps compile",
    "analyze_command": "python workflow.py --auto . --steps analyze",
    "flash_command": "STM32_Programmer_CLI.exe -c port=SWD -w {hex_file} -v -rst",
    "test_command": "python serial_test.py --port {port} --test tests.json",
    "build_log": "build.log"
  },
  
  "detection": {
    "file_watch": true,
    "compile_monitor": true,
    "serial_monitor": true,
    "watch_interval_sec": 2,
    "watch_patterns": ["*.c", "*.h", "*.s"],
    "watch_dirs": ["USER/", "Main/", "Common/"]
  },
  
  "fix": {
    "auto_fix": true,
    "max_retries": 3,
    "retry_delay_sec": 2,
    "backup_before_fix": true,
    "rollback_on_failure": true,
    "backup_dir": ".debug-backups"
  },
  
  "git": {
    "enabled": true,
    "auto_commit": true,
    "commit_prefix": "debug-loop",
    "auto_tag": true,
    "tag_prefix": "debug",
    "tag_on_success": true,
    "keep_snapshots": 10
  },
  
  "documentation": {
    "log_dir": "docs/debug-logs",
    "generate_patch": true,
    "generate_reproduce": true,
    "generate_analysis": true,
    "generate_verify": true,
    "auto_cleanup_days": 30
  },
  
  "patterns": {
    "error_patterns_file": "docs/debug-logs/error-patterns.json",
    "auto_record_patterns": true,
    "match_threshold": 0.8,
    "max_patterns": 1000
  },
  
  "notifications": {
    "enabled": true,
    "on_success": true,
    "on_failure": true,
    "on_timeout": true
  }
}
```

### 配置项说明

#### project 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `name` | string | 项目名称 | 自动检测 |
| `chip` | string | 芯片型号 | 自动检测 |
| `series` | string | 芯片系列 (F0/F1/F4/H7 等) | 自动检测 |
| `flash_kb` | number | Flash 大小 (KB) | 自动检测 |
| `ram_kb` | number | RAM 大小 (KB) | 自动检测 |

#### debugger 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `type` | string | 调试器类型: auto/stlink/cmsis-dap/jlink/keil | auto |
| `port` | string | 串口端口 | 自动检测 |
| `baudrate` | number | 波特率 | 115200 |
| `reset_method` | string | 复位方式: swd/dtr_rts/break | swd |
| `reset_delay_ms` | number | 复位后等待时间 (ms) | 200 |
| `timeout_ms` | number | 调试器通信超时 (ms) | 5000 |

#### workflow 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `compile_command` | string | 编译命令 | workflow.py 编译 |
| `analyze_command` | string | 分析命令 | workflow.py 分析 |
| `flash_command` | string | 烧录命令 (支持变量替换) | STM32_Programmer |
| `test_command` | string | 测试命令 (支持变量替换) | serial_test.py |
| `build_log` | string | 编译日志文件 | build.log |

**变量替换**:
- `{hex_file}`: HEX 文件路径
- `{port}`: 串口端口
- `{chip}`: 芯片型号

#### detection 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `file_watch` | bool | 启用文件监控 | true |
| `compile_monitor` | bool | 启用编译监控 | true |
| `serial_monitor` | bool | 启用串口监控 | true |
| `watch_interval_sec` | number | 文件检查间隔 (秒) | 2 |
| `watch_patterns` | array | 监控文件模式 | ["*.c", "*.h"] |
| `watch_dirs` | array | 监控目录 | ["USER/", "Main/"] |

#### fix 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `auto_fix` | bool | 自动修复 | true |
| `max_retries` | number | 最大重试次数 | 3 |
| `retry_delay_sec` | number | 重试间隔 (秒) | 2 |
| `backup_before_fix` | bool | 修复前备份 | true |
| `rollback_on_failure` | bool | 失败时回滚 | true |
| `backup_dir` | string | 备份目录 | .debug-backups |

#### git 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `enabled` | bool | 启用 Git 版本控制 | true |
| `auto_commit` | bool | 自动创建 commit | true |
| `commit_prefix` | string | commit 消息前缀 | debug-loop |
| `auto_tag` | bool | 自动创建标签 | true |
| `tag_prefix` | string | 标签前缀 | debug |
| `tag_on_success` | bool | 成功时创建标签 | true |
| `keep_snapshots` | number | 保留快照数量 | 10 |

#### documentation 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `log_dir` | string | 文档目录 | docs/debug-logs |
| `generate_patch` | bool | 生成修复补丁 | true |
| `generate_reproduce` | bool | 生成复现步骤 | true |
| `generate_analysis` | bool | 生成分析文档 | true |
| `generate_verify` | bool | 生成验证报告 | true |
| `auto_cleanup_days` | number | 自动清理天数 | 30 |

#### patterns 配置

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `error_patterns_file` | string | 错误模式库文件 | error-patterns.json |
| `auto_record_patterns` | bool | 自动记录新模式 | true |
| `match_threshold` | number | 模式匹配阈值 (0-1) | 0.8 |
| `max_patterns` | number | 最大模式数量 | 1000 |

### 配置文件位置

配置文件按优先级查找：

1. 命令行指定: `--config path/to/debug-loop.json`
2. 项目根目录: `./debug-loop.json`
3. 用户配置: `~/.debug-loop/config.json`
4. 默认配置: 内置默认值

---

## 代码模块化设计

### 目录结构

```
D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts\
├── debug_loop.py              # 主入口
├── debug_loop_engine/         # 核心引擎模块
│   ├── __init__.py
│   ├── engine.py              # DebugLoopEngine 主类
│   ├── config.py              # 配置管理
│   └── state.py               # 状态管理
├── debug_loop_components/     # 组件模块
│   ├── __init__.py
│   ├── detector.py            # Detector 检测器
│   ├── analyzer.py            # Analyzer 分析器
│   ├── fixer.py               # Fixer 修复器
│   ├── verifier.py            # Verifier 验证器
│   └── recorder.py            # Recorder 记录器
├── debug_loop_tools/          # 工具封装模块
│   ├── __init__.py
│   ├── toolchain.py           # ToolChain 工具链封装
│   ├── git_manager.py         # GitManager 版本控制
│   ├── debugger.py            # Debugger 调试器封装
│   └── serial_tool.py         # SerialTool 串口工具
└── debug_loop_tests/          # 测试模块
    ├── __init__.py
    ├── test_detector.py
    ├── test_analyzer.py
    ├── test_fixer.py
    ├── test_verifier.py
    ├── test_recorder.py
    ├── test_engine.py
    └── test_integration.py
```

### 模块依赖关系

```
debug_loop.py
    │
    ▼
debug_loop_engine/
    │
    ├─→ debug_loop_components/
    │       │
    │       ├─→ detector.py     → toolchain.py, config.py
    │       ├─→ analyzer.py     → config.py, state.py
    │       ├─→ fixer.py        → git_manager.py, toolchain.py
    │       ├─→ verifier.py     → toolchain.py, debugger.py
    │       └─→ recorder.py     → config.py
    │
    └─→ debug_loop_tools/
            │
            ├─→ toolchain.py    → 现有工具脚本
            ├─→ git_manager.py  → Git 命令
            ├─→ debugger.py     → STM32_Programmer/pyOCD
            └─→ serial_tool.py  → pyserial
```

### 模块接口定义

#### DebugLoopEngine 接口

```python
class DebugLoopEngine:
    """核心引擎 - 协调所有组件"""
    
    def __init__(self, config: dict):
        """初始化引擎
        
        Args:
            config: 配置字典，从 debug-loop.json 加载
        """
        self.config = ConfigManager(config)
        self.state = StateManager()
        self.detector = Detector(self.config)
        self.analyzer = Analyzer(self.config)
        self.fixer = Fixer(self.config)
        self.verifier = Verifier(self.config)
        self.recorder = Recorder(self.config)
        self.git = GitManager(self.config)
    
    def run(self) -> dict:
        """运行一次完整的调试循环
        
        Returns:
            dict: 运行结果
                - success: bool
                - issues_found: int
                - issues_fixed: int
                - issues_failed: int
        """
        pass
    
    def run_cycle(self) -> dict:
        """运行单个调试周期
        
        Returns:
            dict: 周期结果
        """
        pass
    
    def stop(self):
        """停止引擎"""
        pass
```

#### Detector 接口

```python
class Detector:
    """问题检测器"""
    
    def __init__(self, config: ConfigManager):
        """初始化检测器"""
        self.config = config
        self.toolchain = ToolChain(config)
    
    def detect_compile_errors(self) -> List[dict]:
        """检测编译错误
        
        Returns:
            List[dict]: 错误列表
                - file: str
                - line: int
                - message: str
                - severity: str
        """
        pass
    
    def detect_runtime_errors(self) -> List[dict]:
        """检测运行时错误"""
        pass
    
    def detect_hardware_errors(self) -> List[dict]:
        """检测硬件错误"""
        pass
    
    def detect_all(self) -> List[dict]:
        """检测所有类型的问题"""
        pass
```

#### Analyzer 接口

```python
class Analyzer:
    """问题分析器"""
    
    def __init__(self, config: ConfigManager):
        """初始化分析器"""
        self.config = config
        self.patterns = self._load_patterns()
    
    def analyze(self, issue: dict) -> dict:
        """分析问题
        
        Args:
            issue: 问题信息
            
        Returns:
            dict: 分析结果
                - root_cause: str
                - impact_scope: List[str]
                - fix_suggestions: List[dict]
                - matched_patterns: List[str]
        """
        pass
    
    def match_pattern(self, error_message: str) -> dict:
        """匹配错误模式"""
        pass
    
    def record_pattern(self, issue: dict, fix: dict):
        """记录新的错误模式"""
        pass
```

#### Fixer 接口

```python
class Fixer:
    """问题修复器"""
    
    def __init__(self, config: ConfigManager):
        """初始化修复器"""
        self.config = config
        self.git = GitManager(config)
    
    def fix(self, issue: dict, analysis: dict) -> dict:
        """修复问题
        
        Args:
            issue: 问题信息
            analysis: 分析结果
            
        Returns:
            dict: 修复结果
                - success: bool
                - changes: List[dict]
                - patch_file: str
        """
        pass
    
    def rollback(self, snapshot_id: str = None) -> bool:
        """回滚修复"""
        pass
    
    def create_snapshot(self) -> str:
        """创建快照"""
        pass
```

#### Verifier 接口

```python
class Verifier:
    """修复验证器"""
    
    def __init__(self, config: ConfigManager):
        """初始化验证器"""
        self.config = config
        self.toolchain = ToolChain(config)
        self.debugger = Debugger(config)
    
    def verify_compile(self) -> dict:
        """验证编译"""
        pass
    
    def verify_flash(self) -> dict:
        """验证烧录"""
        pass
    
    def verify_function(self) -> dict:
        """验证功能"""
        pass
    
    def verify_all(self) -> dict:
        """验证所有"""
        pass
```

#### Recorder 接口

```python
class Recorder:
    """文档记录器"""
    
    def __init__(self, config: ConfigManager):
        """初始化记录器"""
        self.config = config
    
    def record_issue(self, issue: dict, analysis: dict, fix: dict, verification: dict) -> str:
        """记录问题
        
        Returns:
            str: 文档目录路径
        """
        pass
    
    def generate_analysis_doc(self, issue: dict, analysis: dict) -> str:
        """生成分析文档"""
        pass
    
    def generate_reproduce_doc(self, issue: dict) -> str:
        """生成复现文档"""
        pass
    
    def generate_verify_doc(self, verification: dict) -> str:
        """生成验证文档"""
        pass
    
    def generate_patch(self, fix: dict) -> str:
        """生成修复补丁"""
        pass
```

#### GitManager 接口

```python
class GitManager:
    """Git 版本控制管理器"""
    
    def __init__(self, config: ConfigManager):
        """初始化 Git 管理器"""
        self.config = config
        self.is_git_repo = self._check_git_repo()
    
    def create_snapshot(self, message: str = None) -> str:
        """创建 Git 快照
        
        Returns:
            str: 快照 ID (branch@commit)
        """
        pass
    
    def rollback(self, snapshot_id: str = None) -> bool:
        """回滚到快照"""
        pass
    
    def create_tag(self, tag_name: str, message: str = None) -> str:
        """创建标签
        
        Returns:
            str: 标签名
        """
        pass
    
    def list_snapshots(self, limit: int = 10) -> List[dict]:
        """列出快照"""
        pass
    
    def list_tags(self) -> List[dict]:
        """列出标签"""
        pass
```

### 模块独立性保证

每个模块都设计为**独立可测试**：

| 模块 | 独立性 | 测试方式 |
|------|--------|---------|
| `ConfigManager` | ✅ 完全独立 | 单元测试，Mock 配置 |
| `StateManager` | ✅ 完全独立 | 单元测试，内存状态 |
| `Detector` | ✅ 依赖 ToolChain | Mock ToolChain |
| `Analyzer` | ✅ 依赖 Config | Mock Config + 测试数据 |
| `Fixer` | ✅ 依赖 GitManager | Mock GitManager |
| `Verifier` | ✅ 依赖 ToolChain + Debugger | Mock 工具链 |
| `Recorder` | ✅ 依赖 Config | Mock Config + 文件系统 |
| `GitManager` | ✅ 依赖 Git | 使用测试仓库 |
| `ToolChain` | ⚠️ 依赖外部工具 | 集成测试 |

---

### 配置示例

#### 示例 1: 仅调试模式（不烧录）

```json
{
  "fix": {
    "auto_fix": true
  },
  "workflow": {
    "flash_command": null
  }
}
```

#### 示例 2: 使用 DAP-Link 调试器

```json
{
  "debugger": {
    "type": "cmsis-dap",
    "reset_method": "swd"
  },
  "workflow": {
    "flash_command": "pyocd flash -t STM32F407ZG {hex_file}",
    "reset_command": "pyocd reset -t STM32F407ZG"
  }
}
```

#### 示例 3: 禁用 Git 版本控制

```json
{
  "git": {
    "enabled": false
  }
}
```

#### 示例 4: 自定义文档目录

```json
{
  "documentation": {
    "log_dir": "debug-docs/2026-Q3"
  }
}
```

---

## 命令行接口

```bash
# 基本用法
python debug_loop.py --auto . --port COM3

# 完整选项
python debug_loop.py --auto . \
    --port COM3 \
    --debugger cmsis-dap \
    --watch \
    --max-retries 3 \
    --timeout 300 \
    --log-dir docs/debug-logs \
    --auto-fix \
    --auto-flash \
    --auto-reset \
    --auto-verify

# 使用自定义配置文件
python debug_loop.py --auto . --config my-config.json

# 只分析不修改
python debug_loop.py --auto . --dry-run
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--auto .` | 自动检测项目 | 必需 |
| `--port COM3` | 串口端口 | 无（运行时检测） |
| `--debugger TYPE` | 调试器类型 | auto（自动检测） |
| `--watch` | 启用文件监控 | False |
| `--max-retries N` | 最大重试次数 | 3 |
| `--timeout N` | 超时时间（秒） | 300 |
| `--log-dir DIR` | 文档目录 | docs/debug-logs |
| `--auto-fix` | 自动修复 | True |
| `--auto-flash` | 自动烧录 | True |
| `--auto-reset` | 自动复位 | True |
| `--auto-verify` | 自动验证 | True |
| `--dry-run` | 只分析不修改 | False |

## Git 版本控制

### 标签命名规范

| 标签类型 | 格式 | 示例 |
|---------|------|------|
| **调试快照** | `debug/snapshot-{timestamp}` | `debug/snapshot-20260728-143000` |
| **修复版本** | `debug/fix-{issue-id}` | `debug/fix-001` |
| **稳定版本** | `debug/stable-{version}` | `debug/stable-v1.0` |
| **里程碑** | `debug/milestone-{name}` | `debug/milestone-led-ok` |

### 回滚命令

```bash
# 回滚到上一个 commit
git reset --hard HEAD~1

# 回滚到指定 commit
git reset --hard <commit-hash>

# 回滚到标签
git checkout debug/fix-001

# 查看所有标签
git tag -l "debug/*"
```

## 错误处理策略

| 失败类型 | 处理策略 | 最大重试 | 回滚 |
|---------|---------|---------|------|
| 编译错误 | 自动修复 → 重编译 | 3 次 | 是 (Git commit) |
| 修复失败 | 回滚 → 记录 → 跳过 | 1 次 | 是 (Git reset) |
| 烧录失败 | 重试烧录 → 检查连接 | 3 次 | 否 |
| 复位失败 | 重试复位 → 手动复位提示 | 3 次 | 否 |
| 验证失败 | 回滚 → 记录 → 等待干预 | 1 次 | 是 (Git reset) |

## 调试器支持

| 调试器 | 烧录命令 | 复位命令 |
|--------|---------|---------|
| **ST-LINK** | `STM32_Programmer_CLI.exe -c port=SWD -w {hex} -v -rst` | `STM32_Programmer_CLI.exe -c port=SWD -rst` |
| **DAP-Link** | `pyocd flash -t STM32F407ZG {hex}` | `pyocd reset -t STM32F407ZG` |
| **Keil** | `UV4.exe -f project.uvprojx -t Target 1` | Keil 内置复位 |

## 文档目录结构

```
docs/debug-logs/
├── 2026-07-28_14-30-00/
│   ├── issue-001.json           # 问题元数据
│   ├── issue-001-analysis.md    # 分析过程
│   ├── issue-001-fix.patch      # 修复补丁
│   ├── issue-001-reproduce.md   # 复现步骤
│   └── issue-001-verify.md      # 验证结果
├── 2026-07-28_15-00-00/
│   ├── issue-002.json
│   └── ...
└── error-patterns.json          # 错误模式库
```

## 依赖工具

| 工具 | 用途 | 必需 |
|------|------|------|
| Python 3.8+ | 运行脚本 | ✅ |
| Keil MDK-ARM | 编译工具链 | ✅ |
| Git | 版本控制 | ✅ |
| STM32CubeProgrammer | 烧录 (ST-LINK) | ⚠️ |
| pyOCD | 烧录 (DAP-Link) | ⚠️ |
| pyserial | 串口通信 | ⚠️ |

## 验收系统

### 验收标准

#### 功能验收标准

| 编号 | 功能 | 验收标准 | 优先级 |
|------|------|---------|--------|
| F-001 | 问题检测 | 能检测编译错误、运行时异常、硬件故障 | P0 |
| F-002 | 根因分析 | 能分析问题根因，匹配历史模式 | P0 |
| F-003 | 自动修复 | 能自动修复 80% 以上的常见编译错误 | P0 |
| F-004 | 自动烧录 | 能自动检测调试器并烧录固件 | P0 |
| F-005 | 自动复位 | 烧录后能自动复位芯片 | P0 |
| F-006 | 功能验证 | 能通过串口测试验证功能 | P1 |
| F-007 | 文档生成 | 能生成完整的调试文档 | P1 |
| F-008 | Git 版本控制 | 能自动创建 commit 和标签 | P1 |
| F-009 | 错误回滚 | 修复失败时能自动回滚 | P1 |
| F-010 | 文件监控 | 能监控文件变化并自动触发 | P2 |

#### 非功能验收标准

| 编号 | 类别 | 验收标准 | 优先级 |
|------|------|---------|--------|
| NF-001 | 性能 | 检测阶段 < 5 秒 | P0 |
| NF-002 | 性能 | 修复阶段 < 30 秒 | P0 |
| NF-003 | 性能 | 验证阶段 < 60 秒 | P1 |
| NF-004 | 可靠性 | 自动修复成功率 > 80% | P0 |
| NF-005 | 可靠性 | 回滚成功率 100% | P0 |
| NF-006 | 可用性 | 命令行帮助清晰完整 | P1 |
| NF-007 | 可维护性 | 代码模块化，易于扩展 | P1 |
| NF-008 | 可测试性 | 单元测试覆盖率 > 70% | P2 |

### 验收测试用例

#### 测试用例 1: 编译错误检测

```python
class TestCompileErrorDetection:
    """编译错误检测测试"""
    
    def test_detect_undefined_reference(self):
        """测试检测 undefined reference 错误"""
        # 准备：创建一个有 undefined reference 错误的项目
        # 执行：运行 Detector.detect_compile_errors()
        # 验证：返回正确的错误信息
        
    def test_detect_syntax_error(self):
        """测试检测语法错误"""
        # 准备：创建一个有语法错误的文件
        # 执行：运行 Detector.detect_compile_errors()
        # 验证：返回正确的错误信息
        
    def test_detect_no_errors(self):
        """测试无错误情况"""
        # 准备：确保项目无编译错误
        # 执行：运行 Detector.detect_compile_errors()
        # 验证：返回空列表
```

#### 测试用例 2: 自动修复

```python
class TestAutoFix:
    """自动修复测试"""
    
    def test_fix_missing_header(self):
        """测试修复缺少头文件"""
        # 准备：创建缺少头文件的代码
        # 执行：运行 Fixer.fix()
        # 验证：头文件被添加，编译通过
        
    def test_fix_volatile_missing(self):
        """测试修复缺少 volatile"""
        # 准备：创建缺少 volatile 的代码
        # 执行：运行 Fixer.fix()
        # 验证：volatile 被添加
        
    def test_fix_rollback_on_failure(self):
        """测试修复失败时回滚"""
        # 准备：创建无法自动修复的错误
        # 执行：运行 Fixer.fix()
        # 验证：代码被回滚到原始状态
```

#### 测试用例 3: Git 版本控制

```python
class TestGitVersionControl:
    """Git 版本控制测试"""
    
    def test_create_snapshot(self):
        """测试创建 Git 快照"""
        # 准备：确保是 Git 仓库
        # 执行：运行 GitManager.create_snapshot()
        # 验证：Git commit 被创建
        
    def test_rollback_to_snapshot(self):
        """测试回滚到快照"""
        # 准备：创建快照
        # 执行：运行 GitManager.rollback()
        # 验证：代码被回滚
        
    def test_create_tag(self):
        """测试创建标签"""
        # 准备：完成一次修复
        # 执行：运行 GitManager.create_tag()
        # 验证：Git tag 被创建
```

#### 测试用例 4: 文档生成

```python
class TestDocumentation:
    """文档生成测试"""
    
    def test_generate_analysis_doc(self):
        """测试生成分析文档"""
        # 准备：创建问题和分析结果
        # 执行：运行 Recorder.generate_analysis_doc()
        # 验证：Markdown 文档被生成
        
    def test_generate_reproduce_doc(self):
        """测试生成复现文档"""
        # 准备：创建问题信息
        # 执行：运行 Recorder.generate_reproduce_doc()
        # 验证：复现步骤文档被生成
        
    def test_generate_patch(self):
        """测试生成修复补丁"""
        # 准备：创建修复信息
        # 执行：运行 Recorder.generate_patch()
        # 验证：diff 格式补丁被生成
```

#### 测试用例 5: 完整流程

```python
class TestFullFlow:
    """完整流程测试"""
    
    def test_detect_fix_verify_flow(self):
        """测试检测-修复-验证完整流程"""
        # 准备：创建一个有编译错误的项目
        # 执行：运行 DebugLoopEngine.run()
        # 验证：
        #   1. 错误被检测
        #   2. 错误被修复
        #   3. 编译通过
        #   4. 文档被生成
        
    def test_multiple_issues(self):
        """测试多个问题处理"""
        # 准备：创建多个编译错误
        # 执行：运行 DebugLoopEngine.run()
        # 验证：所有问题都被处理
```

### 验收测试执行

```bash
# 运行所有测试
python -m pytest debug_loop_tests/ -v

# 运行特定模块测试
python -m pytest debug_loop_tests/test_detector.py -v
python -m pytest debug_loop_tests/test_fixer.py -v

# 运行并生成覆盖率报告
python -m pytest debug_loop_tests/ --cov=debug_loop_engine --cov-report=html

# 运行集成测试
python -m pytest debug_loop_tests/test_integration.py -v
```

### 验收检查清单

| 阶段 | 检查项 | 状态 |
|------|--------|------|
| **开发前** | 设计文档已审阅 | ☐ |
| | 接口定义已确认 | ☐ |
| | 测试用例已定义 | ☐ |
| | 依赖工具已安装 | ☐ |
| **开发中** | 单元测试通过 | ☐ |
| | 代码审查完成 | ☐ |
| | 文档已更新 | ☐ |
| | 模块独立性验证 | ☐ |
| **开发后** | 功能验收通过 | ☐ |
| | 非功能验收通过 | ☐ |
| | 集成测试通过 | ☐ |
| | 用户验收通过 | ☐ |
| | 性能测试通过 | ☐ |
| | 安全测试通过 | ☐ |

### 验收流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                        验收流程                                      │
└─────────────────────────────────────────────────────────────────────┘
                                │
          ┌─────────────────────┼─────────────────────┐
          ▼                     ▼                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  阶段 1:         │  │  阶段 2:         │  │  阶段 3:         │
│  代码审查        │  │  单元测试        │  │  集成测试        │
│                  │  │                  │  │                  │
│  - 代码规范      │  │  - 各组件测试    │  │  - 完整流程      │
│  - 接口一致性    │  │  - 边界条件      │  │  - 工具集成      │
│  - 文档完整性    │  │  - 错误处理      │  │  - 端到端测试    │
└──────────────────┘  └──────────────────┘  └──────────────────┘
          │                     │                     │
          └─────────────────────┼─────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 4: 功能验收                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  测试项                    | 验收标准          | 状态       │   │
│  │  -------------------------|------------------|-----------│   │
│  │  问题检测                  | 检测编译错误      | ☐         │   │
│  │  自动修复                  | 修复 80% 常见错误 | ☐         │   │
│  │  自动烧录                  | 检测调试器并烧录  | ☐         │   │
│  │  自动复位                  | 烧录后自动复位    | ☐         │   │
│  │  自动验证                  | 串口测试通过      | ☐         │   │
│  │  文档生成                  | 生成完整文档      | ☐         │   │
│  │  Git 版本控制              | 自动 commit/tag   | ☐         │   │
│  │  错误回滚                  | 修复失败时回滚    | ☐         │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 5: 非功能验收                                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  类别      | 测试项              | 验收标准    | 状态       │   │
│  │  ----------|--------------------|------------|-----------│   │
│  │  性能      | 检测阶段            | < 5 秒     | ☐         │   │
│  │  性能      | 修复阶段            | < 30 秒    | ☐         │   │
│  │  性能      | 验证阶段            | < 60 秒    | ☐         │   │
│  │  可靠性    | 自动修复成功率      | > 80%      | ☐         │   │
│  │  可靠性    | 回滚成功率          | 100%       | ☐         │   │
│  │  可用性    | 命令行帮助          | 清晰完整   | ☐         │   │
│  │  可维护性  | 代码模块化          | 易于扩展   | ☐         │   │
│  │  可测试性  | 单元测试覆盖率      | > 70%      | ☐         │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 6: 用户验收                                                    │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  1. 用户使用系统进行实际项目调试                              │   │
│  │  2. 记录用户反馈                                              │   │
│  │  3. 修复发现的问题                                            │   │
│  │  4. 用户确认验收                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  阶段 7: 生成验收报告                                                │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  1. 汇总所有测试结果                                          │   │
│  │  2. 生成验收报告                                              │   │
│  │  3. 保存到 docs/acceptance-report.md                         │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 验收报告模板

```markdown
# 验收报告

## 项目信息
- 项目名称: debug_loop.py
- 版本: 1.0.0
- 验收日期: 2026-07-28
- 验收人员: 用户

## 功能验收

| 功能 | 验收标准 | 测试结果 | 状态 |
|------|---------|---------|------|
| 问题检测 | 检测编译错误 | 通过 | ✅ |
| 自动修复 | 修复 80% 常见错误 | 通过 | ✅ |
| 自动烧录 | 检测调试器并烧录 | 通过 | ✅ |
| 自动复位 | 烧录后自动复位 | 通过 | ✅ |
| 文档生成 | 生成完整文档 | 通过 | ✅ |

## 非功能验收

| 类别 | 验收标准 | 测试结果 | 状态 |
|------|---------|---------|------|
| 性能 | 检测 < 5秒 | 3秒 | ✅ |
| 可靠性 | 修复成功率 > 80% | 85% | ✅ |
| 可测试性 | 覆盖率 > 70% | 75% | ✅ |

## 问题记录

| 问题 | 描述 | 状态 |
|------|------|------|
| 无 | - | - |

## 验收结论

✅ 通过验收，可以发布使用

---
*验收报告由 debug_loop.py 自动生成*
```

---

## 工具集成详细说明

### 与现有工具的集成关系

```
┌─────────────────────────────────────────────────────────────────────┐
│                        debug_loop.py 集成架构                        │
└─────────────────────────────────────────────────────────────────────┘
                                │
          ┌─────────────────────┼─────────────────────┐
          ▼                     ▼                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  编译工具链      │  │  烧录工具链      │  │  测试工具链      │
│                  │  │                  │  │                  │
│  - workflow.py   │  │  - STM32_Programmer│  │  - serial_test.py│
│  - auto_fix.py   │  │  - pyocd         │  │  - serial_debug.py│
│  - check_elf.py  │  │  - Keil UV4      │  │  - renode_sim.py │
└──────────────────┘  └──────────────────┘  └──────────────────┘
          │                     │                     │
          └─────────────────────┼─────────────────────┘
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        调用接口定义                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 工具集成详细说明

#### 1. workflow.py 集成

**用途**: 编译、分析、烧录

```python
class WorkflowIntegration:
    """workflow.py 集成封装"""
    
    def __init__(self, project_dir: str, scripts_dir: str):
        self.project_dir = project_dir
        self.scripts_dir = scripts_dir
        self.workflow_script = Path(scripts_dir) / "workflow.py"
    
    def compile(self) -> dict:
        """编译项目
        
        调用: python workflow.py --auto . --steps compile
        
        Returns:
            dict: 编译结果
                - success: bool
                - errors: List[dict]
                - warnings: List[dict]
                - build_log: str
        """
        cmd = [
            "python", str(self.workflow_script),
            "--auto", self.project_dir,
            "--steps", "compile"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "success": result.returncode == 0,
            "errors": self._parse_errors(result.stdout),
            "warnings": self._parse_warnings(result.stdout),
            "build_log": result.stdout
        }
    
    def analyze(self) -> dict:
        """静态分析
        
        调用: python workflow.py --auto . --steps analyze
        """
        cmd = [
            "python", str(self.workflow_script),
            "--auto", self.project_dir,
            "--steps", "analyze"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        return {
            "success": result.returncode == 0,
            "output": result.stdout
        }
    
    def flash(self, hex_file: str, port: str = None) -> dict:
        """烧录固件
        
        调用: python workflow.py --auto . --steps flash --port COM3
        """
        cmd = [
            "python", str(self.workflow_script),
            "--auto", self.project_dir,
            "--steps", "flash"
        ]
        if port:
            cmd.extend(["--port", port])
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        return {
            "success": result.returncode == 0,
            "output": result.stdout
        }
```

#### 2. auto_fix.py 集成

**用途**: 自动修复编译错误

```python
class AutoFixIntegration:
    """auto_fix.py 集成封装"""
    
    def __init__(self, project_dir: str, scripts_dir: str):
        self.project_dir = project_dir
        self.scripts_dir = scripts_dir
        self.auto_fix_script = Path(scripts_dir) / "auto_fix.py"
    
    def fix_error(self, error_message: str) -> dict:
        """修复单个错误
        
        调用: python auto_fix.py --auto . --error "xxx" --auto-fix
        
        Args:
            error_message: 错误信息
            
        Returns:
            dict: 修复结果
                - success: bool
                - changes: List[dict]
                - file_modified: str
        """
        cmd = [
            "python", str(self.auto_fix_script),
            "--auto", self.project_dir,
            "--error", error_message,
            "--auto-fix"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "success": result.returncode == 0,
            "changes": self._parse_changes(result.stdout),
            "output": result.stdout
        }
    
    def fix_all(self, errors: List[dict]) -> dict:
        """修复所有错误
        
        Args:
            errors: 错误列表
            
        Returns:
            dict: 修复结果
                - fixed_count: int
                - failed_count: int
                - details: List[dict]
        """
        fixed = 0
        failed = 0
        details = []
        
        for error in errors:
            result = self.fix_error(error["message"])
            if result["success"]:
                fixed += 1
            else:
                failed += 1
            details.append(result)
        
        return {
            "fixed_count": fixed,
            "failed_count": failed,
            "details": details
        }
```

#### 3. check_elf.py 集成

**用途**: ELF 文件检查

```python
class CheckElfIntegration:
    """check_elf.py 集成封装"""
    
    def __init__(self, project_dir: str, scripts_dir: str):
        self.project_dir = project_dir
        self.scripts_dir = scripts_dir
        self.check_elf_script = Path(scripts_dir) / "check_elf.py"
    
    def check(self) -> dict:
        """检查 ELF 文件
        
        调用: python check_elf.py --auto .
        
        Returns:
            dict: 检查结果
                - success: bool
                - flash_usage: dict
                - ram_usage: dict
                - symbols: List[dict]
        """
        cmd = [
            "python", str(self.check_elf_script),
            "--auto", self.project_dir
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "success": result.returncode == 0,
            "output": result.stdout
        }
```

#### 4. error_tracker.py 集成

**用途**: 错误追踪和模式匹配

```python
class ErrorTrackerIntegration:
    """error_tracker.py 集成封装"""
    
    def __init__(self, scripts_dir: str):
        self.scripts_dir = scripts_dir
        self.error_tracker_script = Path(scripts_dir) / "error_tracker.py"
    
    def search_similar(self, error_message: str) -> dict:
        """搜索相似错误
        
        调用: python error_tracker.py --search "xxx"
        
        Returns:
            dict: 搜索结果
                - found: bool
                - similar_errors: List[dict]
                - suggested_fix: str
        """
        cmd = [
            "python", str(self.error_tracker_script),
            "--search", error_message
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "found": result.returncode == 0,
            "similar_errors": self._parse_errors(result.stdout),
            "output": result.stdout
        }
    
    def record_fix(self, error: str, fix: str) -> bool:
        """记录修复
        
        调用: python error_tracker.py --record --error "xxx" --fix "xxx"
        """
        cmd = [
            "python", str(self.error_tracker_script),
            "--record",
            "--error", error,
            "--fix", fix
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0
```

#### 5. serial_test.py 集成

**用途**: 串口测试

```python
class SerialTestIntegration:
    """serial_test.py 集成封装"""
    
    def __init__(self, scripts_dir: str, port: str):
        self.scripts_dir = scripts_dir
        self.port = port
        self.serial_test_script = Path(scripts_dir) / "serial_test.py"
    
    def run_test(self, test_file: str = None) -> dict:
        """运行串口测试
        
        调用: python serial_test.py --port COM3 --test tests.json
        
        Returns:
            dict: 测试结果
                - success: bool
                - passed: int
                - failed: int
                - details: List[dict]
        """
        cmd = [
            "python", str(self.serial_test_script),
            "--port", self.port
        ]
        if test_file:
            cmd.extend(["--test", test_file])
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "success": result.returncode == 0,
            "output": result.stdout
        }
```

#### 6. serial_debug.py 集成

**用途**: 串口调试和监控

```python
class SerialDebugIntegration:
    """serial_debug.py 集成封装"""
    
    def __init__(self, scripts_dir: str, port: str):
        self.scripts_dir = scripts_dir
        self.port = port
        self.serial_debug_script = Path(scripts_dir) / "serial_debug.py"
    
    def monitor(self, duration: int = 10) -> dict:
        """监控串口输出
        
        调用: python serial_debug.py --port COM3 --proto printf --listen 10
        
        Returns:
            dict: 监控结果
                - output: str
                - errors_detected: List[str]
        """
        cmd = [
            "python", str(self.serial_debug_script),
            "--port", self.port,
            "--proto", "printf",
            "--listen", str(duration)
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=duration+5)
        
        return {
            "output": result.stdout,
            "errors_detected": self._detect_errors(result.stdout)
        }
    
    def send_command(self, command: str) -> dict:
        """发送串口命令
        
        调用: python serial_debug.py --port COM3 --proto text --send "xxx"
        """
        cmd = [
            "python", str(self.serial_debug_script),
            "--port", self.port,
            "--proto", "text",
            "--send", command
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        return {
            "success": result.returncode == 0,
            "response": result.stdout
        }
```

### 工具链封装类

```python
class ToolChain:
    """工具链封装 - 统一调用接口"""
    
    def __init__(self, project_dir: str, port: str = None):
        self.project_dir = Path(project_dir)
        self.port = port
        self.scripts_dir = Path(__file__).parent.parent / "scripts"
        
        # 初始化各工具集成
        self.workflow = WorkflowIntegration(project_dir, self.scripts_dir)
        self.auto_fix = AutoFixIntegration(project_dir, self.scripts_dir)
        self.check_elf = CheckElfIntegration(project_dir, self.scripts_dir)
        self.error_tracker = ErrorTrackerIntegration(self.scripts_dir)
        
        if port:
            self.serial_test = SerialTestIntegration(self.scripts_dir, port)
            self.serial_debug = SerialDebugIntegration(self.scripts_dir, port)
    
    def detect_debugger(self) -> str:
        """检测调试器类型"""
        # 尝试 ST-LINK
        result = subprocess.run(
            ["STM32_Programmer_CLI.exe", "-c", "port=SWD"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            return "ST-LINK"
        
        # 尝试 pyOCD
        result = subprocess.run(
            ["pyocd", "list"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            return "PYOCD"
        
        return "UNKNOWN"
    
    def find_hex_file(self) -> str:
        """查找 HEX 文件"""
        hex_files = list(self.project_dir.rglob("*.hex"))
        if hex_files:
            return str(hex_files[0])
        raise FileNotFoundError("未找到 HEX 文件")
```

### 工具依赖检查

```python
class DependencyChecker:
    """依赖检查器 - 确保所有工具可用"""
    
    REQUIRED_TOOLS = {
        "python": {"check": "python --version", "required": True},
        "git": {"check": "git --version", "required": True},
        "keil": {"check": "UV4.exe -h", "required": True},
    }
    
    OPTIONAL_TOOLS = {
        "stm32_programmer": {"check": "STM32_Programmer_CLI.exe -h", "required": False},
        "pyocd": {"check": "pyocd --version", "required": False},
        "pyserial": {"check": "python -c 'import serial'", "required": False},
    }
    
    def check_all(self) -> dict:
        """检查所有依赖
        
        Returns:
            dict: 检查结果
                - all_required: bool
                - missing_required: List[str]
                - missing_optional: List[str]
        """
        missing_required = []
        missing_optional = []
        
        for name, tool in self.REQUIRED_TOOLS.items():
            if not self._check_tool(tool["check"]):
                missing_required.append(name)
        
        for name, tool in self.OPTIONAL_TOOLS.items():
            if not self._check_tool(tool["check"]):
                missing_optional.append(name)
        
        return {
            "all_required": len(missing_required) == 0,
            "missing_required": missing_required,
            "missing_optional": missing_optional
        }
    
    def _check_tool(self, check_cmd: str) -> bool:
        """检查单个工具"""
        try:
            result = subprocess.run(
                check_cmd, shell=True,
                capture_output=True, text=True, timeout=5
            )
            return result.returncode == 0
        except:
            return False
```

---

## 调试器配置详细说明

### 调试器类型支持

| 调试器 | 型号 | 烧录命令 | 复位命令 | 检测方式 |
|--------|------|---------|---------|---------|
| **ST-LINK** | V2/V3 | `STM32_Programmer_CLI.exe` | `-rst` 参数 | `STM32_Programmer_CLI.exe -c port=SWD` |
| **DAP-Link** | CMSIS-DAP | `pyocd flash` | `pyocd reset` | `pyocd list` |
| **J-Link** | J-Link | `JLinkExe` | `JLinkExe` | `JLinkExe` |
| **Keil** | 内置调试器 | `UV4.exe -f` | Keil 内置 | `.uvprojx` 配置 |

### 调试器检测流程

```
┌─────────────────────────────────────────────────────────────────────┐
│  调试器检测流程                                                      │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  1. 检查配置文件                                                     │
│     - 读取 debug-loop.json 的 debugger.type                          │
│     - 如果不是 "auto"，使用配置的类型                                 │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼ (如果是 auto)
┌─────────────────────────────────────────────────────────────────────┐
│  2. 尝试 ST-LINK                                                     │
│     命令: STM32_Programmer_CLI.exe -c port=SWD                      │
│     成功: 返回 "ST-LINK"                                            │
│     失败: 继续下一步                                                │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  3. 尝试 pyOCD (DAP-Link)                                            │
│     命令: pyocd list                                                │
│     成功: 返回 "PYOCD"                                              │
│     失败: 继续下一步                                                │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  4. 检查 Keil 配置                                                   │
│     检查: .uvprojx 文件中的调试器配置                                 │
│     有配置: 返回 "KEIL"                                             │
│     无配置: 返回 "UNKNOWN"                                          │
└─────────────────────────────────────────────────────────────────────┘
```

### 调试器配置类

```python
class Debugger:
    """调试器封装 - 支持多种调试器"""
    
    def __init__(self, config: dict):
        """初始化调试器
        
        Args:
            config: 调试器配置
                - type: str (auto/stlink/cmsis-dap/jlink/keil)
                - port: str (串口端口)
                - baudrate: int (波特率)
                - reset_method: str (swd/dtr_rts/break)
                - reset_delay_ms: int (复位后等待时间)
        """
        self.config = config
        self.detected_type = None
        self.port = config.get("port")
        self.baudrate = config.get("baudrate", 115200)
        self.reset_method = config.get("reset_method", "swd")
        self.reset_delay_ms = config.get("reset_delay_ms", 200)
    
    def detect(self) -> str:
        """检测调试器类型
        
        Returns:
            str: 调试器类型
        """
        configured_type = self.config.get("type", "auto")
        
        if configured_type != "auto":
            self.detected_type = configured_type
            return configured_type
        
        # 尝试 ST-LINK
        if self._try_stlink():
            self.detected_type = "ST-LINK"
            return "ST-LINK"
        
        # 尝试 pyOCD (DAP-Link)
        if self._try_pyocd():
            self.detected_type = "PYOCD"
            return "PYOCD"
        
        # 尝试 Keil
        if self._try_keil():
            self.detected_type = "KEIL"
            return "KEIL"
        
        self.detected_type = "UNKNOWN"
        return "UNKNOWN"
    
    def flash(self, hex_file: str) -> dict:
        """烧录固件
        
        Args:
            hex_file: HEX 文件路径
            
        Returns:
            dict: 烧录结果
        """
        if self.detected_type is None:
            self.detect()
        
        if self.detected_type == "ST-LINK":
            return self._flash_stlink(hex_file)
        elif self.detected_type == "PYOCD":
            return self._flash_pyocd(hex_file)
        elif self.detected_type == "KEIL":
            return self._flash_keil()
        else:
            return {"success": False, "error": "未检测到调试器"}
    
    def reset(self) -> dict:
        """复位芯片
        
        Returns:
            dict: 复位结果
        """
        if self.detected_type is None:
            self.detect()
        
        if self.detected_type == "ST-LINK":
            return self._reset_stlink()
        elif self.detected_type == "PYOCD":
            return self._reset_pyocd()
        elif self.detected_type == "KEIL":
            return self._reset_keil()
        else:
            return {"success": False, "error": "未检测到调试器"}
    
    def _try_stlink(self) -> bool:
        """尝试 ST-LINK"""
        try:
            result = subprocess.run(
                ["STM32_Programmer_CLI.exe", "-c", "port=SWD"],
                capture_output=True, text=True, timeout=10
            )
            return result.returncode == 0
        except:
            return False
    
    def _try_pyocd(self) -> bool:
        """尝试 pyOCD"""
        try:
            result = subprocess.run(
                ["pyocd", "list"],
                capture_output=True, text=True, timeout=10
            )
            return result.returncode == 0
        except:
            return False
    
    def _try_keil(self) -> bool:
        """尝试 Keil"""
        # 检查 .uvprojx 文件中的调试器配置
        uvprojx_files = list(Path(".").glob("*.uvprojx"))
        return len(uvprojx_files) > 0
    
    def _flash_stlink(self, hex_file: str) -> dict:
        """使用 ST-LINK 烧录"""
        cmd = [
            "STM32_Programmer_CLI.exe",
            "-c", "port=SWD",
            "-w", hex_file,
            "-v",
            "-rst"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        return {
            "success": result.returncode == 0,
            "output": result.stdout,
            "error": result.stderr if result.returncode != 0 else None
        }
    
    def _flash_pyocd(self, hex_file: str) -> dict:
        """使用 pyOCD 烧录"""
        # 先烧录
        flash_cmd = [
            "pyocd", "flash",
            "-t", "STM32F407ZG",
            hex_file
        ]
        flash_result = subprocess.run(flash_cmd, capture_output=True, text=True)
        
        if flash_result.returncode != 0:
            return {
                "success": False,
                "output": flash_result.stdout,
                "error": flash_result.stderr
            }
        
        # 再复位
        reset_result = self._reset_pyocd()
        
        return {
            "success": reset_result["success"],
            "output": flash_result.stdout + "\n" + reset_result.get("output", ""),
            "error": reset_result.get("error")
        }
    
    def _flash_keil(self) -> dict:
        """使用 Keil 烧录"""
        # 查找 .uvprojx 文件
        uvprojx_files = list(Path(".").glob("*.uvprojx"))
        if not uvprojx_files:
            return {"success": False, "error": "未找到 .uvprojx 文件"}
        
        uvprojx_file = uvprojx_files[0]
        
        cmd = [
            "UV4.exe",
            "-f", str(uvprojx_file),
            "-t", "Target 1"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        return {
            "success": result.returncode == 0,
            "output": result.stdout,
            "error": result.stderr if result.returncode != 0 else None
        }
    
    def _reset_stlink(self) -> dict:
        """使用 ST-LINK 复位"""
        cmd = [
            "STM32_Programmer_CLI.exe",
            "-c", "port=SWD",
            "-rst"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        # 等待复位完成
        time.sleep(self.reset_delay_ms / 1000)
        
        return {
            "success": result.returncode == 0,
            "output": result.stdout,
            "error": result.stderr if result.returncode != 0 else None
        }
    
    def _reset_pyocd(self) -> dict:
        """使用 pyOCD 复位"""
        cmd = [
            "pyocd", "reset",
            "-t", "STM32F407ZG"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        # 等待复位完成
        time.sleep(self.reset_delay_ms / 1000)
        
        return {
            "success": result.returncode == 0,
            "output": result.stdout,
            "error": result.stderr if result.returncode != 0 else None
        }
    
    def _reset_keil(self) -> dict:
        """使用 Keil 复位"""
        # Keil 复位需要通过调试会话
        # 这里返回提示信息
        return {
            "success": False,
            "error": "Keil 复位需要手动操作",
            "hint": "请在 Keil 中点击 Reset 按钮"
        }
```

### 调试器配置示例

#### 示例 1: ST-LINK 配置

```json
{
  "debugger": {
    "type": "stlink",
    "port": null,
    "baudrate": 115200,
    "reset_method": "swd",
    "reset_delay_ms": 200,
    "timeout_ms": 5000
  },
  "workflow": {
    "flash_command": "STM32_Programmer_CLI.exe -c port=SWD -w {hex_file} -v -rst"
  }
}
```

#### 示例 2: DAP-Link 配置

```json
{
  "debugger": {
    "type": "cmsis-dap",
    "port": "COM3",
    "baudrate": 115200,
    "reset_method": "swd",
    "reset_delay_ms": 200,
    "timeout_ms": 5000
  },
  "workflow": {
    "flash_command": "pyocd flash -t STM32F407ZG {hex_file}",
    "reset_command": "pyocd reset -t STM32F407ZG"
  }
}
```

#### 示例 3: 自动检测配置

```json
{
  "debugger": {
    "type": "auto",
    "port": "COM3",
    "baudrate": 115200,
    "reset_method": "swd",
    "reset_delay_ms": 200,
    "timeout_ms": 5000
  }
}
```

### 调试器串口监控

```python
class DebuggerSerialMonitor:
    """调试器串口监控"""
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial = None
    
    def start_monitor(self, duration: int = 10) -> dict:
        """开始监控串口
        
        Args:
            duration: 监控时长（秒）
            
        Returns:
            dict: 监控结果
                - output: str
                - hard_fault_detected: bool
                - watchdog_reset_detected: bool
        """
        import serial
        
        self.serial = serial.Serial(self.port, self.baudrate, timeout=1)
        output = []
        hard_fault = False
        watchdog_reset = False
        
        start_time = time.time()
        while time.time() - start_time < duration:
            if self.serial.in_waiting:
                line = self.serial.readline().decode('utf-8', errors='ignore')
                output.append(line)
                
                # 检测 HardFault
                if "Hard Fault" in line or "HARDFAULT" in line:
                    hard_fault = True
                
                # 检测看门狗复位
                if "IWDG Reset" in line or "WWDG Reset" in line:
                    watchdog_reset = True
        
        self.serial.close()
        
        return {
            "output": "\n".join(output),
            "hard_fault_detected": hard_fault,
            "watchdog_reset_detected": watchdog_reset
        }
    
    def send_command(self, command: str) -> str:
        """发送命令并等待响应"""
        import serial
        
        self.serial = serial.Serial(self.port, self.baudrate, timeout=1)
        self.serial.write((command + "\r\n").encode())
        
        time.sleep(0.1)
        response = self.serial.readline().decode('utf-8', errors='ignore')
        
        self.serial.close()
        return response
```

---

## 实施计划

| 阶段 | 内容 | 预计时间 |
|------|------|---------|
| 1 | 创建 debug_loop.py 核心框架 | 2 小时 |
| 2 | 实现 Detector 组件 | 1 小时 |
| 3 | 实现 Analyzer 组件 | 1 小时 |
| 4 | 实现 Fixer 组件 | 2 小时 |
| 5 | 实现 Verifier 组件 | 1 小时 |
| 6 | 实现 Recorder 组件 | 1 小时 |
| 7 | 集成测试与调试 | 2 小时 |
| **总计** | | **10 小时** |

---

*设计日期: 2026-07-28*
*版本: 1.0.0*
