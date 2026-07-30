cmake_minimum_required(VERSION 3.12.4)
project(lvgl_pc_sim C CXX)

set(CMAKE_C_STANDARD 11)

# 禁用不需要的 LVGL 功能（必须在 add_subdirectory 之前设置）
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 查找 SDL2
# 可以通过 -DSDL2_DIR=... 或 -DCMAKE_PREFIX_PATH=... 指定 SDL2 安装路径
# 如果 SDL2 未安装，可以通过 -DSKIP_SDL2=ON 跳过（仅用于验证构建）
option(SKIP_SDL2 "跳过 SDL2 查找（用于验证构建）" OFF)
if(NOT SKIP_SDL2)
    # 先尝试 find_package（适用于有 SDL2Config.cmake 的情况）
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        # 手动查找 SDL2（适用于 MinGW 等非标准安装）
        find_path(SDL2_INCLUDE_DIR SDL2/SDL.h
            HINTS ENV SDL2_DIR
            PATHS
                ${CMAKE_PREFIX_PATH}/include
                /usr/include
                /usr/local/include
        )
        find_library(SDL2_LIBRARY
            NAMES SDL2
            HINTS ENV SDL2_DIR
            PATHS
                ${CMAKE_PREFIX_PATH}/lib
                /usr/lib
                /usr/local/lib
        )
        if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
            message(STATUS "Found SDL2: ${SDL2_LIBRARY}")
            message(STATUS "SDL2 include: ${SDL2_INCLUDE_DIR}")
            # 创建 imported target
            add_library(SDL2::SDL2 UNKNOWN IMPORTED)
            set_target_properties(SDL2::SDL2 PROPERTIES
                IMPORTED_LOCATION "${SDL2_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR "SDL2 not found. Install SDL2 or set SDL2_DIR.")
        endif()
    endif()
else()
    message(STATUS "Skipping SDL2 search (SKIP_SDL2=ON)")
    # 创建一个虚拟的 SDL2 目标
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
endif()

# 包含 LVGL 源码（复用父目录的 LVGL）
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL)
add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)

# 移除 ARM 专用的 Helium 汇编文件（PC 平台不支持）
get_target_property(LVGL_SOURCES lvgl SOURCES)
foreach(SRC ${LVGL_SOURCES})
    if(SRC MATCHES "helium.*\\.S$")
        list(REMOVE_ITEM LVGL_SOURCES "${SRC}")
    endif()
endforeach()
set_target_properties(lvgl PROPERTIES SOURCES "${LVGL_SOURCES}")

# PC 模拟源文件
add_executable(lvgl_pc_sim
    main.c
    sdl_driver.c
)

# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_selection.c
)

# 包含目录
target_include_directories(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
)

# 链接库
target_link_libraries(lvgl_pc_sim
    lvgl
    SDL2::SDL2
)
