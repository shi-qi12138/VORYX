find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

set(voryx_llvm_hints)
if(CMAKE_HOST_WIN32)
    # VS 生成器选择的实例和 VS 开发终端均可定位自带的 LLVM。
    string(REGEX REPLACE ",.*$" "" voryx_vs_instance "${CMAKE_GENERATOR_INSTANCE}")
    foreach(voryx_vs_root IN ITEMS "${voryx_vs_instance}" "$ENV{VSINSTALLDIR}")
        if(IS_ABSOLUTE "${voryx_vs_root}")
            list(APPEND voryx_llvm_hints "${voryx_vs_root}/VC/Tools/Llvm/x64/bin"
                "${voryx_vs_root}/VC/Tools/Llvm/bin")
        endif()
    endforeach()
endif()

find_program(VORYX_CLANG_FORMAT NAMES clang-format clang-format-21 HINTS ${voryx_llvm_hints})
find_program(VORYX_CLANG_TIDY NAMES clang-tidy clang-tidy-21 HINTS ${voryx_llvm_hints})
if(NOT VORYX_CLANG_FORMAT OR NOT VORYX_CLANG_TIDY)
    message(FATAL_ERROR
        "VORYX quality checks require clang-format and clang-tidy. "
        "Install the LLVM tools or set VORYX_CLANG_FORMAT and VORYX_CLANG_TIDY to their executable paths. "
        "See docs/environment.md for Windows setup.")
endif()

set(VORYX_QUALITY_SCRIPT "${PROJECT_SOURCE_DIR}/tools/quality/check.py")
set(voryx_braces_command "${Python3_EXECUTABLE}" "${VORYX_QUALITY_SCRIPT}" braces
    --tool "${VORYX_CLANG_TIDY}" --build-dir "${CMAKE_BINARY_DIR}")
if(CMAKE_GENERATOR MATCHES "^Visual Studio ")
    # MSBuild 生成器不导出编译数据库，按当前配置单独生成 Ninja 编译命令。
    set(voryx_braces_command "${Python3_EXECUTABLE}" -B -m tools.quality.vs_braces
        --cmake "${CMAKE_COMMAND}" --instance "${voryx_vs_instance}"
        --platform "${CMAKE_VS_PLATFORM_NAME}" "--sdk=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
        --config "$<CONFIG>" --compiler "${CMAKE_CXX_COMPILER}"
        --tool "${VORYX_CLANG_TIDY}" --build-dir "${CMAKE_BINARY_DIR}")
    foreach(setting VORYX_BUILD_TESTS VORYX_BUILD_SAMPLES VORYX_BUILD_GPU_TESTS
            VORYX_ENABLE_NULL VORYX_ENABLE_VULKAN VORYX_ENABLE_WIN32_WSI
            VORYX_DEPENDENCY_CACHE Vulkan_INCLUDE_DIR Vulkan_LIBRARY)
        if(DEFINED ${setting})
            list(APPEND voryx_braces_command "--define=${setting}=${${setting}}")
        endif()
    endforeach()
endif()

add_custom_target(voryx_format_check
    COMMAND "${Python3_EXECUTABLE}" "${VORYX_QUALITY_SCRIPT}" format
        --tool "${VORYX_CLANG_FORMAT}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM
)
add_custom_target(voryx_braces_check
    COMMAND ${voryx_braces_command}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM
)
if(TARGET voryx_wayland_clear_xdg_protocol)
    add_dependencies(voryx_braces_check voryx_wayland_clear_xdg_protocol)
endif()
add_custom_target(voryx_quality DEPENDS voryx_format_check voryx_braces_check)

function(voryx_register_quality_tests)
    add_test(NAME quality.format
        COMMAND "${Python3_EXECUTABLE}" "${VORYX_QUALITY_SCRIPT}" format
            --tool "${VORYX_CLANG_FORMAT}")
    add_test(NAME quality.braces
        COMMAND ${voryx_braces_command})
    set_tests_properties(quality.braces PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_test(NAME quality.braces_enforcement
        COMMAND "${Python3_EXECUTABLE}" "${VORYX_QUALITY_SCRIPT}" selftest
            --tool "${VORYX_CLANG_TIDY}" --build-dir "${CMAKE_BINARY_DIR}")
    set_tests_properties(quality.format quality.braces quality.braces_enforcement
        PROPERTIES LABELS "quality;environment" TIMEOUT 60)
    set_tests_properties(quality.braces PROPERTIES TIMEOUT 180)
endfunction()
