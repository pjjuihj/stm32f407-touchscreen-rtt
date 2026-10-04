# 依赖与许可证核验

此清单记录证据，不代替法律意见。记录实际 vendored 内容和许可证文件；不要仅凭 README、目录名或上游当前默认分支认定本地副本的版本或许可证。

| 组件 | 本地路径/来源 | 本地精确版本或提交 | 本地许可证文件/文本 | 上游声明/来源 | 核验状态 |
|---|---|---|---|---|---|
| LVGL | `LVGL/`（待维护者核对） | 待核实 | 待核实 | [LVGL](https://github.com/lvgl/lvgl)：上游元数据标示 MIT | 待维护者核实 |
| STM32F4 HAL | `STM32F4xx_HAL_Driver/`（待维护者核对） | 待核实 | 待核实 | [STMicroelectronics/stm32f4xx-hal-driver](https://github.com/STMicroelectronics/stm32f4xx-hal-driver)：上游元数据标示 BSD-3-Clause | 待维护者核实 |
| SEGGER RTT | `SEGGER_RTT/`（待维护者核对） | 待核实 | 待核实 | [SEGGERMicro/RTT](https://github.com/SEGGERMicro/RTT)：上游采用 BSD 风格许可 | 待维护者核实 |

## 已知的本地声明与边界

- 项目 README 列出 LVGL v9.5.0，并设有独立的项目级 MIT License 章节；这不证明 vendored LVGL 副本的精确版本或许可证。仓库根目录当前没有 `LICENSE` 文件。
- 上游 LVGL 项目元数据标示 MIT；这仅为上游参考，不能单独证明当前 vendored 副本的修订、适用许可证或随附许可文本。维护者需逐项核对本地文件和对应上游版本。
- 本地 HAL/RTT 精确修订及其随附许可证文本尚未核实。维护者需针对当前 vendored 文件确认版本、版权声明、许可条款和再分发义务，再更新表格。
- 核验人/日期：待填写。
