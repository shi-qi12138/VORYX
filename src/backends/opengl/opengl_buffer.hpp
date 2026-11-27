#pragma once

#include <buffer_backend.hpp>

namespace voryx::opengl_backend
{
struct Api;
class Context;

class Buffer final : public rhi_detail::BufferStorage
{
public:
    Buffer(Context &context, const Api &api) noexcept;
    ~Buffer() override;

    void Allocate(std::uint64_t size);
    void Bind(unsigned int target) const;
    [[nodiscard]] unsigned int Name() const noexcept;

    Result<void> Write(std::uint64_t offset, ByteSpan data) noexcept override;
    Result<void> Read(std::uint64_t offset,
                      MutableByteSpan output) noexcept override;

private:
    Context &context;
    const Api &api;
    unsigned int name = 0;
};
} // namespace voryx::opengl_backend
