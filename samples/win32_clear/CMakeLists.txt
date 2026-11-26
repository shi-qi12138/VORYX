add_executable(voryx_win32_clear main.cpp)
target_link_libraries(voryx_win32_clear PRIVATE Voryx::runtime user32)
set_target_properties(voryx_win32_clear PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_win32_clear)

target_include_directories(voryx_win32_clear PRIVATE
    "${PROJECT_SOURCE_DIR}/samples/common" "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
