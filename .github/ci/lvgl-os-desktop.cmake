# CI-only adapter for the vendored LVGL source tree, which omits env_support.
# Keep the firmware source list and LVGL configuration in the repository.
file(GLOB_RECURSE LVGL_CI_SOURCES CONFIGURE_DEPENDS
    "${LVGL_ROOT_DIR}/src/*.c"
)
list(FILTER LVGL_CI_SOURCES EXCLUDE REGEX "/drivers/")

add_library(lvgl STATIC ${LVGL_CI_SOURCES})
target_include_directories(lvgl PUBLIC
    "${LVGL_ROOT_DIR}"
    "${LVGL_ROOT_DIR}/src"
    "${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
