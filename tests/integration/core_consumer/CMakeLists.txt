cmake_minimum_required(VERSION 3.25)
project(VoryxCoreConsumer LANGUAGES CXX)

# 独立应用接入生产库时不应依赖测试框架或质量检查工具。
set(VORYX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VORYX_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(VORYX_ENABLE_QUALITY_CHECKS OFF CACHE BOOL "" FORCE)
add_subdirectory("${VORYX_SOURCE_DIR}" voryx)
add_executable(core_consumer main.cpp)
target_link_libraries(core_consumer PRIVATE Voryx::core)
