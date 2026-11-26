#pragma once

#include "buffer_backend.hpp"

namespace voryx::rhi_detail
{
struct Buffer;
struct Texture;
struct Pipeline;
struct BindingSet;

struct Operation
{
    using Kind = OperationKind;
    Kind kind = Kind::Barrier;
    std::shared_ptr<Buffer> src;
    std::shared_ptr<Buffer> dst;
    BufferUse before = BufferUse::Initial;
    BufferUse after = BufferUse::Initial;
    std::uint64_t srcOffset = 0;
    std::uint64_t dstOffset = 0;
    std::uint64_t size = 0;
    std::shared_ptr<Texture> texture{};
    std::shared_ptr<Texture> depth{};
    std::shared_ptr<Pipeline> pipeline{};
    std::shared_ptr<BindingSet> bindingSet{};
    BackendOperation native{};

    void ReleaseRecordingRefs() const noexcept;
    void TransferToSubmission(std::uint64_t serial) const noexcept;
    void ReleaseInFlightRefs() const noexcept;
};

} // namespace voryx::rhi_detail
