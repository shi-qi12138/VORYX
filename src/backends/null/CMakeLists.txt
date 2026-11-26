add_library(voryx_backend_null STATIC null_backend.cpp)
target_include_directories(voryx_backend_null PRIVATE "${PROJECT_SOURCE_DIR}/src/rhi")
target_link_libraries(voryx_backend_null PUBLIC Voryx::rhi_api
    PRIVATE voryx_backend_contract voryx_rhi)
set_target_properties(voryx_backend_null PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME detail_null)
voryx_enable_warnings(voryx_backend_null)

target_include_directories(voryx_backend_null PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
