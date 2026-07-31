cmake_minimum_required(VERSION 3.10)
project(lvgl_tests C)

set(CMAKE_C_STANDARD 11)

# 包含 LVGL 源码
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL)
add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)

# 禁用不需要的 LVGL 功能
set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 测试源文件
set(TEST_SOURCES
    test_status_bar.c
    test_bottom_nav.c
    test_notification.c
    test_main.c
)

# 创建测试可执行文件
add_executable(lvgl_tests ${TEST_SOURCES})

# 包含目录
target_include_directories(lvgl_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
)

# 链接库
target_link_libraries(lvgl_tests
    lvgl
)

# 添加测试
add_test(NAME lvgl_tests COMMAND lvgl_tests)
