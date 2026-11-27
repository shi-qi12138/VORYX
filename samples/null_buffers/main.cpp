#include <voryx/runtime/device_factory.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <utility>

namespace
{
voryx::Result<void> Run(const voryx::DeviceDesc &desc)
{
    // 同一段公共 API 流程可选择 CPU 契约后端或真实图形设备。
    auto created = voryx::CreateDevice(desc);
    if (!created) {
        return voryx::Result<void>::Failure(std::move(created).GetError());
    }
    auto device = std::move(created).Value();
    std::printf("Device: %s; software=%s\n", device->GetInfo().name.c_str(),
                device->GetInfo().softwareAdapter ? "true" : "false");
    const auto usage =
        voryx::BufferUsage::CopySrc | voryx::BufferUsage::CopyDst;
    const std::array input{std::byte{1}, std::byte{2}, std::byte{3},
                           std::byte{4}};
    auto source = device->CreateBuffer({input.size(), usage, "source"}, input);
    if (!source) {
        return voryx::Result<void>::Failure(std::move(source).GetError());
    }
    auto destination =
        device->CreateBuffer({input.size(), usage, "destination"});
    if (!destination) {
        return voryx::Result<void>::Failure(std::move(destination).GetError());
    }
    auto commands = device->CreateCommandList();
    if (!commands) {
        return voryx::Result<void>::Failure(std::move(commands).GetError());
    }
    auto command = std::move(commands).Value();
    auto begin = command->Begin();
    if (!begin) {
        return voryx::Result<void>::Failure(std::move(begin).GetError());
    }
    const std::array barriers{
        voryx::BufferBarrier{source.Value(), voryx::BufferUse::Initial,
                             voryx::BufferUse::CopySrc},
        voryx::BufferBarrier{destination.Value(), voryx::BufferUse::Initial,
                             voryx::BufferUse::CopyDst}};
    auto transition = command->Barrier(barriers);
    if (!transition) {
        return voryx::Result<void>::Failure(std::move(transition).GetError());
    }
    auto copy = command->CopyBuffer(
        {source.Value(), destination.Value(), 0, 0, input.size()});
    if (!copy) {
        return voryx::Result<void>::Failure(std::move(copy).GetError());
    }
    auto end = command->End();
    if (!end) {
        return voryx::Result<void>::Failure(std::move(end).GetError());
    }
    const std::array<voryx::ICommandList *, 1> lists{command.get()};
    auto submitted = device->Submit({lists});
    if (!submitted) {
        return voryx::Result<void>::Failure(std::move(submitted).GetError());
    }
    auto wait = device->Wait(submitted.Value(), 0);
    if (!wait) {
        return voryx::Result<void>::Failure(std::move(wait).GetError());
    }
    std::array<std::byte, 4> output{};
    auto read = device->ReadBuffer(destination.Value(), 0, output);
    if (!read) {
        return voryx::Result<void>::Failure(std::move(read).GetError());
    }
    if (output != input) {
        return voryx::Result<void>::Failure({voryx::ErrorCode::InternalError,
                                             "VerifyCopy",
                                             "Byte mismatch",
                                             {}});
    }
    // 命令包装对象先于设备销毁；已提交资源由设备负责最后清理。
    return voryx::Result<void>::Success();
}
} // namespace

int main(int argc, char **argv)
{
    std::atomic<int> validationErrors{0};
    voryx::DeviceDesc desc{voryx::BackendType::Null, "Buffer copy sample"};
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--vulkan") {
            desc.backend = voryx::BackendType::Vulkan;
        } else if (argument == "--opengl") {
            desc.backend = voryx::BackendType::OpenGL;
        } else if (argument == "--validation") {
            desc.enableValidation = true;
        } else if (argument == "--allow-software") {
            desc.allowSoftwareAdapter = true;
        } else {
            std::fputs(
                "Usage: buffer_sample [--vulkan|--opengl] [--validation] "
                "[--allow-software]\n",
                stderr);
            return 2;
        }
    }
    // 回调上下文必须覆盖设备和命令对象的整个生命周期。
    desc.logUserData = &validationErrors;
    desc.logCallback = [](voryx::LogLevel level, std::string_view message,
                          void *data) noexcept {
        if (level == voryx::LogLevel::Error) {
            static_cast<std::atomic<int> *>(data)->fetch_add(1);
        }
        std::fwrite(message.data(), 1, message.size(), stderr);
        std::fputc('\n', stderr);
    };
    const auto result = Run(desc);
    if (!result) {
        const auto &error = result.GetError();
        const auto name = voryx::GetErrorCodeName(error.errorCode);
        std::fprintf(stderr, "%.*s in %s; backend code=%lld\n",
                     static_cast<int>(name.size()), name.data(),
                     error.operation.c_str(),
                     static_cast<long long>(error.backendCode.value_or(0)));
        return 1;
    }
    if (validationErrors.load() != 0) {
        std::fputs("Validation reported errors.\n", stderr);
        return 1;
    }
    std::puts("Buffer copy verified.");
    return 0;
}
