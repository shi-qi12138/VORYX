cmake_minimum_required(VERSION 3.25)
project(VoryxInstalledConsumer LANGUAGES CXX)

find_package(Voryx 0.1.0 EXACT CONFIG REQUIRED COMPONENTS core rhi_api runtime
    PATHS "${VORYX_EXPECTED_PREFIX}" NO_DEFAULT_PATH)
if(NOT Voryx_WITH_NULL)
    message(FATAL_ERROR "This consumer test requires the Null validation backend")
endif()

# 导入目标必须指向迁移后的安装目录，不能依赖原始构建或暂存路径。
string(TOUPPER "${CMAKE_BUILD_TYPE}" configuration)
set(implementation_targets Voryx::core Voryx::runtime Voryx::detail_null Voryx::detail_rhi)
if(Voryx_WITH_VULKAN)
    list(APPEND implementation_targets Voryx::detail_vulkan)
endif()
foreach(target IN LISTS implementation_targets)
    get_target_property(location ${target} "IMPORTED_LOCATION_${configuration}")
    cmake_path(IS_PREFIX VORYX_EXPECTED_PREFIX "${location}" NORMALIZE relocated)
    if(NOT relocated)
        message(FATAL_ERROR "Target ${target} did not relocate: ${location}")
    endif()
endforeach()

add_executable(sdk_consumer main.cpp)
target_link_libraries(sdk_consumer PRIVATE Voryx::runtime)
add_executable(pipeline_consumer pipeline.cpp)
target_link_libraries(pipeline_consumer PRIVATE Voryx::runtime)
