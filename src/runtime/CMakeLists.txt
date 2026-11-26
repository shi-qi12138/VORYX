add_library(voryx_runtime STATIC device_factory.cpp presenter.cpp)
add_library(Voryx::runtime ALIAS voryx_runtime)
target_link_libraries(voryx_runtime PUBLIC Voryx::rhi_api PRIVATE voryx_backend_contract)
target_compile_definitions(voryx_runtime PRIVATE
    VORYX_HAS_NULL=$<BOOL:${VORYX_ENABLE_NULL}>
    VORYX_HAS_VULKAN=$<BOOL:${VORYX_ENABLE_VULKAN}>
    VORYX_HAS_WIN32_PRESENTER=$<BOOL:${VORYX_ENABLE_WIN32_WSI}>
    VORYX_HAS_XLIB_PRESENTER=$<BOOL:${VORYX_ENABLE_X11}>
    VORYX_HAS_WAYLAND_PRESENTER=$<BOOL:${VORYX_ENABLE_WAYLAND}>
    VORYX_PLATFORM_WINDOWS=$<BOOL:${WIN32}>
    VORYX_PLATFORM_LINUX=$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>)
if(VORYX_ENABLE_NULL)
    target_link_libraries(voryx_runtime PRIVATE voryx_backend_null)
endif()
if(VORYX_ENABLE_VULKAN)
    target_link_libraries(voryx_runtime PRIVATE voryx_backend_vulkan)
endif()
set_target_properties(voryx_runtime PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME runtime)
voryx_enable_warnings(voryx_runtime)

target_include_directories(voryx_runtime PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
