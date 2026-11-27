include(CMakePackageConfigHelpers)

foreach(directory CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_BINDIR CMAKE_INSTALL_DATADIR)
    if(IS_ABSOLUTE "${${directory}}")
        message(FATAL_ERROR "Relocatable VORYX SDK requires a relative ${directory}")
    endif()
endforeach()

# 静态库的内部链接依赖也需要导出，但不安装内部头文件或开发工具。
set(voryx_install_targets voryx_core voryx_rhi_api voryx_rhi voryx_runtime voryx_backend_contract)
if(VORYX_ENABLE_NULL)
    list(APPEND voryx_install_targets voryx_backend_null)
endif()
if(VORYX_ENABLE_VULKAN)
    list(APPEND voryx_install_targets voryx_backend_vulkan)
endif()
if(VORYX_ENABLE_OPENGL)
    list(APPEND voryx_install_targets voryx_backend_opengl)
endif()
install(TARGETS ${voryx_install_targets}
    EXPORT VoryxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/voryx"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.hpp")

set(voryx_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Voryx")
configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/VoryxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/VoryxConfig.cmake"
    INSTALL_DESTINATION "${voryx_package_dir}"
    PATH_VARS CMAKE_INSTALL_DATADIR)
# 首版验证期只接受准确版本，避免对尚未冻结的接口作兼容承诺。
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/VoryxConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY ExactVersion)
install(EXPORT VoryxTargets FILE VoryxTargets.cmake NAMESPACE Voryx::
    DESTINATION "${voryx_package_dir}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VoryxConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VoryxConfigVersion.cmake"
    DESTINATION "${voryx_package_dir}")
if(VORYX_ENABLE_NULL)
    install(FILES "${PROJECT_SOURCE_DIR}/samples/null_buffers/main.cpp"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/null_buffers")
    install(FILES "${PROJECT_SOURCE_DIR}/samples/null_buffers/standalone.CMakeLists.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/null_buffers"
        RENAME CMakeLists.txt)
endif()
if(VORYX_ENABLE_OPENGL)
    foreach(example opengl_clear opengl_triangle)
        set(destination "${CMAKE_INSTALL_DATADIR}/Voryx/examples/${example}")
        install(FILES "${PROJECT_SOURCE_DIR}/samples/${example}/main.cpp"
            "${PROJECT_SOURCE_DIR}/samples/common/opengl_sample.hpp"
            DESTINATION "${destination}")
        install(FILES "${PROJECT_SOURCE_DIR}/samples/${example}/standalone.CMakeLists.txt"
            DESTINATION "${destination}" RENAME CMakeLists.txt)
    endforeach()
    install(FILES "${PROJECT_SOURCE_DIR}/assets/shaders/opengl/triangle_glsl.hpp"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/opengl_triangle")
endif()
if(VORYX_ENABLE_WIN32_WSI)
    install(FILES "${PROJECT_SOURCE_DIR}/samples/win32_clear/main.cpp"
        "${PROJECT_SOURCE_DIR}/samples/common/triangle_sample.hpp"
        "${PROJECT_SOURCE_DIR}/samples/common/window_frame_sample.hpp"
        "${PROJECT_SOURCE_DIR}/assets/shaders/generated/triangle_spirv.hpp"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/win32_clear")
    install(FILES "${PROJECT_SOURCE_DIR}/samples/win32_clear/standalone.CMakeLists.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/win32_clear"
        RENAME CMakeLists.txt)
endif()
if(VORYX_ENABLE_X11)
    install(FILES "${PROJECT_SOURCE_DIR}/samples/xlib_clear/main.cpp"
        "${PROJECT_SOURCE_DIR}/samples/common/triangle_sample.hpp"
        "${PROJECT_SOURCE_DIR}/samples/common/window_frame_sample.hpp"
        "${PROJECT_SOURCE_DIR}/assets/shaders/generated/triangle_spirv.hpp"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/xlib_clear")
    install(FILES "${PROJECT_SOURCE_DIR}/samples/xlib_clear/standalone.CMakeLists.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/xlib_clear"
        RENAME CMakeLists.txt)
endif()
if(VORYX_ENABLE_WAYLAND)
    install(FILES "${PROJECT_SOURCE_DIR}/samples/wayland_clear/main.cpp"
        "${PROJECT_SOURCE_DIR}/samples/common/triangle_sample.hpp"
        "${PROJECT_SOURCE_DIR}/samples/common/window_frame_sample.hpp"
        "${PROJECT_SOURCE_DIR}/assets/shaders/generated/triangle_spirv.hpp"
        "${PROJECT_SOURCE_DIR}/samples/wayland_clear/wayland_sample.cmake"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/wayland_clear")
    install(FILES "${PROJECT_SOURCE_DIR}/samples/wayland_clear/standalone.CMakeLists.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Voryx/examples/wayland_clear"
        RENAME CMakeLists.txt)
endif()
