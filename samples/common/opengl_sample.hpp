#pragma once

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <voryx/runtime/device_factory.hpp>

namespace voryx::sample
{
template<typename T>
T Take(Result<T> result)
{
    if (!result) {
        const auto &error = result.GetError();
        auto message = std::string(GetErrorCodeName(error.errorCode)) + " in "
                       + error.operation;
        if (!error.message.empty()) {
            message.append(": ").append(error.message);
        }
        throw std::runtime_error(std::move(message));
    }
    return std::move(result).Value();
}

/// 两个离屏样例共用设备参数与失败处理，录制步骤保留在各自的入口。
inline int RunOpenGL(int argc, char **argv, const char *program,
                     std::string_view label, void (*run)(IDevice &))
{
    const bool allowSoftware =
        argc == 2 && std::string_view(argv[1]) == "--allow-software";
    if (argc != 1 && !allowSoftware) {
        std::fprintf(stderr, "Usage: %s [--allow-software]\n", program);
        return 1;
    }
    try {
        DeviceDesc desc{BackendType::OpenGL, label};
        desc.allowSoftwareAdapter = allowSoftware;
        auto device = Take(CreateDevice(desc));
        run(*device);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
} // namespace voryx::sample
