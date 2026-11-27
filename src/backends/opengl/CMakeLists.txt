add_library(voryx_backend_opengl STATIC
    opengl_api.cpp
    opengl_buffer.cpp
    opengl_color_target.cpp
    opengl_device.cpp
    opengl_draw.cpp
    opengl_pipeline.cpp)
if(WIN32)
    target_sources(voryx_backend_opengl PRIVATE platform/wgl_context.cpp)
    target_link_libraries(voryx_backend_opengl PRIVATE opengl32 gdi32 user32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
    target_sources(voryx_backend_opengl PRIVATE platform/egl_context.cpp)
    target_link_libraries(voryx_backend_opengl PRIVATE OpenGL::EGL)
else()
    message(FATAL_ERROR "The OpenGL backend requires Windows or Linux.")
endif()
target_include_directories(voryx_backend_opengl PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/rhi" "${PROJECT_SOURCE_DIR}/src/core")
target_link_libraries(voryx_backend_opengl PUBLIC Voryx::rhi_api
    PRIVATE voryx_rhi voryx_backend_contract)
set_target_properties(voryx_backend_opengl PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME detail_opengl)
voryx_enable_warnings(voryx_backend_opengl)
