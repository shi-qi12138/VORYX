find_package(PkgConfig REQUIRED)
pkg_check_modules(VORYX_WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(wayland_protocol_dir wayland-protocols pkgdatadir)
find_program(VORYX_WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)
set(xdg_protocol "${wayland_protocol_dir}/stable/xdg-shell/xdg-shell.xml")
if(NOT EXISTS "${xdg_protocol}")
    message(FATAL_ERROR "Install wayland-protocols to build the Wayland host sample.")
endif()

function(voryx_add_wayland_sample target)
    set(protocol_header "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client.h")
    set(protocol_source "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
    # 协议生成产物保持在构建目录，单独编译为 C。
    add_custom_command(OUTPUT "${protocol_header}" "${protocol_source}"
        COMMAND "${VORYX_WAYLAND_SCANNER}" client-header "${xdg_protocol}" "${protocol_header}"
        COMMAND "${VORYX_WAYLAND_SCANNER}" private-code "${xdg_protocol}" "${protocol_source}"
        DEPENDS "${xdg_protocol}" VERBATIM)
    add_library(${target}_xdg_protocol STATIC "${protocol_source}" "${protocol_header}")
    target_link_libraries(${target}_xdg_protocol PRIVATE PkgConfig::VORYX_WAYLAND_CLIENT)
    add_executable(${target} main.cpp)
    add_dependencies(${target} ${target}_xdg_protocol)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${target} PRIVATE Voryx::runtime
        ${target}_xdg_protocol PkgConfig::VORYX_WAYLAND_CLIENT)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()
