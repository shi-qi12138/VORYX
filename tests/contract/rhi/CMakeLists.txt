add_executable(voryx_rhi_contract_tests buffer_test.cpp command_test.cpp factory_test.cpp pipeline_test.cpp draw_test.cpp binding_test.cpp async_test.cpp async_state_test.cpp)
target_link_libraries(voryx_rhi_contract_tests PRIVATE Voryx::runtime GTest::gtest_main)
target_include_directories(voryx_rhi_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/src/rhi" "${PROJECT_SOURCE_DIR}/src/core")
target_compile_definitions(voryx_rhi_contract_tests PRIVATE VORYX_TEST_VULKAN=0 VORYX_TEST_SOFTWARE=0
    VORYX_TEST_HAS_VULKAN=$<BOOL:${VORYX_ENABLE_VULKAN}>)
set_target_properties(voryx_rhi_contract_tests PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_rhi_contract_tests)
target_include_directories(voryx_rhi_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
include(GoogleTest)
gtest_discover_tests(voryx_rhi_contract_tests
    TEST_PREFIX "contract.null."
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
    PROPERTIES LABELS "contract_null" TIMEOUT 20)
if(VORYX_BUILD_GPU_TESTS)
    add_executable(voryx_vulkan_contract_tests buffer_test.cpp command_test.cpp pipeline_test.cpp draw_test.cpp binding_test.cpp async_test.cpp)
    target_link_libraries(voryx_vulkan_contract_tests PRIVATE Voryx::runtime GTest::gtest_main)
    target_include_directories(voryx_vulkan_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
    target_compile_definitions(voryx_vulkan_contract_tests PRIVATE VORYX_TEST_VULKAN=1
        VORYX_TEST_SOFTWARE=$<BOOL:${VORYX_ALLOW_SOFTWARE_VULKAN}>)
    target_include_directories(voryx_vulkan_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/assets/shaders/generated")
    set_target_properties(voryx_vulkan_contract_tests PROPERTIES CXX_EXTENSIONS OFF)
    voryx_enable_warnings(voryx_vulkan_contract_tests)
    gtest_discover_tests(voryx_vulkan_contract_tests
        TEST_PREFIX "contract.vulkan."
        XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
        PROPERTIES LABELS "gpu_vulkan" TIMEOUT 60 ENVIRONMENT "${VORYX_GPU_TEST_ENV}")
endif()
