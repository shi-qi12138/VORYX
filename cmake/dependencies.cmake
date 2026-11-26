include(FetchContent)

# 固定上游版本并校验内容；归档缓存可用于离线构建。
set(VORYX_GOOGLETEST_SHA256
    "65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c")
set(VORYX_DEPENDENCY_CACHE "${PROJECT_SOURCE_DIR}/build/_downloads" CACHE PATH
    "Directory containing verified dependency archives")
set(archive "${VORYX_DEPENDENCY_CACHE}/googletest-v1.17.0.tar.gz")
if(EXISTS "${archive}")
    set(googletest_url "${archive}")
else()
    set(googletest_url
        "https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0")
endif()

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL "${googletest_url}"
    URL_HASH "SHA256=${VORYX_GOOGLETEST_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(googletest)
