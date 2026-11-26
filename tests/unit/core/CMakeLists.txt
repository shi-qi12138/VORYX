add_executable(voryx_core_tests error_test.cpp result_test.cpp log_test.cpp)
target_link_libraries(voryx_core_tests PRIVATE Voryx::core GTest::gtest_main)
set_target_properties(voryx_core_tests PROPERTIES CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_core_tests)
include(GoogleTest)
gtest_discover_tests(voryx_core_tests
    TEST_PREFIX "unit.core."
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/reports"
    PROPERTIES LABELS "unit" TIMEOUT 20
)
