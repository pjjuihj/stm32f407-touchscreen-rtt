# 项目治理手册

本目录提供本项目从需求、设计、实现、验证到稳定发布与恢复的轻量记录规范。治理记录应能回答：为什么改、改了什么、如何验证、由谁确认、如何恢复。优先使用 GitHub Issues 和 Pull Requests 的现有编号关联工作，不另建平行跟踪系统。

## 入口与现有规范

- 需求、缺陷和产品需求文档（PRD）的权威入口是 GitHub Issues：[`pjjuihj/stm32f407-touchscreen-rtt`](https://github.com/pjjuihj/stm32f407-touchscreen-rtt/issues)。PR 用于实现与评审，应链接相关 issue。分诊只使用 `needs-triage`、`needs-info`、`ready-for-agent`、`ready-for-human`、`wontfix` 五个规范标签。
- 仓库已有的 issue tracker、领域文档和 ADR 规范分别见 [`docs/agents/issue-tracker.md`](../agents/issue-tracker.md)、[`docs/agents/domain.md`](../agents/domain.md) 与 [`docs/adr/`](../adr/)。本手册仅索引并补足治理流程，不替代或改写它们。
- 安全漏洞报告入口见仓库根目录 [`SECURITY.md`](../../SECURITY.md)。

## 工作流与记录约定

1. 在 GitHub issue 中写明问题或需求、验收条件及风险；不复制出第二套需求编号。分诊时只用上述五个标签。
2. 对涉及多个模块、接口或边界行为的工作，先在关联 issue/PR 中附上功能与接口规格；采用仓库既有 ADR 记录需要长期保留的架构决策。
3. 每个变更通过 issue/PR 编号关联需求、代码和验证证据。PR 描述变更范围、兼容性、风险和验证；不得把 PR 当成独立需求入口。
4. 自动化测试以仓库内测试的路径和名称标识；实机实验使用 `TEST-ID`。记录命令/步骤、固件来源和原始结果位置。无法验证的项目明确写 `NOT RUN` 或 `NOT PROVEN`，不推断通过。
5. 报告构建与主机侧自动测试结果时，和 FLASH/HIL 结果分开。CI 不执行烧录、复位或任何硬件动作。软件通过不能代表板上验证通过。
6. 稳定版本采用 `vMAJOR.MINOR.PATCH`。初始稳定发布之前不制造或预先添加版本标签；由维护者审核候选版本及其证据。
7. 只有维护者记录 `FLASH=PASS` 且 `HIL=PASS`，并附可审阅证据，才可发布稳定版本。证据应指向确切源提交、产物和目标板验证结果。
8. 每次发布须记录恢复方案。恢复使用不可变发布资产及其哈希，并记录对应源提交；不得以可变分支名代替版本来源。

## 记录模板

需要时复制对应模板到关联 issue/PR 或适当的文档位置。具体文件保存位置服从仓库既有约定；不创建第二套编号或状态数据库。

- [需求与追溯](templates/requirements-traceability.md)
- [功能与接口规格](templates/feature-interface-spec.md)
- [变更记录](templates/change.md)
- [风险记录](templates/risk.md)
- [依赖与许可证清单](templates/dependencies-licenses.md)
- [测试与证据](templates/test-evidence.md)
- [稳定版本发布](templates/release.md)
- [恢复记录](templates/recovery.md)

## 版本和状态用语

- **构建**：指定源提交在指定主机/工具链上的构建结果。
- **主机测试**：不接触目标硬件的自动化测试结果，记录测试路径及名称。
- **FLASH**：维护者确认的指定产物下载/编程结果及其证据。
- **HIL**：维护者确认的硬件在环/实机验收结果及其证据。列出实际执行的 TEST-ID。
- **稳定发布**：满足本手册 FLASH 与 HIL 门槛，并有不可变资产、哈希、源提交及恢复记录的版本。

## 依赖许可证现状

项目 README 列出 LVGL v9.5.0，并另有项目级 MIT License 章节；这两项 README 信息不能据以确认 vendored LVGL 副本的精确版本或许可证。仓库根目录当前没有 `LICENSE` 文件。上游项目元数据表明 LVGL 为 MIT、ST STM32F4 HAL 为 BSD-3-Clause、SEGGER RTT 为 BSD 风格。当前本地检出的精确依赖修订、随包许可证文本及其适用范围尚待维护者逐项核对，见[依赖与许可证模板](templates/dependencies-licenses.md)。

上游参考：

- [LVGL](https://github.com/lvgl/lvgl)
- [STMicroelectronics/stm32f4xx-hal-driver](https://github.com/STMicroelectronics/stm32f4xx-hal-driver)
- [SEGGERMicro/RTT](https://github.com/SEGGERMicro/RTT)
