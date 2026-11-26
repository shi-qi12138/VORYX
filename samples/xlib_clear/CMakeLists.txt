add_executable(voryx_xlib_clear main.cpp)
target_link_libraries(voryx_xlib_clear PRIVATE Voryx::runtime X11::X11)
set_target_properties(voryx_xlib_clear PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_xlib_clear)

target_include_directories(voryx_xlib_clear PRIVATE
    "${PROJECT_SOURCE_DIR}/samples/common" "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
