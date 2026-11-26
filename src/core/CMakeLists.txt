add_library(voryx_core STATIC error.cpp log.cpp)
add_library(Voryx::core ALIAS voryx_core)
target_include_directories(voryx_core PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(voryx_core PUBLIC cxx_std_20)
set_target_properties(voryx_core PROPERTIES CXX_EXTENSIONS OFF EXPORT_NAME core)
voryx_enable_warnings(voryx_core)
