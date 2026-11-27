add_executable(voryx_opengl_triangle main.cpp)
target_include_directories(voryx_opengl_triangle PRIVATE
    "${PROJECT_SOURCE_DIR}/samples/common"
    "${PROJECT_SOURCE_DIR}/assets/shaders/opengl")
target_link_libraries(voryx_opengl_triangle PRIVATE Voryx::runtime)
set_target_properties(voryx_opengl_triangle PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_opengl_triangle)
