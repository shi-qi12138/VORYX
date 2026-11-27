#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace voryx::opengl_backend
{
using Proc = void (*)();

/// 独立拥有上下文。所有入口只在创建设备的线程调用。
class Context
{
public:
    Context();
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    void Initialize();
    [[nodiscard]] Proc Resolve(const char *name) const noexcept;
    [[nodiscard]] bool SoftwarePixelFormat() const noexcept;

    class Current
    {
    public:
        explicit Current(Context &context);
        ~Current();
        Current(const Current &) = delete;
        Current &operator=(const Current &) = delete;

    private:
#if defined(_WIN32)
        std::array<void *, 2> previous{};
#else
        Context &context;
        std::array<void *, 4> previous{};
        std::uint32_t previousApi = 0;
#endif
    };

private:
    struct Native;
    std::unique_ptr<Native> native;
};
} // namespace voryx::opengl_backend
