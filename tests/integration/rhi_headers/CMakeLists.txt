# 每个已公开的 RHI/工厂头分别编译，防止依赖调用方的包含顺序。
add_library(voryx_rhi_headers_check OBJECT)
foreach(header platform/native_window rhi/types rhi/command_list rhi/device
        rhi/texture rhi/pipeline rhi/binding rhi/presenter runtime/device_factory runtime/presenter)
    string(REPLACE "/" "_" name "${header}")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${name}_header.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <voryx/${header}.hpp>\n")
    target_sources(voryx_rhi_headers_check PRIVATE "${source}")
endforeach()
target_link_libraries(voryx_rhi_headers_check PRIVATE Voryx::rhi_api)
set_target_properties(voryx_rhi_headers_check PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_rhi_headers_check)
