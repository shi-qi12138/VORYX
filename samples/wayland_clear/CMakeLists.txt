enable_language(C)
include(wayland_sample.cmake)
voryx_add_wayland_sample(voryx_wayland_clear)
voryx_enable_warnings(voryx_wayland_clear)

target_include_directories(voryx_wayland_clear PRIVATE
    "${PROJECT_SOURCE_DIR}/samples/common" "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
