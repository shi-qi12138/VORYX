function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 90)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Consumer verification failed (${result})\n${output}\n${errors}")
    endif()
endfunction()

run_checked("${CMAKE_COMMAND}"
    -S "${SOURCE_ROOT}/tests/integration/core_consumer" -B "${BINARY_ROOT}"
    -G Ninja "-DVORYX_SOURCE_DIR=${SOURCE_ROOT}"
    "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIGURATION}" "-DCMAKE_MAKE_PROGRAM=${NINJA}")
run_checked("${CMAKE_COMMAND}" --build "${BINARY_ROOT}" --parallel 2)
if(CMAKE_HOST_WIN32)
    run_checked("${BINARY_ROOT}/core_consumer.exe")
else()
    run_checked("${BINARY_ROOT}/core_consumer")
endif()
