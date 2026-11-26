find_package(Vulkan 1.3.280 REQUIRED)
add_executable(voryx_vulkan_triangle_tests triangle_pixels_test.cpp)
target_link_libraries(voryx_vulkan_triangle_tests PRIVATE voryx_backend_vulkan Vulkan::Vulkan GTest::gtest_main)
target_include_directories(voryx_vulkan_triangle_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/src/backends/vulkan" "${PROJECT_SOURCE_DIR}/src/rhi" "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
target_compile_definitions(voryx_vulkan_triangle_tests PRIVATE
    VORYX_TEST_SOFTWARE=$<BOOL:${VORYX_ALLOW_SOFTWARE_VULKAN}>)
set_target_properties(voryx_vulkan_triangle_tests PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_vulkan_triangle_tests)
include(GoogleTest)
gtest_discover_tests(voryx_vulkan_triangle_tests
    TEST_PREFIX "gpu.vulkan."
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
    PROPERTIES LABELS "gpu_vulkan" TIMEOUT 60 ENVIRONMENT "${VORYX_GPU_TEST_ENV}")
