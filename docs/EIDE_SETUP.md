# EIDE 环境配置指南

## 1. VS Code 设置 (`Project/.vscode/settings.json`)

```json
{
    // 编译器路径
    "EIDE.ARM.GCC.InstallDirectory": "D:\\arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi",
    "EIDE.Toolchain.AnyGcc.InstallDirectory": "D:\\arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi",
    "EIDE.ARM.ARMCC5.InstallDirectory": "D:\\k5\\ARM\\ARMCC",

    // 调试器路径（必须用 14.3，15.2 有 bug 会卡死）
    "cortex-debug.gdbPath": "D:/arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gdb.exe",
    "cortex-debug.armToolchainPath.windows": "D:/arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi/bin"
}
```

## 2. EIDE 项目配置 (`.eide/eide.yml`)

- **编译器**: AC5 (Keil ARM Compiler 5)
- **烧录器**: OpenOCD (CMSIS-DAP)
- **调试器**: cortex-debug

## 3. 编译配置 (AC5)

编译器选项抑制 LVGL 警告：
```
--diag_suppress=1 --diag_suppress=1295 --diag_suppress=188
--diag_suppress=111 --diag_suppress=191 --diag_suppress=177
--diag_suppress=546 --diag_suppress=68 --diag_suppress=1293
```

## 4. LVGL 配置 (`LVGL/lv_conf.h`)

- `LV_USE_BUTTON 1`
- `LV_USE_ILI9341 1`
- `LV_USE_LOG 1`
- 禁用不需要的：OpenGL、GLTF、FFmpeg、FreeType 等

## 5. 调试配置 (`.vscode/launch.json`)

使用 OpenOCD + CMSIS-DAP 调试 STM32F407。

## 6. 已知问题

| 问题 | 原因 | 解决 |
|------|------|------|
| GDB 15.2 卡死 | GDB 15.2 bug | 改用 14.3 |
| CMSIS-DAP CMD_INFO failed | USB 连接问题 | 拔插 USB/按 RESET |
| LVGL 编译警告多 | AC5 严格检查 | --diag_suppress |
