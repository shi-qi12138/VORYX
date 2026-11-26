add_library(voryx_backend_contract INTERFACE)
target_include_directories(voryx_backend_contract INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
set_target_properties(voryx_backend_contract PROPERTIES EXPORT_NAME detail_backend_contract)
target_link_libraries(voryx_backend_contract INTERFACE Voryx::rhi_api)
