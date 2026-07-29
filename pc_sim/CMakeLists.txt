cmake_minimum_required(VERSION 3.10)
project(lvgl_pc_sim C)

set(CMAKE_C_STANDARD 11)

# 查找 SDL2（Windows 上使用 find_package 或手动指定路径）
# 可以通过 -DSDL2_DIR=... 指定 SDL2 安装路径
# 如果 SDL2 未安装，可以通过 -DSKIP_SDL2=ON 跳过（仅用于验证构建）
option(SKIP_SDL2 "跳过 SDL2 查找（用于验证构建）" OFF)
if(NOT SKIP_SDL2)
    find_package(SDL2 REQUIRED)
else()
    message(STATUS "Skipping SDL2 search (SKIP_SDL2=ON)")
    # 创建一个虚拟的 SDL2 目标
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
endif()

# 包含 LVGL 源码（复用父目录的 LVGL）
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL)
add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)

# 禁用不需要的 LVGL 功能
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lv_conf.h CACHE STRING "" FORCE)
set(LV_USE_DEMOWidgets OFF CACHE BOOL "" FORCE)
set(LV_USE_DEMOBENCHMARKS OFF CACHE BOOL "" FORCE)

# PC 模拟源文件
add_executable(lvgl_pc_sim
    main.c
    sdl_driver.c
)

# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
)

# 包含目录
target_include_directories(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
    ${SDL2_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(lvgl_pc_sim
    lvgl
    SDL2::SDL2
)
