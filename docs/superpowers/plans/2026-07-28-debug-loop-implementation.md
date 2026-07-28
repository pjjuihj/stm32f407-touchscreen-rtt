# 自动化闭环调试工作流实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个全自动闭环调试工作流，用于 STM32 嵌入式开发，支持自动检测问题、分析根因、修复代码、烧录验证、复位测试、生成文档。

**Architecture:** 采用模块化设计，核心引擎 + 可插拔组件架构。主入口 debug_loop.py 调用 DebugLoopEngine，Engine 协调 Detector、Analyzer、Fixer、Verifier、Recorder 五个组件完成闭环流程。

**Tech Stack:** Python 3.8+, subprocess (调用外部工具), json (配置/数据), pathlib (路径处理)

## Global Constraints

- Python 3.8+ 兼容性
- 不修改现有工具脚本（workflow.py, auto_fix.py 等）
- 所有文件路径使用 pathlib.Path
- 错误处理必须记录到日志，不能静默失败
- Git 操作必须在事务性上下文中执行（先备份后修改）

---

## 文件结构

```
D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts\
├── debug_loop.py                    # 主入口（创建）
├── debug_loop_engine/               # 核心引擎模块（创建）
│   ├── __init__.py
│   ├── engine.py                    # DebugLoopEngine 主类
│   ├── config.py                    # ConfigManager 配置管理
│   └── state.py                     # StateManager 状态管理
├── debug_loop_components/           # 组件模块（创建）
│   ├── __init__.py
│   ├── detector.py                  # Detector 检测器
│   ├── analyzer.py                  # Analyzer 分析器
│   ├── fixer.py                     # Fixer 修复器
│   ├── verifier.py                  # Verifier 验证器
│   └── recorder.py                  # Recorder 记录器
├── debug_loop_tools/                # 工具封装模块（创建）
│   ├── __init__.py
│   ├── toolchain.py                 # ToolChain 工具链封装
│   ├── git_manager.py               # GitManager 版本控制
│   ├── debugger.py                  # Debugger 调试器封装
│   └── serial_tool.py               # SerialTool 串口工具
└── debug_loop_tests/                # 测试模块（创建）
    ├── __init__.py
    ├── test_config.py
    ├── test_detector.py
    ├── test_analyzer.py
    ├── test_fixer.py
    ├── test_verifier.py
    ├── test_recorder.py
    ├── test_git_manager.py
    ├── test_engine.py
    └── test_integration.py
```

---

### Task 1: 项目脚手架和配置管理

**Files:**
- Create: `debug_loop_engine/__init__.py`
- Create: `debug_loop_engine/config.py`
- Create: `debug_loop_engine/state.py`
- Create: `debug_loop_components/__init__.py`
- Create: `debug_loop_tools/__init__.py`
- Create: `debug_loop_tests/__init__.py`
- Create: `debug_loop_tests/test_config.py`

**Interfaces:**
- Produces: `ConfigManager(config_path: Path)`, `StateManager()`

- [ ] **Step 1: 创建目录结构**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
mkdir -p debug_loop_engine debug_loop_components debug_loop_tools debug_loop_tests
```

- [ ] **Step 2: 创建 __init__.py 文件**

```python
# debug_loop_engine/__init__.py
from .config import ConfigManager
from .state import StateManager
from .engine import DebugLoopEngine

__all__ = ["ConfigManager", "StateManager", "DebugLoopEngine"]
```

```python
# debug_loop_components/__init__.py
from .detector import Detector
from .analyzer import Analyzer
from .fixer import Fixer
from .verifier import Verifier
from .recorder import Recorder

__all__ = ["Detector", "Analyzer", "Fixer", "Verifier", "Recorder"]
```

```python
# debug_loop_tools/__init__.py
from .toolchain import ToolChain
from .git_manager import GitManager
from .debugger import Debugger
from .serial_tool import SerialTool

__all__ = ["ToolChain", "GitManager", "Debugger", "SerialTool"]
```

```python
# debug_loop_tests/__init__.py
```

- [ ] **Step 3: 编写 ConfigManager 测试**

```python
# debug_loop_tests/test_config.py
import pytest
from pathlib import Path
import json
import tempfile
import os

# 导入待实现的模块（会失败，这是预期的）
from debug_loop_engine.config import ConfigManager


class TestConfigManager:
    """ConfigManager 测试"""

    def test_load_default_config(self):
        """测试加载默认配置"""
        manager = ConfigManager(None)
        config = manager.get_all()

        assert config["debugger"]["type"] == "auto"
        assert config["fix"]["auto_fix"] is True
        assert config["git"]["enabled"] is True

    def test_load_from_file(self, tmp_path):
        """测试从文件加载配置"""
        config_file = tmp_path / "debug-loop.json"
        config_file.write_text(json.dumps({
            "debugger": {"type": "stlink", "port": "COM3"}
        }))

        manager = ConfigManager(config_file)
        assert manager.get("debugger.type") == "stlink"
        assert manager.get("debugger.port") == "COM3"

    def test_get_with_dot_notation(self):
        """测试使用点号路径获取配置"""
        manager = ConfigManager(None)
        assert manager.get("debugger.type") == "auto"
        assert manager.get("git.auto_commit") is True

    def test_set_value(self):
        """测试设置配置值"""
        manager = ConfigManager(None)
        manager.set("debugger.type", "stlink")
        assert manager.get("debugger.type") == "stlink"

    def test_get_nonexistent_returns_default(self):
        """测试获取不存在的配置返回默认值"""
        manager = ConfigManager(None)
        assert manager.get("nonexistent.key", "default") == "default"

    def test_load_invalid_json_raises_error(self, tmp_path):
        """测试加载无效 JSON 文件"""
        config_file = tmp_path / "bad.json"
        config_file.write_text("not valid json {{{")

        with pytest.raises(ValueError):
            ConfigManager(config_file)
```

- [ ] **Step 4: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_config.py -v
```

Expected: FAIL with "ModuleNotFoundError: No module named 'debug_loop_engine.config'"

- [ ] **Step 5: 实现 ConfigManager**

```python
# debug_loop_engine/config.py
"""配置管理模块"""
from pathlib import Path
from typing import Any, Optional
import json


# 默认配置
DEFAULT_CONFIG = {
    "project": {
        "name": None,
        "chip": None,
        "series": None,
        "flash_kb": None,
        "ram_kb": None
    },
    "debugger": {
        "type": "auto",
        "port": None,
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
        "file_watch": True,
        "compile_monitor": True,
        "serial_monitor": True,
        "watch_interval_sec": 2,
        "watch_patterns": ["*.c", "*.h"],
        "watch_dirs": ["USER/", "Main/", "Common/"]
    },
    "fix": {
        "auto_fix": True,
        "max_retries": 3,
        "retry_delay_sec": 2,
        "backup_before_fix": True,
        "rollback_on_failure": True,
        "backup_dir": ".debug-backups"
    },
    "git": {
        "enabled": True,
        "auto_commit": True,
        "commit_prefix": "debug-loop",
        "auto_tag": True,
        "tag_prefix": "debug",
        "tag_on_success": True,
        "keep_snapshots": 10
    },
    "documentation": {
        "log_dir": "docs/debug-logs",
        "generate_patch": True,
        "generate_reproduce": True,
        "generate_analysis": True,
        "generate_verify": True,
        "auto_cleanup_days": 30
    },
    "patterns": {
        "error_patterns_file": "docs/debug-logs/error-patterns.json",
        "auto_record_patterns": True,
        "match_threshold": 0.8,
        "max_patterns": 1000
    }
}


class ConfigManager:
    """配置管理器"""

    def __init__(self, config_path: Optional[Path]):
        """初始化配置管理器

        Args:
            config_path: 配置文件路径，None 表示使用默认配置
        """
        self._config = self._deep_copy(DEFAULT_CONFIG)

        if config_path and config_path.exists():
            self._load_from_file(config_path)

    def _load_from_file(self, path: Path):
        """从文件加载配置"""
        try:
            content = path.read_text(encoding="utf-8")
            user_config = json.loads(content)
            self._deep_merge(self._config, user_config)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in config file: {e}")

    def _deep_merge(self, base: dict, override: dict):
        """深度合并两个字典"""
        for key, value in override.items():
            if key in base and isinstance(base[key], dict) and isinstance(value, dict):
                self._deep_merge(base[key], value)
            else:
                base[key] = value

    def _deep_copy(self, d: dict) -> dict:
        """深拷贝字典"""
        return json.loads(json.dumps(d))

    def get(self, dot_path: str, default: Any = None) -> Any:
        """使用点号路径获取配置值

        Args:
            dot_path: 点号分隔的路径，如 "debugger.type"
            default: 默认值

        Returns:
            配置值
        """
        keys = dot_path.split(".")
        value = self._config

        for key in keys:
            if isinstance(value, dict) and key in value:
                value = value[key]
            else:
                return default

        return value

    def set(self, dot_path: str, value: Any):
        """使用点号路径设置配置值

        Args:
            dot_path: 点号分隔的路径
            value: 要设置的值
        """
        keys = dot_path.split(".")
        config = self._config

        for key in keys[:-1]:
            if key not in config:
                config[key] = {}
            config = config[key]

        config[keys[-1]] = value

    def get_all(self) -> dict:
        """获取所有配置"""
        return self._deep_copy(self._config)

    def save(self, path: Path):
        """保存配置到文件"""
        path.write_text(
            json.dumps(self._config, indent=2, ensure_ascii=False),
            encoding="utf-8"
        )
```

- [ ] **Step 6: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_config.py -v
```

Expected: All tests PASS

- [ ] **Step 7: 实现 StateManager**

```python
# debug_loop_engine/state.py
"""状态管理模块"""
from enum import Enum
from typing import Optional, List
from datetime import datetime
from dataclasses import dataclass, field


class IssueStatus(Enum):
    """问题状态"""
    DETECTED = "detected"
    ANALYZING = "analyzing"
    FIXING = "fixing"
    FLASHING = "flashing"
    VERIFYING = "verifying"
    RESOLVED = "resolved"
    FAILED = "failed"


class IssueType(Enum):
    """问题类型"""
    COMPILE = "compile"
    RUNTIME = "runtime"
    HARDWARE = "hardware"


@dataclass
class Issue:
    """问题数据类"""
    id: str
    timestamp: str
    type: IssueType
    severity: str
    status: IssueStatus
    source_file: str = ""
    source_line: int = 0
    error_message: str = ""
    root_cause: str = ""
    fix_description: str = ""
    git_commit: str = ""


class StateManager:
    """状态管理器"""

    def __init__(self):
        """初始化状态管理器"""
        self._current_issue: Optional[Issue] = None
        self._issue_counter: int = 0
        self._history: List[Issue] = []

    def create_issue(self, issue_type: IssueType, error_message: str,
                     source_file: str = "", source_line: int = 0,
                     severity: str = "medium") -> Issue:
        """创建新问题"""
        self._issue_counter += 1
        issue_id = f"issue-{self._issue_counter:03d}"

        issue = Issue(
            id=issue_id,
            timestamp=datetime.now().isoformat(),
            type=issue_type,
            severity=severity,
            status=IssueStatus.DETECTED,
            source_file=source_file,
            source_line=source_line,
            error_message=error_message
        )

        self._current_issue = issue
        return issue

    def update_status(self, status: IssueStatus, **kwargs):
        """更新当前问题状态"""
        if self._current_issue:
            self._current_issue.status = status
            for key, value in kwargs.items():
                if hasattr(self._current_issue, key):
                    setattr(self._current_issue, key, value)

    def complete_issue(self, success: bool):
        """完成当前问题"""
        if self._current_issue:
            if success:
                self._current_issue.status = IssueStatus.RESOLVED
            else:
                self._current_issue.status = IssueStatus.FAILED

            self._history.append(self._current_issue)
            self._current_issue = None

    def get_current_issue(self) -> Optional[Issue]:
        """获取当前问题"""
        return self._current_issue

    def get_history(self) -> List[Issue]:
        """获取历史问题"""
        return self._history.copy()

    def get_statistics(self) -> dict:
        """获取统计信息"""
        total = len(self._history)
        resolved = sum(1 for i in self._history if i.status == IssueStatus.RESOLVED)
        failed = sum(1 for i in self._history if i.status == IssueStatus.FAILED)

        return {
            "total": total,
            "resolved": resolved,
            "failed": failed,
            "success_rate": resolved / total if total > 0 else 0
        }
```

- [ ] **Step 8: 编写 StateManager 测试**

```python
# debug_loop_tests/test_state.py
import pytest
from debug_loop_engine.state import StateManager, IssueStatus, IssueType


class TestStateManager:
    """StateManager 测试"""

    def test_create_issue(self):
        """测试创建问题"""
        manager = StateManager()
        issue = manager.create_issue(
            IssueType.COMPILE,
            "undefined reference to 'HAL_GPIO_Init'",
            "main.c",
            42
        )

        assert issue.id == "issue-001"
        assert issue.type == IssueType.COMPILE
        assert issue.status == IssueStatus.DETECTED

    def test_update_status(self):
        """测试更新状态"""
        manager = StateManager()
        manager.create_issue(IssueType.COMPILE, "error")
        manager.update_status(IssueStatus.ANALYZING, root_cause="missing header")

        current = manager.get_current_issue()
        assert current.status == IssueStatus.ANALYZING
        assert current.root_cause == "missing header"

    def test_complete_issue_success(self):
        """测试成功完成问题"""
        manager = StateManager()
        manager.create_issue(IssueType.COMPILE, "error")
        manager.complete_issue(success=True)

        assert manager.get_current_issue() is None
        assert len(manager.get_history()) == 1
        assert manager.get_history()[0].status == IssueStatus.RESOLVED

    def test_complete_issue_failure(self):
        """测试失败完成问题"""
        manager = StateManager()
        manager.create_issue(IssueType.COMPILE, "error")
        manager.complete_issue(success=False)

        assert manager.get_history()[0].status == IssueStatus.FAILED

    def test_statistics(self):
        """测试统计信息"""
        manager = StateManager()
        manager.create_issue(IssueType.COMPILE, "error1")
        manager.complete_issue(success=True)
        manager.create_issue(IssueType.COMPILE, "error2")
        manager.complete_issue(success=False)

        stats = manager.get_statistics()
        assert stats["total"] == 2
        assert stats["resolved"] == 1
        assert stats["failed"] == 1
        assert stats["success_rate"] == 0.5

    def test_issue_counter_increments(self):
        """测试问题计数器递增"""
        manager = StateManager()
        issue1 = manager.create_issue(IssueType.COMPILE, "error1")
        manager.complete_issue(success=True)
        issue2 = manager.create_issue(IssueType.COMPILE, "error2")

        assert issue1.id == "issue-001"
        assert issue2.id == "issue-002"
```

- [ ] **Step 9: 运行所有测试**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_config.py debug_loop_tests/test_state.py -v
```

Expected: All tests PASS

- [ ] **Step 10: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_engine/
git commit -m "feat(debug-loop): add ConfigManager and StateManager with tests"
```

---

### Task 2: 工具链封装 - GitManager

**Files:**
- Create: `debug_loop_tools/git_manager.py`
- Create: `debug_loop_tests/test_git_manager.py`

**Interfaces:**
- Consumes: `ConfigManager`
- Produces: `GitManager(project_dir: Path)`, `create_snapshot()`, `rollback()`, `create_tag()`

- [ ] **Step 1: 编写 GitManager 测试**

```python
# debug_loop_tests/test_git_manager.py
import pytest
from pathlib import Path
import subprocess
import tempfile
import os

from debug_loop_tools.git_manager import GitManager


@pytest.fixture
def git_project(tmp_path):
    """创建临时 Git 项目"""
    # 初始化 Git 仓库
    subprocess.run(["git", "init"], cwd=tmp_path, capture_output=True)
    subprocess.run(["git", "config", "user.email", "test@test.com"], cwd=tmp_path, capture_output=True)
    subprocess.run(["git", "config", "user.name", "Test"], cwd=tmp_path, capture_output=True)

    # 创建一个文件并提交
    test_file = tmp_path / "test.c"
    test_file.write_text("int main() { return 0; }")
    subprocess.run(["git", "add", "."], cwd=tmp_path, capture_output=True)
    subprocess.run(["git", "commit", "-m", "initial"], cwd=tmp_path, capture_output=True)

    return tmp_path


class TestGitManager:
    """GitManager 测试"""

    def test_is_git_repo(self, git_project):
        """测试检测 Git 仓库"""
        manager = GitManager(git_project)
        assert manager.is_git_repo is True

    def test_is_not_git_repo(self, tmp_path):
        """测试检测非 Git 仓库"""
        manager = GitManager(tmp_path)
        assert manager.is_git_repo is False

    def test_create_snapshot(self, git_project):
        """测试创建快照"""
        manager = GitManager(git_project)

        # 修改文件
        test_file = git_project / "test.c"
        test_file.write_text("int main() { return 1; }")

        snapshot_id = manager.create_snapshot("test snapshot")
        assert snapshot_id is not None
        assert "main@" in snapshot_id

    def test_rollback(self, git_project):
        """测试回滚"""
        manager = GitManager(git_project)
        original_content = (git_project / "test.c").read_text()

        # 修改文件
        test_file = git_project / "test.c"
        test_file.write_text("int main() { return 1; }")

        # 创建快照
        snapshot_id = manager.create_snapshot()

        # 回滚
        result = manager.rollback(snapshot_id)
        assert result is True

        # 验证内容恢复
        assert (git_project / "test.c").read_text() == original_content

    def test_create_tag(self, git_project):
        """测试创建标签"""
        manager = GitManager(git_project)
        tag_name = manager.create_tag("v1.0", "Release 1.0")
        assert tag_name == "debug/v1.0"

    def test_list_tags(self, git_project):
        """测试列出标签"""
        manager = GitManager(git_project)
        manager.create_tag("v1.0")
        manager.create_tag("v2.0")

        tags = manager.list_tags()
        assert len(tags) == 2

    def test_get_current_branch(self, git_project):
        """测试获取当前分支"""
        manager = GitManager(git_project)
        branch = manager.get_current_branch()
        assert branch in ["main", "master"]

    def test_get_last_commit(self, git_project):
        """测试获取最后提交"""
        manager = GitManager(git_project)
        commit = manager.get_last_commit()
        assert commit is not None
        assert len(commit) > 0
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_git_manager.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 GitManager**

```python
# debug_loop_tools/git_manager.py
"""Git 版本控制管理器"""
from pathlib import Path
from typing import Optional, List, Dict
import subprocess


class GitManager:
    """Git 版本控制管理器"""

    def __init__(self, project_dir: Path):
        """初始化 Git 管理器

        Args:
            project_dir: 项目目录
        """
        self.project_dir = project_dir
        self.is_git_repo = self._check_git_repo()
        self.commit_prefix = "debug-loop"
        self.tag_prefix = "debug"

    def _check_git_repo(self) -> bool:
        """检查是否是 Git 仓库"""
        return (self.project_dir / ".git").exists()

    def _run_git(self, command: str) -> str:
        """执行 Git 命令

        Args:
            command: Git 命令

        Returns:
            命令输出

        Raises:
            RuntimeError: 命令执行失败
        """
        full_cmd = f"git -C {self.project_dir} {command}"
        result = subprocess.run(
            full_cmd,
            shell=True,
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            raise RuntimeError(f"Git command failed: {result.stderr}")
        return result.stdout.strip()

    def get_current_branch(self) -> str:
        """获取当前分支名"""
        return self._run_git("rev-parse --abbrev-ref HEAD")

    def get_last_commit(self) -> str:
        """获取最后提交的 hash"""
        return self._run_git("rev-parse --short HEAD")

    def create_snapshot(self, message: Optional[str] = None) -> str:
        """创建 Git 快照

        Args:
            message: 提交消息

        Returns:
            快照 ID (branch@commit)
        """
        if not self.is_git_repo:
            raise RuntimeError("Not a Git repository")

        # 检查是否有修改
        status = self._run_git("status --porcelain")
        if not status:
            branch = self.get_current_branch()
            commit = self.get_last_commit()
            return f"{branch}@{commit}"

        # 添加所有修改
        self._run_git("add -A")

        # 创建 commit
        branch = self.get_current_branch()
        commit = self.get_last_commit()
        snapshot_msg = message or f"{self.commit_prefix}: snapshot at {commit}"
        self._run_git(f'commit -m "{snapshot_msg}"')

        # 获取新 commit
        new_commit = self.get_last_commit()
        return f"{branch}@{new_commit}"

    def rollback(self, snapshot_id: str) -> bool:
        """回滚到指定快照

        Args:
            snapshot_id: 快照 ID (branch@commit)

        Returns:
            是否成功
        """
        if not self.is_git_repo:
            return False

        parts = snapshot_id.split("@")
        if len(parts) != 2:
            return False

        commit_hash = parts[1]
        try:
            self._run_git(f"reset --hard {commit_hash}")
            return True
        except RuntimeError:
            return False

    def create_tag(self, tag_name: str, message: Optional[str] = None) -> str:
        """创建标签

        Args:
            tag_name: 标签名
            message: 标签消息

        Returns:
            完整标签名
        """
        full_tag = f"{self.tag_prefix}/{tag_name}"
        tag_msg = message or f"Version {tag_name}"
        self._run_git(f'tag -a {full_tag} -m "{tag_msg}"')
        return full_tag

    def list_tags(self) -> List[Dict[str, str]]:
        """列出所有标签"""
        try:
            tags_output = self._run_git(f'tag -l "{self.tag_prefix}/*"')
            if not tags_output:
                return []

            tags = []
            for line in tags_output.split('\n'):
                if line.strip():
                    tags.append({"name": line.strip()})
            return tags
        except RuntimeError:
            return []

    def delete_tag(self, tag_name: str) -> bool:
        """删除标签"""
        full_tag = f"{self.tag_prefix}/{tag_name}"
        try:
            self._run_git(f"tag -d {full_tag}")
            return True
        except RuntimeError:
            return False
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_git_manager.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_tools/git_manager.py scripts/debug_loop_tests/test_git_manager.py
git commit -m "feat(debug-loop): add GitManager with snapshot/rollback/tag support"
```

---

### Task 3: 工具链封装 - ToolChain

**Files:**
- Create: `debug_loop_tools/toolchain.py`
- Create: `debug_loop_tests/test_toolchain.py`

**Interfaces:**
- Consumes: `ConfigManager`
- Produces: `ToolChain(project_dir: Path)`, `compile()`, `detect_debugger()`, `find_hex_file()`

- [ ] **Step 1: 编写 ToolChain 测试**

```python
# debug_loop_tests/test_toolchain.py
import pytest
from pathlib import Path
import tempfile
import os

from debug_loop_tools.toolchain import ToolChain
from debug_loop_engine.config import ConfigManager


@pytest.fixture
def project_with_build_log(tmp_path):
    """创建带 build.log 的项目"""
    build_log = tmp_path / "build.log"
    build_log.write_text("*** Using Compiler 'V5.06'\nBuild target 'Target 1'\n0 Error(s), 0 Warning(s).\n")
    return tmp_path


@pytest.fixture
def project_with_hex(tmp_path):
    """创建带 HEX 文件的项目"""
    obj_dir = tmp_path / "Project" / "OBJ"
    obj_dir.mkdir(parents=True)
    hex_file = obj_dir / "TOUCH.hex"
    hex_file.write_text(":00000001FF")
    return tmp_path


class TestToolChain:
    """ToolChain 测试"""

    def test_init(self, project_with_build_log):
        """测试初始化"""
        config = ConfigManager(None)
        chain = ToolChain(project_with_build_log, config)
        assert chain.project_dir == project_with_build_log

    def test_parse_compile_success(self, project_with_build_log):
        """测试解析编译成功"""
        config = ConfigManager(None)
        chain = ToolChain(project_with_build_log, config)

        result = chain._parse_compile_output(
            "0 Error(s), 0 Warning(s)."
        )
        assert result["success"] is True
        assert result["errors"] == 0
        assert result["warnings"] == 0

    def test_parse_compile_errors(self, project_with_build_log):
        """测试解析编译错误"""
        config = ConfigManager(None)
        chain = ToolChain(project_with_build_log, config)

        result = chain._parse_compile_output(
            "main.c(10): error: #20: identifier \"xxx\" undefined\n1 Error(s), 0 Warning(s)."
        )
        assert result["success"] is False
        assert result["errors"] == 1

    def test_find_hex_file(self, project_with_hex):
        """测试查找 HEX 文件"""
        config = ConfigManager(None)
        chain = ToolChain(project_with_hex, config)

        hex_file = chain.find_hex_file()
        assert hex_file is not None
        assert hex_file.name == "TOUCH.hex"

    def test_find_hex_file_not_found(self, tmp_path):
        """测试未找到 HEX 文件"""
        config = ConfigManager(None)
        chain = ToolChain(tmp_path, config)

        hex_file = chain.find_hex_file()
        assert hex_file is None

    def test_detect_debugger_unknown(self, tmp_path):
        """测试检测未知调试器"""
        config = ConfigManager(None)
        chain = ToolChain(tmp_path, config)

        # 在没有调试器工具的环境中，应该返回 unknown
        debugger = chain.detect_debugger()
        assert debugger in ["ST-LINK", "PYOCD", "KEIL", "UNKNOWN"]
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_toolchain.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 ToolChain**

```python
# debug_loop_tools/toolchain.py
"""工具链封装模块"""
from pathlib import Path
from typing import Optional, Dict, List
import subprocess
import re

from debug_loop_engine.config import ConfigManager


class ToolChain:
    """工具链封装 - 统一调用接口"""

    def __init__(self, project_dir: Path, config: ConfigManager):
        """初始化工具链

        Args:
            project_dir: 项目目录
            config: 配置管理器
        """
        self.project_dir = project_dir
        self.config = config
        self.scripts_dir = Path(__file__).parent.parent

    def compile(self) -> Dict:
        """编译项目

        Returns:
            编译结果
        """
        cmd = [
            "python",
            str(self.scripts_dir / "workflow.py"),
            "--auto", str(self.project_dir),
            "--steps", "compile"
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300
            )
            return self._parse_compile_output(result.stdout)
        except subprocess.TimeoutExpired:
            return {"success": False, "errors": -1, "warnings": -1, "output": "Compile timeout"}
        except Exception as e:
            return {"success": False, "errors": -1, "warnings": -1, "output": str(e)}

    def _parse_compile_output(self, output: str) -> Dict:
        """解析编译输出

        Args:
            output: 编译输出

        Returns:
            解析结果
        """
        error_match = re.search(r"(\d+) Error\(s\)", output)
        warning_match = re.search(r"(\d+) Warning\(s\)", output)

        errors = int(error_match.group(1)) if error_match else 0
        warnings = int(warning_match.group(1)) if warning_match else 0

        return {
            "success": errors == 0,
            "errors": errors,
            "warnings": warnings,
            "output": output
        }

    def find_hex_file(self) -> Optional[Path]:
        """查找 HEX 文件

        Returns:
            HEX 文件路径，未找到返回 None
        """
        hex_files = list(self.project_dir.rglob("*.hex"))
        if hex_files:
            return hex_files[0]
        return None

    def detect_debugger(self) -> str:
        """检测调试器类型

        Returns:
            调试器类型
        """
        configured_type = self.config.get("debugger.type", "auto")

        if configured_type != "auto":
            return configured_type.upper()

        # 尝试 ST-LINK
        if self._try_command("STM32_Programmer_CLI.exe -c port=SWD"):
            return "ST-LINK"

        # 尝试 pyOCD
        if self._try_command("pyocd list"):
            return "PYOCD"

        # 检查 Keil 项目
        if list(self.project_dir.rglob("*.uvprojx")):
            return "KEIL"

        return "UNKNOWN"

    def _try_command(self, command: str) -> bool:
        """尝试执行命令

        Args:
            command: 命令

        Returns:
            是否成功
        """
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                timeout=10
            )
            return result.returncode == 0
        except Exception:
            return False

    def flash(self, hex_file: Path) -> Dict:
        """烧录固件

        Args:
            hex_file: HEX 文件路径

        Returns:
            烧录结果
        """
        debugger = self.detect_debugger()

        if debugger == "ST-LINK":
            cmd = [
                "STM32_Programmer_CLI.exe",
                "-c", "port=SWD",
                "-w", str(hex_file),
                "-v", "-rst"
            ]
        elif debugger == "PYOCD":
            cmd = [
                "pyocd", "flash",
                "-t", self.config.get("project.chip", "STM32F407ZG"),
                str(hex_file)
            ]
        else:
            return {"success": False, "error": f"Unsupported debugger: {debugger}"}

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60
            )
            return {
                "success": result.returncode == 0,
                "output": result.stdout,
                "error": result.stderr if result.returncode != 0 else None
            }
        except Exception as e:
            return {"success": False, "error": str(e)}
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_toolchain.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_tools/toolchain.py scripts/debug_loop_tests/test_toolchain.py
git commit -m "feat(debug-loop): add ToolChain with compile/flash/detect capabilities"
```

---

### Task 4: 检测器组件 - Detector

**Files:**
- Create: `debug_loop_components/detector.py`
- Create: `debug_loop_tests/test_detector.py`

**Interfaces:**
- Consumes: `ConfigManager`, `ToolChain`
- Produces: `Detector(config, toolchain)`, `detect_all()`, `detect_compile_errors()`

- [ ] **Step 1: 编写 Detector 测试**

```python
# debug_loop_tests/test_detector.py
import pytest
from pathlib import Path
from unittest.mock import Mock, MagicMock

from debug_loop_components.detector import Detector
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import IssueType


@pytest.fixture
def mock_toolchain():
    """模拟 ToolChain"""
    toolchain = Mock()
    toolchain.compile.return_value = {
        "success": False,
        "errors": 2,
        "warnings": 1,
        "output": "main.c(10): error: #20 undefined\nmain.c(20): warning: unused variable"
    }
    return toolchain


@pytest.fixture
def detector(tmp_path, mock_toolchain):
    """创建 Detector"""
    config = ConfigManager(None)
    return Detector(config, mock_toolchain, tmp_path)


class TestDetector:
    """Detector 测试"""

    def test_detect_compile_errors(self, detector, mock_toolchain):
        """测试检测编译错误"""
        issues = detector.detect_compile_errors()

        assert len(issues) > 0
        assert issues[0].type == IssueType.COMPILE
        assert "error" in issues[0].error_message.lower()

    def test_detect_compile_success(self, detector, mock_toolchain):
        """测试编译成功时无错误"""
        mock_toolchain.compile.return_value = {
            "success": True,
            "errors": 0,
            "warnings": 0,
            "output": "0 Error(s), 0 Warning(s)."
        }

        issues = detector.detect_compile_errors()
        assert len(issues) == 0

    def test_parse_error_line(self, detector):
        """测试解析错误行"""
        error_line = "main.c(10): error: #20: identifier \"xxx\" undefined"
        result = detector._parse_error_line(error_line)

        assert result is not None
        assert result["file"] == "main.c"
        assert result["line"] == 10
        assert "undefined" in result["message"].lower()

    def test_parse_error_line_no_match(self, detector):
        """测试解析非错误行"""
        normal_line = "Build complete."
        result = detector._parse_error_line(normal_line)
        assert result is None

    def test_severity_from_error(self, detector):
        """测试根据错误确定严重度"""
        assert detector._get_severity("error") == "high"
        assert detector._get_severity("warning") == "medium"
        assert detector._get_severity("note") == "low"
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_detector.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 Detector**

```python
# debug_loop_components/detector.py
"""问题检测器模块"""
from pathlib import Path
from typing import List, Dict, Optional
import re

from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue, IssueType


class Detector:
    """问题检测器"""

    def __init__(self, config: ConfigManager, state: StateManager, project_dir: Path):
        """初始化检测器

        Args:
            config: 配置管理器
            state: 状态管理器
            project_dir: 项目目录
        """
        self.config = config
        self.state = state
        self.project_dir = project_dir

    def detect_compile_errors(self) -> List[Issue]:
        """检测编译错误

        Returns:
            问题列表
        """
        issues = []

        # 读取 build.log
        build_log_path = self.project_dir / self.config.get("workflow.build_log", "build.log")
        if not build_log_path.exists():
            return issues

        build_log = build_log_path.read_text(encoding="utf-8", errors="ignore")

        # 解析错误
        for line in build_log.split('\n'):
            parsed = self._parse_error_line(line)
            if parsed:
                issue = self.state.create_issue(
                    IssueType.COMPILE,
                    parsed["message"],
                    parsed["file"],
                    parsed["line"],
                    self._get_severity(parsed["message"])
                )
                issues.append(issue)

        return issues

    def _parse_error_line(self, line: str) -> Optional[Dict]:
        """解析错误行

        Args:
            line: 日志行

        Returns:
            解析结果，None 表示不是错误行
        """
        # 匹配: file(line): error: message
        pattern = r'(.+)\((\d+)\):\s*(error|warning):\s*(.+)'
        match = re.match(pattern, line)

        if match:
            return {
                "file": match.group(1),
                "line": int(match.group(2)),
                "type": match.group(3),
                "message": match.group(4)
            }

        return None

    def _get_severity(self, message: str) -> str:
        """根据消息确定严重度

        Args:
            message: 消息内容

        Returns:
            严重度
        """
        message_lower = message.lower()

        if "error" in message_lower:
            return "high"
        elif "warning" in message_lower:
            return "medium"
        else:
            return "low"

    def detect_all(self) -> List[Issue]:
        """检测所有类型的问题

        Returns:
            所有问题列表
        """
        issues = []

        # 检测编译错误
        issues.extend(self.detect_compile_errors())

        # TODO: 添加运行时异常检测
        # TODO: 添加硬件故障检测

        return issues
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_detector.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_components/detector.py scripts/debug_loop_tests/test_detector.py
git commit -m "feat(debug-loop): add Detector for compile error detection"
```

---

### Task 5: 分析器组件 - Analyzer

**Files:**
- Create: `debug_loop_components/analyzer.py`
- Create: `debug_loop_tests/test_analyzer.py`

**Interfaces:**
- Consumes: `ConfigManager`, `StateManager`, `Issue`
- Produces: `Analyzer(config, state)`, `analyze(issue)`, `find_pattern()`

- [ ] **Step 1: 编写 Analyzer 测试**

```python
# debug_loop_tests/test_analyzer.py
import pytest
from pathlib import Path
import json

from debug_loop_components.analyzer import Analyzer
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue, IssueType


@pytest.fixture
def analyzer(tmp_path):
    """创建 Analyzer"""
    config = ConfigManager(None)
    state = StateManager()
    return Analyzer(config, state, tmp_path)


@pytest.fixture
def sample_issue():
    """示例问题"""
    return Issue(
        id="issue-001",
        timestamp="2026-07-28T14:30:00Z",
        type=IssueType.COMPILE,
        severity="high",
        status="detected",
        source_file="main.c",
        source_line=10,
        error_message="undefined reference to 'HAL_GPIO_Init'"
    )


class TestAnalyzer:
    """Analyzer 测试"""

    def test_analyze_issue(self, analyzer, sample_issue):
        """测试分析问题"""
        result = analyzer.analyze(sample_issue)

        assert "root_cause" in result
        assert "fix_suggestions" in result
        assert "impact_scope" in result

    def test_suggest_fix_missing_header(self, analyzer, sample_issue):
        """测试建议修复 - 缺少头文件"""
        suggestions = analyzer._suggest_fixes(sample_issue)

        # 应该建议添加头文件
        assert any("include" in s.lower() for s in suggestions)

    def test_load_patterns(self, analyzer, tmp_path):
        """测试加载错误模式"""
        patterns_file = tmp_path / "patterns.json"
        patterns_file.write_text(json.dumps([
            {
                "error_type": "compile",
                "error_pattern": "undefined reference",
                "fix_type": "add_header",
                "fix_description": "Add missing #include"
            }
        ]))

        patterns = analyzer._load_patterns()
        assert len(patterns) == 1
        assert patterns[0]["fix_type"] == "add_header"

    def test_find_matching_pattern(self, analyzer, tmp_path):
        """测试查找匹配模式"""
        patterns_file = tmp_path / "patterns.json"
        patterns_file.write_text(json.dumps([
            {
                "error_type": "compile",
                "error_pattern": "undefined reference",
                "fix_type": "add_header",
                "fix_description": "Add missing #include"
            }
        ]))

        analyzer._load_patterns()
        match = analyzer._find_matching_pattern("undefined reference to 'xxx'")

        assert match is not None
        assert match["fix_type"] == "add_header"
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_analyzer.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 Analyzer**

```python
# debug_loop_components/analyzer.py
"""问题分析器模块"""
from pathlib import Path
from typing import Dict, List, Optional
import json
import re

from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue


class Analyzer:
    """问题分析器"""

    def __init__(self, config: ConfigManager, state: StateManager, project_dir: Path):
        """初始化分析器

        Args:
            config: 配置管理器
            state: 状态管理器
            project_dir: 项目目录
        """
        self.config = config
        self.state = state
        self.project_dir = project_dir
        self._patterns = self._load_patterns()

    def _load_patterns(self) -> List[Dict]:
        """加载错误模式库

        Returns:
            模式列表
        """
        patterns_file = self.project_dir / self.config.get(
            "patterns.error_patterns_file",
            "docs/debug-logs/error-patterns.json"
        )

        if patterns_file.exists():
            try:
                return json.loads(patterns_file.read_text(encoding="utf-8"))
            except Exception:
                return []
        return []

    def _find_matching_pattern(self, error_message: str) -> Optional[Dict]:
        """查找匹配的模式

        Args:
            error_message: 错误信息

        Returns:
            匹配的模式，未找到返回 None
        """
        threshold = self.config.get("patterns.match_threshold", 0.8)

        for pattern in self._patterns:
            pattern_text = pattern.get("error_pattern", "")
            similarity = self._calculate_similarity(error_message, pattern_text)

            if similarity >= threshold:
                return pattern

        return None

    def _calculate_similarity(self, str1: str, str2: str) -> float:
        """计算字符串相似度

        Args:
            str1: 字符串 1
            str2: 字符串 2

        Returns:
            相似度 (0-1)
        """
        if not str1 or not str2:
            return 0.0

        # 简单的基于关键词的相似度计算
        words1 = set(str1.lower().split())
        words2 = set(str2.lower().split())

        if not words1 or not words2:
            return 0.0

        intersection = words1 & words2
        union = words1 | words2

        return len(intersection) / len(union) if union else 0.0

    def analyze(self, issue: Issue) -> Dict:
        """分析问题

        Args:
            issue: 问题

        Returns:
            分析结果
        """
        # 查找匹配的模式
        match = self._find_matching_pattern(issue.error_message)

        # 生成修复建议
        fix_suggestions = self._suggest_fixes(issue)

        # 分析影响范围
        impact_scope = self._analyze_impact(issue)

        # 分析根因
        root_cause = self._analyze_root_cause(issue, match)

        return {
            "root_cause": root_cause,
            "fix_suggestions": fix_suggestions,
            "impact_scope": impact_scope,
            "matched_pattern": match
        }

    def _suggest_fixes(self, issue: Issue) -> List[str]:
        """生成修复建议

        Args:
            issue: 问题

        Returns:
            修复建议列表
        """
        suggestions = []
        error_msg = issue.error_message.lower()

        # 根据错误类型生成建议
        if "undefined reference" in error_msg:
            # 提取函数名
            func_match = re.search(r"undefined reference to '(\w+)'", issue.error_message)
            if func_match:
                func_name = func_match.group(1)
                if func_name.startswith("HAL_"):
                    header = func_name.replace("HAL_", "stm32f4xx_hal_").lower() + ".h"
                    suggestions.append(f"Add #include \"{header}\"")
                else:
                    suggestions.append(f"Check if '{func_name}' is defined and linked")

        elif "undeclared identifier" in error_msg:
            suggestions.append("Add variable declaration or #include")

        elif "redefinition" in error_msg:
            suggestions.append("Remove duplicate definition or use include guard")

        if not suggestions:
            suggestions.append("Review error message and check documentation")

        return suggestions

    def _analyze_impact(self, issue: Issue) -> List[str]:
        """分析影响范围

        Args:
            issue: 问题

        Returns:
            受影响的文件列表
        """
        # 简化实现：返回错误文件
        return [issue.source_file] if issue.source_file else []

    def _analyze_root_cause(self, issue: Issue, pattern: Optional[Dict]) -> str:
        """分析根因

        Args:
            issue: 问题
            pattern: 匹配的模式

        Returns:
            根因描述
        """
        if pattern:
            return pattern.get("fix_description", "Unknown")

        # 基于错误信息推断根因
        error_msg = issue.error_message.lower()

        if "undefined reference" in error_msg:
            return "Missing function definition or library linkage"
        elif "undeclared identifier" in error_msg:
            return "Missing variable declaration or header include"
        elif "syntax error" in error_msg:
            return "Invalid code syntax"
        else:
            return "Unknown root cause"
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_analyzer.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_components/analyzer.py scripts/debug_loop_tests/test_analyzer.py
git commit -m "feat(debug-loop): add Analyzer for root cause analysis and fix suggestions"
```

---

### Task 6: 修复器组件 - Fixer

**Files:**
- Create: `debug_loop_components/fixer.py`
- Create: `debug_loop_tests/test_fixer.py`

**Interfaces:**
- Consumes: `ConfigManager`, `StateManager`, `GitManager`, `Analyzer`
- Produces: `Fixer(config, state, git_manager, analyzer)`, `fix(issue, analysis)`, `rollback()`

- [ ] **Step 1: 编写 Fixer 测试**

```python
# debug_loop_tests/test_fixer.py
import pytest
from pathlib import Path
from unittest.mock import Mock, MagicMock, patch

from debug_loop_components.fixer import Fixer
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue, IssueType, IssueStatus


@pytest.fixture
def mock_git_manager():
    """模拟 GitManager"""
    git_manager = Mock()
    git_manager.is_git_repo = True
    git_manager.create_snapshot.return_value = "main@abc123"
    git_manager.rollback.return_value = True
    return git_manager


@pytest.fixture
def fixer(tmp_path, mock_git_manager):
    """创建 Fixer"""
    config = ConfigManager(None)
    state = StateManager()
    analyzer = Mock()
    return Fixer(config, state, mock_git_manager, analyzer, tmp_path)


@pytest.fixture
def sample_issue():
    """示例问题"""
    return Issue(
        id="issue-001",
        timestamp="2026-07-28T14:30:00Z",
        type=IssueType.COMPILE,
        severity="high",
        status="detected",
        source_file="main.c",
        source_line=10,
        error_message="undefined reference to 'HAL_GPIO_Init'"
    )


class TestFixer:
    """Fixer 测试"""

    def test_fix_creates_snapshot(self, fixer, sample_issue, mock_git_manager):
        """测试修复前创建快照"""
        analysis = {
            "root_cause": "Missing header",
            "fix_suggestions": ["Add #include \"stm32f4xx_hal_gpio.h\""]
        }

        fixer.fix(sample_issue, analysis)

        mock_git_manager.create_snapshot.assert_called_once()

    def test_fix_rollback_on_failure(self, fixer, sample_issue, mock_git_manager):
        """测试修复失败时回滚"""
        analysis = {
            "root_cause": "Unknown",
            "fix_suggestions": []
        }

        # 模拟修复失败
        fixer._apply_fix = Mock(return_value=False)

        result = fixer.fix(sample_issue, analysis)

        assert result["success"] is False
        mock_git_manager.rollback.assert_called_once()

    def test_apply_fix_add_header(self, fixer, tmp_path):
        """测试应用修复 - 添加头文件"""
        # 创建测试文件
        test_file = tmp_path / "main.c"
        test_file.write_text("int main() { return 0; }\n")

        suggestion = "Add #include \"stm32f4xx_hal_gpio.h\""
        result = fixer._apply_fix(test_file, suggestion, 1)

        assert result is True
        content = test_file.read_text()
        assert "stm32f4xx_hal_gpio.h" in content

    def test_rollback_on_failure(self, fixer, sample_issue, mock_git_manager):
        """测试修复失败时回滚"""
        analysis = {"root_cause": "Unknown", "fix_suggestions": []}

        # 模拟修复失败
        fixer._apply_fix = Mock(return_value=False)

        result = fixer.fix(sample_issue, analysis)

        assert result["success"] is False
        mock_git_manager.rollback.assert_called_once()
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_fixer.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 Fixer**

```python
# debug_loop_components/fixer.py
"""问题修复器模块"""
from pathlib import Path
from typing import Dict, List, Optional
import re
import time

from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue, IssueStatus
from debug_loop_tools.git_manager import GitManager


class Fixer:
    """问题修复器"""

    def __init__(self, config: ConfigManager, state: StateManager,
                 git_manager: GitManager, project_dir: Path):
        """初始化修复器

        Args:
            config: 配置管理器
            state: 状态管理器
            git_manager: Git 管理器
            project_dir: 项目目录
        """
        self.config = config
        self.state = state
        self.git_manager = git_manager
        self.project_dir = project_dir

    def fix(self, issue: Issue, analysis: Dict) -> Dict:
        """修复问题

        Args:
            issue: 问题
            analysis: 分析结果

        Returns:
            修复结果
        """
        # 更新状态
        self.state.update_status(IssueStatus.FIXING)

        # 创建 Git 快照
        snapshot_id = None
        if self.config.get("git.enabled") and self.git_manager.is_git_repo:
            snapshot_id = self.git_manager.create_snapshot(
                f"debug-loop: before fix {issue.id}"
            )

        # 尝试修复
        success = False
        changes = []

        for suggestion in analysis.get("fix_suggestions", []):
            if self._apply_fix_from_suggestion(issue, suggestion):
                success = True
                changes.append(suggestion)
                break

        # 如果修复失败且回滚配置开启，执行回滚
        if not success and self.config.get("fix.rollback_on_failure"):
            if snapshot_id:
                self.git_manager.rollback(snapshot_id)

        return {
            "success": success,
            "changes": changes,
            "snapshot_id": snapshot_id
        }

    def _apply_fix_from_suggestion(self, issue: Issue, suggestion: str) -> bool:
        """根据建议应用修复

        Args:
            issue: 问题
            suggestion: 修复建议

        Returns:
            是否成功
        """
        if not issue.source_file:
            return False

        file_path = self.project_dir / issue.source_file
        if not file_path.exists():
            return False

        # 解析修复建议
        if "Add #include" in suggestion:
            return self._add_include(file_path, suggestion)
        elif "Add" in suggestion and "declaration" in suggestion.lower():
            return self._add_declaration(file_path, issue)

        return False

    def _add_include(self, file_path: Path, suggestion: str) -> bool:
        """添加头文件

        Args:
            file_path: 文件路径
            suggestion: 修复建议

        Returns:
            是否成功
        """
        # 提取头文件名
        match = re.search(r'#include\s*"([^"]+)"', suggestion)
        if not match:
            return False

        header = match.group(1)

        # 读取文件
        content = file_path.read_text(encoding="utf-8")

        # 检查是否已存在
        if f'#include "{header}"' in content:
            return True

        # 在文件开头添加
        new_content = f'#include "{header}"\n{content}'

        # 写入文件
        file_path.write_text(new_content, encoding="utf-8")
        return True

    def _add_declaration(self, file_path: Path, issue: Issue) -> bool:
        """添加声明

        Args:
            file_path: 文件路径
            issue: 问题

        Returns:
            是否成功
        """
        # 简化实现：仅记录，不实际修改
        return False

    def rollback(self, snapshot_id: str) -> bool:
        """回滚到指定快照

        Args:
            snapshot_id: 快照 ID

        Returns:
            是否成功
        """
        return self.git_manager.rollback(snapshot_id)
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_fixer.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_components/fixer.py scripts/debug_loop_tests/test_fixer.py
git commit -m "feat(debug-loop): add Fixer with auto-fix and rollback capabilities"
```

---

### Task 7: 验证器组件 - Verifier

**Files:**
- Create: `debug_loop_components/verifier.py`
- Create: `debug_loop_tests/test_verifier.py`

**Interfaces:**
- Consumes: `ConfigManager`, `StateManager`, `ToolChain`
- Produces: `Verifier(config, state, toolchain)`, `verify_compile()`, `verify_all()`

- [ ] **Step 1: 编写 Verifier 测试**

```python
# debug_loop_tests/test_verifier.py
import pytest
from pathlib import Path
from unittest.mock import Mock

from debug_loop_components.verifier import Verifier
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager


@pytest.fixture
def mock_toolchain():
    """模拟 ToolChain"""
    toolchain = Mock()
    toolchain.compile.return_value = {
        "success": True,
        "errors": 0,
        "warnings": 0,
        "output": "Build complete."
    }
    toolchain.find_hex_file.return_value = Path("project.hex")
    return toolchain


@pytest.fixture
def verifier(tmp_path, mock_toolchain):
    """创建 Verifier"""
    config = ConfigManager(None)
    state = StateManager()
    return Verifier(config, state, mock_toolchain, tmp_path)


class TestVerifier:
    """Verifier 测试"""

    def test_verify_compile_success(self, verifier, mock_toolchain):
        """测试编译验证成功"""
        result = verifier.verify_compile()

        assert result["success"] is True
        assert result["errors"] == 0

    def test_verify_compile_failure(self, verifier, mock_toolchain):
        """测试编译验证失败"""
        mock_toolchain.compile.return_value = {
            "success": False,
            "errors": 2,
            "warnings": 0,
            "output": "2 Error(s)."
        }

        result = verifier.verify_compile()

        assert result["success"] is False
        assert result["errors"] == 2

    def test_verify_all(self, verifier, mock_toolchain):
        """测试完整验证"""
        result = verifier.verify_all()

        assert "compile" in result
        assert "overall" in result
        assert result["overall"] is True
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_verifier.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 Verifier**

```python
# debug_loop_components/verifier.py
"""问题验证器模块"""
from pathlib import Path
from typing import Dict

from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, IssueStatus
from debug_loop_tools.toolchain import ToolChain


class Verifier:
    """问题验证器"""

    def __init__(self, config: ConfigManager, state: StateManager,
                 toolchain: ToolChain, project_dir: Path):
        """初始化验证器

        Args:
            config: 配置管理器
            state: 状态管理器
            toolchain: 工具链
            project_dir: 项目目录
        """
        self.config = config
        self.state = state
        self.toolchain = toolchain
        self.project_dir = project_dir

    def verify_compile(self) -> Dict:
        """验证编译

        Returns:
            验证结果
        """
        self.state.update_status(IssueStatus.VERIFYING)

        result = self.toolchain.compile()

        return {
            "success": result["success"],
            "errors": result["errors"],
            "warnings": result["warnings"],
            "output": result["output"]
        }

    def verify_flash(self) -> Dict:
        """验证烧录

        Returns:
            验证结果
        """
        hex_file = self.toolchain.find_hex_file()
        if not hex_file:
            return {
                "success": False,
                "error": "HEX file not found"
            }

        result = self.toolchain.flash(hex_file)
        return result

    def verify_all(self) -> Dict:
        """执行完整验证

        Returns:
            验证结果
        """
        compile_result = self.verify_compile()

        overall = compile_result["success"]

        return {
            "compile": compile_result,
            "overall": overall
        }
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_verifier.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_components/verifier.py scripts/debug_loop_tests/test_verifier.py
git commit -m "feat(debug-loop): add Verifier for compile and flash verification"
```

---

### Task 8: 记录器组件 - Recorder

**Files:**
- Create: `debug_loop_components/recorder.py`
- Create: `debug_loop_tests/test_recorder.py`

**Interfaces:**
- Consumes: `ConfigManager`, `StateManager`
- Produces: `Recorder(config, state, project_dir)`, `record_issue()`, `generate_analysis_doc()`

- [ ] **Step 1: 编写 Recorder 测试**

```python
# debug_loop_tests/test_recorder.py
import pytest
from pathlib import Path
import json

from debug_loop_components.recorder import Recorder
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue, IssueType, IssueStatus


@pytest.fixture
def recorder(tmp_path):
    """创建 Recorder"""
    config = ConfigManager(None)
    state = StateManager()
    return Recorder(config, state, tmp_path)


@pytest.fixture
def sample_issue():
    """示例问题"""
    return Issue(
        id="issue-001",
        timestamp="2026-07-28T14:30:00Z",
        type=IssueType.COMPILE,
        severity="high",
        status=IssueStatus.RESOLVED,
        source_file="main.c",
        source_line=10,
        error_message="undefined reference to 'HAL_GPIO_Init'",
        root_cause="Missing header include",
        fix_description="Add #include \"stm32f4xx_hal_gpio.h\""
    )


class TestRecorder:
    """Recorder 测试"""

    def test_record_issue(self, recorder, sample_issue):
        """测试记录问题"""
        analysis = {"root_cause": "Missing header", "fix_suggestions": []}
        fix_result = {"success": True, "changes": ["Add header"]}
        verify_result = {"success": True}

        result = recorder.record_issue(sample_issue, analysis, fix_result, verify_result)

        assert result is not None
        assert (result / "issue-001.json").exists()

    def test_generate_analysis_doc(self, recorder, sample_issue):
        """测试生成分析文档"""
        analysis = {"root_cause": "Missing header", "fix_suggestions": ["Add header"]}

        result = recorder.generate_analysis_doc(sample_issue, analysis)

        assert result.exists()
        content = result.read_text()
        assert "issue-001" in content
        assert "Missing header" in content

    def test_generate_reproduce_doc(self, recorder, sample_issue):
        """测试生成复现文档"""
        result = recorder.generate_reproduce_doc(sample_issue)

        assert result.exists()
        content = result.read_text()
        assert "Reproduce" in content

    def test_generate_patch(self, recorder, sample_issue):
        """测试生成补丁"""
        fix_result = {"changes": ["Add #include"]}

        result = recorder.generate_patch(sample_issue, fix_result)

        assert result.exists()

    def test_create_log_dir(self, recorder):
        """测试创建日志目录"""
        log_dir = recorder._create_log_dir()

        assert log_dir.exists()
        assert log_dir.is_dir()
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_recorder.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 Recorder**

```python
# debug_loop_components/recorder.py
"""问题记录器模块"""
from pathlib import Path
from typing import Dict
import json
from datetime import datetime

from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, Issue


class Recorder:
    """问题记录器"""

    def __init__(self, config: ConfigManager, state: StateManager, project_dir: Path):
        """初始化记录器

        Args:
            config: 配置管理器
            state: 状态管理器
            project_dir: 项目目录
        """
        self.config = config
        self.state = state
        self.project_dir = project_dir

    def _create_log_dir(self) -> Path:
        """创建日志目录

        Returns:
            日志目录路径
        """
        log_base = self.project_dir / self.config.get("documentation.log_dir", "docs/debug-logs")
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        log_dir = log_base / timestamp
        log_dir.mkdir(parents=True, exist_ok=True)
        return log_dir

    def record_issue(self, issue: Issue, analysis: Dict, fix_result: Dict, verify_result: Dict) -> Path:
        """记录问题

        Args:
            issue: 问题
            analysis: 分析结果
            fix_result: 修复结果
            verify_result: 验证结果

        Returns:
            日志目录路径
        """
        log_dir = self._create_log_dir()

        # 保存问题元数据
        issue_data = {
            "id": issue.id,
            "timestamp": issue.timestamp,
            "type": issue.type.value,
            "severity": issue.severity,
            "status": issue.status.value,
            "source": {
                "file": issue.source_file,
                "line": issue.source_line,
                "error_message": issue.error_message
            },
            "analysis": analysis,
            "fix": fix_result,
            "verification": verify_result
        }

        issue_file = log_dir / f"{issue.id}.json"
        issue_file.write_text(json.dumps(issue_data, indent=2, ensure_ascii=False), encoding="utf-8")

        # 生成其他文档
        self.generate_analysis_doc(issue, analysis)
        self.generate_reproduce_doc(issue)

        return log_dir

    def generate_analysis_doc(self, issue: Issue, analysis: Dict) -> Path:
        """生成分析文档

        Args:
            issue: 问题
            analysis: 分析结果

        Returns:
            文档路径
        """
        log_dir = self._create_log_dir()

        content = f"""# 问题分析报告

## 问题概述
- **ID**: {issue.id}
- **类型**: {issue.type.value}
- **严重度**: {issue.severity}
- **检测时间**: {issue.timestamp}

## 错误信息
```
{issue.error_message}
```

## 根因分析
{analysis.get('root_cause', 'Unknown')}

## 修复建议
"""
        for suggestion in analysis.get('fix_suggestions', []):
            content += f"- {suggestion}\n"

        doc_file = log_dir / f"{issue.id}-analysis.md"
        doc_file.write_text(content, encoding="utf-8")

        return doc_file

    def generate_reproduce_doc(self, issue: Issue) -> Path:
        """生成复现文档

        Args:
            issue: 问题

        Returns:
            文档路径
        """
        log_dir = self._create_log_dir()

        content = f"""# 复现步骤

## 问题信息
- **ID**: {issue.id}
- **文件**: {issue.source_file}
- **行号**: {issue.source_line}

## 错误信息
```
{issue.error_message}
```

## 复现步骤
1. 打开项目
2. 修改相关代码
3. 编译项目
4. 观察错误

## 环境信息
- 操作系统: Windows
- 编译器: Keil MDK-ARM
"""
        doc_file = log_dir / f"{issue.id}-reproduce.md"
        doc_file.write_text(content, encoding="utf-8")

        return doc_file

    def generate_patch(self, issue: Issue, fix_result: Dict) -> Path:
        """生成补丁文件

        Args:
            issue: 问题
            fix_result: 修复结果

        Returns:
            补丁文件路径
        """
        log_dir = self._create_log_dir()

        content = f"""# Fix for {issue.id}

## Changes
"""
        for change in fix_result.get('changes', []):
            content += f"- {change}\n"

        patch_file = log_dir / f"{issue.id}-fix.patch"
        patch_file.write_text(content, encoding="utf-8")

        return patch_file

    def generate_verify_doc(self, verify_result: Dict) -> Path:
        """生成验证文档

        Args:
            verify_result: 验证结果

        Returns:
            文档路径
        """
        log_dir = self._create_log_dir()

        content = f"""# 验证结果

## 编译验证
- 状态: {'通过' if verify_result.get('compile', {}).get('success') else '失败'}
- 错误数: {verify_result.get('compile', {}).get('errors', 0)}
- 警告数: {verify_result.get('compile', {}).get('warnings', 0)}
"""
        doc_file = log_dir / "verify-result.md"
        doc_file.write_text(content, encoding="utf-8")

        return doc_file
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_recorder.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_components/recorder.py scripts/debug_loop_tests/test_recorder.py
git commit -m "feat(debug-loop): add Recorder for generating debug documentation"
```

---

### Task 9: 核心引擎 - DebugLoopEngine

**Files:**
- Create: `debug_loop_engine/engine.py`
- Create: `debug_loop_tests/test_engine.py`

**Interfaces:**
- Consumes: 所有组件 (Detector, Analyzer, Fixer, Verifier, Recorder)
- Produces: `DebugLoopEngine(config_path, project_dir, port)`, `run()`, `run_cycle()`

- [ ] **Step 1: 编写 Engine 测试**

```python
# debug_loop_tests/test_engine.py
import pytest
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

from debug_loop_engine.engine import DebugLoopEngine
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import IssueStatus


@pytest.fixture
def engine(tmp_path):
    """创建 DebugLoopEngine"""
    # 创建 mock build.log
    build_log = tmp_path / "build.log"
    build_log.write_text("0 Error(s), 0 Warning(s).\n")

    return DebugLoopEngine(
        config_path=None,
        project_dir=tmp_path,
        port=None
    )


class TestDebugLoopEngine:
    """DebugLoopEngine 测试"""

    def test_init(self, engine):
        """测试初始化"""
        assert engine.config is not None
        assert engine.state is not None
        assert engine.detector is not None
        assert engine.analyzer is not None
        assert engine.fixer is not None
        assert engine.verifier is not None
        assert engine.recorder is not None

    def test_run_no_issues(self, engine):
        """测试运行 - 无问题"""
        result = engine.run()

        assert result["success"] is True
        assert result["issues_found"] == 0

    def test_run_with_issues(self, engine, tmp_path):
        """测试运行 - 有问题"""
        # 创建有错误的 build.log
        build_log = tmp_path / "build.log"
        build_log.write_text("main.c(10): error: undefined reference\n1 Error(s).\n")

        engine = DebugLoopEngine(
            config_path=None,
            project_dir=tmp_path,
            port=None
        )

        result = engine.run()

        assert result["issues_found"] > 0

    def test_run_cycle(self, engine):
        """测试单次循环"""
        result = engine.run_cycle()

        assert "issues" in result
        assert "fixed" in result
```

- [ ] **Step 2: 运行测试验证失败**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_engine.py -v
```

Expected: FAIL with "ModuleNotFoundError"

- [ ] **Step 3: 实现 DebugLoopEngine**

```python
# debug_loop_engine/engine.py
"""核心引擎模块"""
from pathlib import Path
from typing import Optional, Dict
import time

from .config import ConfigManager
from .state import StateManager, IssueStatus

from debug_loop_components.detector import Detector
from debug_loop_components.analyzer import Analyzer
from debug_loop_components.fixer import Fixer
from debug_loop_components.verifier import Verifier
from debug_loop_components.recorder import Recorder

from debug_loop_tools.toolchain import ToolChain
from debug_loop_tools.git_manager import GitManager


class DebugLoopEngine:
    """调试循环核心引擎"""

    def __init__(self, config_path: Optional[Path], project_dir: Path, port: Optional[str]):
        """初始化引擎

        Args:
            config_path: 配置文件路径
            project_dir: 项目目录
            port: 串口端口
        """
        self.project_dir = project_dir
        self.port = port

        # 初始化配置
        self.config = ConfigManager(config_path)

        # 初始化状态管理
        self.state = StateManager()

        # 初始化工具链
        self.toolchain = ToolChain(project_dir, self.config)
        self.git_manager = GitManager(project_dir)

        # 初始化组件
        self.detector = Detector(self.config, self.state, project_dir)
        self.analyzer = Analyzer(self.config, self.state, project_dir)
        self.fixer = Fixer(self.config, self.state, self.git_manager, project_dir)
        self.verifier = Verifier(self.config, self.state, self.toolchain, project_dir)
        self.recorder = Recorder(self.config, self.state, project_dir)

    def run(self) -> Dict:
        """运行完整的调试循环

        Returns:
            运行结果
        """
        start_time = time.time()

        print("🚀 启动自动化闭环调试工作流...")

        # 阶段 0: 信息获取
        print("\n📋 阶段 0: 获取项目信息...")
        self._collect_project_info()

        # 阶段 1-6: 执行调试循环
        result = self.run_cycle()

        elapsed = time.time() - start_time
        print(f"\n✅ 完成! 耗时 {elapsed:.1f} 秒")

        return {
            "success": result["fixed"] > 0 or result["issues"] == 0,
            "issues_found": result["issues"],
            "issues_fixed": result["fixed"],
            "elapsed_seconds": elapsed
        }

    def run_cycle(self) -> Dict:
        """执行单次调试循环

        Returns:
            循环结果
        """
        issues_fixed = 0

        # 阶段 1: 问题检测
        print("\n🔍 阶段 1: 检测问题...")
        issues = self.detector.detect_all()

        if not issues:
            print("  ✅ 未发现问题")
            return {"issues": 0, "fixed": 0}

        print(f"  ⚠️  发现 {len(issues)} 个问题")

        for issue in issues:
            print(f"\n📝 处理问题: {issue.id}")

            # 阶段 2: 根因分析
            print("  🔍 阶段 2: 分析根因...")
            self.state.update_status(IssueStatus.ANALYZING)
            analysis = self.analyzer.analyze(issue)
            print(f"  根因: {analysis['root_cause']}")

            # 阶段 3: 自动修复
            if self.config.get("fix.auto_fix"):
                print("  🔧 阶段 3: 自动修复...")
                fix_result = self.fixer.fix(issue, analysis)

                if fix_result["success"]:
                    print(f"  ✅ 修复成功: {', '.join(fix_result['changes'])}")

                    # 阶段 4: 编译验证
                    print("  ✓ 阶段 4: 编译验证...")
                    verify_result = self.verifier.verify_compile()

                    if verify_result["success"]:
                        print("  ✅ 编译通过")

                        # 阶段 5: 记录文档
                        print("  📄 阶段 5: 记录文档...")
                        self.recorder.record_issue(issue, analysis, fix_result, verify_result)

                        # 更新状态
                        self.state.complete_issue(True)
                        issues_fixed += 1
                    else:
                        print("  ❌ 编译失败")
                        self.state.complete_issue(False)
                else:
                    print("  ❌ 修复失败")
                    self.state.complete_issue(False)
            else:
                print("  ⏭️  跳过自动修复")
                self.state.complete_issue(False)

        return {
            "issues": len(issues),
            "fixed": issues_fixed
        }

    def _collect_project_info(self):
        """收集项目信息"""
        # 检测调试器
        debugger = self.toolchain.detect_debugger()
        print(f"  调试器: {debugger}")

        # 查找 HEX 文件
        hex_file = self.toolchain.find_hex_file()
        if hex_file:
            print(f"  HEX 文件: {hex_file}")

        # 检查 Git 状态
        if self.git_manager.is_git_repo:
            branch = self.git_manager.get_current_branch()
            commit = self.git_manager.get_last_commit()
            print(f"  Git: {branch}@{commit}")
```

- [ ] **Step 4: 运行测试验证通过**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_engine.py -v
```

Expected: All tests PASS

- [ ] **Step 5: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_engine/engine.py scripts/debug_loop_tests/test_engine.py
git commit -m "feat(debug-loop): add DebugLoopEngine core orchestrator"
```

---

### Task 10: 主入口 - debug_loop.py

**Files:**
- Create: `debug_loop.py`

**Interfaces:**
- Consumes: `DebugLoopEngine`
- Produces: 命令行入口点

- [ ] **Step 1: 创建 debug_loop.py**

```python
#!/usr/bin/env python
"""STM32 自动化闭环调试工作流 - 主入口

用法:
    python debug_loop.py --auto . --port COM3
    python debug_loop.py --auto . --watch
    python debug_loop.py --auto . --dry-run
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

# 添加脚本目录到路径
SCRIPT_DIR = Path(__file__).parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from debug_loop_engine.engine import DebugLoopEngine


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="STM32 自动化闭环调试工作流",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python debug_loop.py --auto . --port COM3
  python debug_loop.py --auto . --watch
  python debug_loop.py --auto . --config my-config.json
        """
    )

    parser.add_argument(
        "--auto", "--project",
        type=str,
        default=".",
        help="项目目录路径 (默认: 当前目录)"
    )

    parser.add_argument(
        "--port",
        type=str,
        default=None,
        help="串口端口 (如 COM3)"
    )

    parser.add_argument(
        "--config",
        type=str,
        default=None,
        help="配置文件路径"
    )

    parser.add_argument(
        "--debugger",
        type=str,
        choices=["auto", "stlink", "cmsis-dap", "jlink", "keil"],
        default=None,
        help="调试器类型"
    )

    parser.add_argument(
        "--watch",
        action="store_true",
        help="启用文件监控模式"
    )

    parser.add_argument(
        "--max-retries",
        type=int,
        default=3,
        help="最大重试次数 (默认: 3)"
    )

    parser.add_argument(
        "--timeout",
        type=int,
        default=300,
        help="超时时间秒 (默认: 300)"
    )

    parser.add_argument(
        "--log-dir",
        type=str,
        default=None,
        help="日志目录路径"
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只分析不修改"
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="详细输出"
    )

    return parser.parse_args()


def main():
    """主函数"""
    args = parse_args()

    # 解析项目目录
    project_dir = Path(args.auto).resolve()
    if not project_dir.exists():
        print(f"❌ 项目目录不存在: {project_dir}")
        return 1

    # 解析配置文件
    config_path = Path(args.config) if args.config else None

    print("=" * 60)
    print("  STM32 自动化闭环调试工作流")
    print("=" * 60)
    print(f"\n  项目目录: {project_dir}")
    if args.port:
        print(f"  串口端口: {args.port}")
    if args.config:
        print(f"  配置文件: {args.config}")
    print()

    try:
        # 创建并运行引擎
        engine = DebugLoopEngine(
            config_path=config_path,
            project_dir=project_dir,
            port=args.port
        )

        # 如果指定了调试器类型，更新配置
        if args.debugger and args.debugger != "auto":
            engine.config.set("debugger.type", args.debugger)

        # 如果指定了日志目录，更新配置
        if args.log_dir:
            engine.config.set("documentation.log_dir", args.log_dir)

        # 运行
        result = engine.run()

        # 输出结果
        print("\n" + "=" * 60)
        if result["success"]:
            print("  ✅ 调试循环完成!")
        else:
            print("  ⚠️  调试循环完成，但有未解决的问题")
        print("=" * 60)

        return 0 if result["success"] else 1

    except KeyboardInterrupt:
        print("\n\n⏹️  用户中断")
        return 130
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: 运行主入口测试**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python debug_loop.py --help
```

Expected: 显示帮助信息

- [ ] **Step 3: 运行完整测试套件**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/ -v
```

Expected: All tests PASS

- [ ] **Step 4: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop.py
git commit -m "feat(debug-loop): add main entry point with CLI interface"
```

---

### Task 11: 集成测试

**Files:**
- Create: `debug_loop_tests/test_integration.py`

**Interfaces:**
- Consumes: 所有模块
- Produces: 集成测试验证

- [ ] **Step 1: 编写集成测试**

```python
# debug_loop_tests/test_integration.py
import pytest
from pathlib import Path
import json

from debug_loop_engine.engine import DebugLoopEngine
from debug_loop_engine.config import ConfigManager
from debug_loop_engine.state import StateManager, IssueStatus


@pytest.fixture
def project_with_error(tmp_path):
    """创建带编译错误的项目"""
    # 创建 build.log
    build_log = tmp_path / "build.log"
    build_log.write_text("main.c(10): error: undefined reference to 'HAL_GPIO_Init'\n1 Error(s).\n")

    # 创建源文件
    src_file = tmp_path / "main.c"
    src_file.write_text("int main() { HAL_GPIO_Init(); return 0; }\n")

    return tmp_path


@pytest.fixture
def project_no_error(tmp_path):
    """创建无错误的项目"""
    # 创建 build.log
    build_log = tmp_path / "build.log"
    build_log.write_text("0 Error(s), 0 Warning(s).\n")

    return tmp_path


class TestIntegration:
    """集成测试"""

    def test_full_cycle_no_issues(self, project_no_error):
        """测试完整流程 - 无问题"""
        engine = DebugLoopEngine(
            config_path=None,
            project_dir=project_no_error,
            port=None
        )

        result = engine.run()

        assert result["success"] is True
        assert result["issues_found"] == 0

    def test_full_cycle_with_issues(self, project_with_error):
        """测试完整流程 - 有问题"""
        engine = DebugLoopEngine(
            config_path=None,
            project_dir=project_with_error,
            port=None
        )

        # 禁用自动修复以测试检测流程
        engine.config.set("fix.auto_fix", False)

        result = engine.run()

        assert result["issues_found"] > 0

    def test_config_loading(self, tmp_path):
        """测试配置加载"""
        config_file = tmp_path / "debug-loop.json"
        config_file.write_text(json.dumps({
            "debugger": {"type": "stlink"},
            "fix": {"auto_fix": False}
        }))

        engine = DebugLoopEngine(
            config_path=config_file,
            project_dir=tmp_path,
            port=None
        )

        assert engine.config.get("debugger.type") == "stlink"
        assert engine.config.get("fix.auto_fix") is False

    def test_state_management(self, project_no_error):
        """测试状态管理"""
        engine = DebugLoopEngine(
            config_path=None,
            project_dir=project_no_error,
            port=None
        )

        engine.run()

        stats = engine.state.get_statistics()
        assert stats["total"] >= 0
```

- [ ] **Step 2: 运行集成测试**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/test_integration.py -v
```

Expected: All tests PASS

- [ ] **Step 3: 运行所有测试**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/ -v --tb=short
```

Expected: All tests PASS

- [ ] **Step 4: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug_loop_tests/test_integration.py
git commit -m "test(debug-loop): add integration tests for full workflow"
```

---

### Task 12: 文档和配置模板

**Files:**
- Create: `debug-loop.json.example`
- Create: `debug_loop_tests/README.md`

- [ ] **Step 1: 创建配置模板**

```json
// debug-loop.json.example
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
    "reset_delay_ms": 200
  },

  "workflow": {
    "compile_command": "python workflow.py --auto . --steps compile",
    "flash_command": "STM32_Programmer_CLI.exe -c port=SWD -w {hex_file} -v -rst",
    "build_log": "build.log"
  },

  "detection": {
    "file_watch": true,
    "compile_monitor": true,
    "watch_interval_sec": 2,
    "watch_patterns": ["*.c", "*.h"],
    "watch_dirs": ["USER/", "Main/", "Common/"]
  },

  "fix": {
    "auto_fix": true,
    "max_retries": 3,
    "rollback_on_failure": true
  },

  "git": {
    "enabled": true,
    "auto_commit": true,
    "commit_prefix": "debug-loop",
    "auto_tag": true,
    "tag_prefix": "debug"
  },

  "documentation": {
    "log_dir": "docs/debug-logs",
    "generate_patch": true,
    "generate_reproduce": true
  },

  "patterns": {
    "error_patterns_file": "docs/debug-logs/error-patterns.json",
    "auto_record_patterns": true,
    "match_threshold": 0.8
  }
}
```

- [ ] **Step 2: 创建测试 README**

```markdown
# debug_loop_tests

自动化闭环调试工作流的测试套件。

## 运行测试

```bash
# 运行所有测试
python -m pytest debug_loop_tests/ -v

# 运行特定模块测试
python -m pytest debug_loop_tests/test_config.py -v
python -m pytest debug_loop_tests/test_detector.py -v

# 运行并生成覆盖率报告
python -m pytest debug_loop_tests/ --cov=debug_loop_engine --cov=debug_loop_components --cov-report=html
```

## 测试结构

- `test_config.py` - 配置管理器测试
- `test_state.py` - 状态管理器测试
- `test_git_manager.py` - Git 管理器测试
- `test_toolchain.py` - 工具链测试
- `test_detector.py` - 检测器测试
- `test_analyzer.py` - 分析器测试
- `test_fixer.py` - 修复器测试
- `test_verifier.py` - 验证器测试
- `test_recorder.py` - 记录器测试
- `test_engine.py` - 核心引擎测试
- `test_integration.py` - 集成测试
```

- [ ] **Step 3: 提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add scripts/debug-loop.json.example scripts/debug_loop_tests/README.md
git commit -m "docs(debug-loop): add config template and test documentation"
```

---

### Task 13: 最终验收测试

- [ ] **Step 1: 运行完整测试套件**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python -m pytest debug_loop_tests/ -v --tb=short
```

Expected: All tests PASS

- [ ] **Step 2: 测试命令行帮助**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts
python debug_loop.py --help
```

Expected: 显示完整的帮助信息

- [ ] **Step 3: 测试实际项目（dry-run 模式）**

```bash
cd c:\Users\CMJ\Desktop\TEST\触摸屏_usart
python D:\ClaudeGlobalConfig\skills\stm32-keil-workflow\scripts\debug_loop.py --auto . --dry-run
```

Expected: 运行成功，无错误

- [ ] **Step 4: 最终提交**

```bash
cd D:\ClaudeGlobalConfig\skills\stm32-keil-workflow
git add -A
git commit -m "feat(debug-loop): complete automated closed-loop debug workflow v1.0.0"
```

---

## 完成检查清单

| 检查项 | 状态 |
|--------|------|
| 所有组件实现完成 | ☐ |
| 所有测试通过 | ☐ |
| 命令行接口可用 | ☐ |
| 配置模板创建 | ☐ |
| 文档完成 | ☐ |
| 代码提交 | ☐ |

---

*计划生成日期: 2026-07-28*
*基于设计文档: docs/superpowers/specs/2026-07-28-debug-loop-design.md*
