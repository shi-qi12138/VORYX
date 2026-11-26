add_library(voryx_rhi_api INTERFACE)
add_library(Voryx::rhi_api ALIAS voryx_rhi_api)
target_include_directories(voryx_rhi_api INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(voryx_rhi_api PROPERTIES EXPORT_NAME rhi_api)
target_link_libraries(voryx_rhi_api INTERFACE Voryx::core)

find_package(Threads REQUIRED)
add_library(voryx_rhi STATIC buffer_state.cpp buffer_command_list.cpp buffer_device.cpp pipeline_device.cpp pipeline_validation.cpp texture_device.cpp draw_command_list.cpp binding_device.cpp frame_device.cpp submission_state.cpp)
target_link_libraries(voryx_rhi PUBLIC Voryx::rhi_api PRIVATE Threads::Threads)
target_include_directories(voryx_rhi PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
set_target_properties(voryx_rhi PROPERTIES EXPORT_NAME detail_rhi CXX_EXTENSIONS OFF)
voryx_enable_warnings(voryx_rhi)
