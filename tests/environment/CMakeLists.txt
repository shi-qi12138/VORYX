add_executable(voryx_environment_tests toolchain_test.cpp)
target_compile_features(voryx_environment_tests PRIVATE cxx_std_20)
set_target_properties(voryx_environment_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(voryx_environment_tests PRIVATE GTest::gtest_main)
voryx_enable_warnings(voryx_environment_tests)

include(GoogleTest)
gtest_discover_tests(voryx_environment_tests
    TEST_PREFIX "environment."
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
    PROPERTIES LABELS "cpu" TIMEOUT 15
)
