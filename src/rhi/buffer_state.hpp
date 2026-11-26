#pragma once

#include "buffer_backend.hpp"
#include "buffer_guard.hpp"
#include "buffer_operation.hpp"

#include <voryx/core/result.hpp>
#include <voryx/rhi/types.hpp>

#include <error_context.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voryx::rhi_detail
{

struct Buffer
{
    BufferUsage usage = BufferUsage::None;
    BufferUse use = BufferUse::Initial;
    std::string debugName;
    std::uint64_t sizeBytes = 0;
    std::unique_ptr<BufferStorage> storage;
    std::uint64_t lastUseSubmission = 0;
    std::size_t inFlightRefs = 0;
    std::size_t recordingRefs = 0;
};

struct Slot
{
    std::uint64_t generation = 1;
    std::shared_ptr<Buffer> buffer;
};

struct BindingLayout
{
    std::vector<BindingLayoutEntry> entries;
    std::unique_ptr<BindingLayoutStorage> storage;
};

struct Pipeline
{
    std::shared_ptr<BindingLayout> bindingLayout;
    std::unique_ptr<PipelineStorage> storage;
    std::uint32_t stride = 0;
    bool hasVertices = false;
    bool depthTest = false;
    Format colorFormat = Format::RGBA8_UNorm;
    std::size_t recordingRefs = 0;
};
struct Texture
{
    Extent2D extent;
    Format format = Format::Auto;
    TextureUsage usage = TextureUsage::None;
    TextureUse use = TextureUse::Initial;
    bool windowImage = false;
    bool initialized = false;
    std::string debugName;
    std::unique_ptr<TextureStorage> storage;
    std::uint64_t lastUseSubmission = 0;
    std::size_t inFlightRefs = 0;
    std::size_t recordingRefs = 0;
};
struct BindingSet
{
    std::shared_ptr<BindingLayout> layout;
    std::vector<std::shared_ptr<Buffer>> buffers;
    std::vector<std::shared_ptr<Texture>> textures;
    std::vector<std::shared_ptr<SamplerStorage>> samplers;
    std::unique_ptr<BindingSetStorage> storage;
    std::size_t inFlightRefs = 0;

    /// 录制引用阻止主机写入，集合持有的引用只阻止销毁。
    void AddRecordingRefs() const noexcept
    {
        for (const auto &buffer : buffers) {
            ++buffer->recordingRefs;
        }
        for (const auto &texture : textures) {
            ++texture->recordingRefs;
        }
    }
    void ReleaseRecordingRefs() const noexcept
    {
        for (const auto &buffer : buffers) {
            --buffer->recordingRefs;
        }
        for (const auto &texture : textures) {
            --texture->recordingRefs;
        }
    }
};

template<typename T>
using BindingMap = std::unordered_map<std::uint64_t, std::shared_ptr<T>>;

struct TextureSlot
{
    std::uint64_t generation = 1;
    std::shared_ptr<Texture> texture;
};
struct PipelineSlot
{
    std::uint64_t generation = 1;
    std::shared_ptr<Pipeline> pipeline;
};

// 窗口借用图像保留一个独立槽位，避免占用用户纹理容量。
inline constexpr auto windowTextureSlot =
    std::numeric_limits<std::uint32_t>::max();

struct PendingSubmission
{
    std::uint64_t serial;
    std::vector<Operation> operations;
    // 倒序析构，先回收原生命令，再释放其资源引用。
    std::unique_ptr<BackendSubmission> native;
};

enum class DeviceAccess
{
    Work,
    Cleanup
};

struct State
{
    State(std::uint64_t identity, DeviceInfo info,
          DeviceCapabilities capabilities,
          std::unique_ptr<BufferBackend> backend);

    [[nodiscard]] std::optional<ErrorCode>
    CheckAccess(DeviceAccess access = DeviceAccess::Work) const noexcept;
    [[nodiscard]] Result<std::shared_ptr<Buffer>>
    Lookup(BufferHandle handle) const;
    Result<std::shared_ptr<Pipeline>> Lookup(PipelineHandle handle) const;
    Result<std::shared_ptr<Texture>> Lookup(TextureHandle handle) const;
    Result<std::shared_ptr<BindingLayout>>
    Lookup(DeviceBindingLayoutHandle handle) const;
    Result<std::shared_ptr<BindingSet>>
    Lookup(DeviceBindingSetHandle handle) const;
    [[nodiscard]] std::optional<ErrorCode>
    CheckToken(SubmissionToken token) const noexcept;

    [[nodiscard]] std::optional<ErrorCode>
    CheckFrame(FrameToken token) const noexcept;

    Result<bool> PollSubmission(std::uint64_t serial) noexcept;
    Result<void> WaitForSubmission(std::uint64_t serial,
                                   TimeoutNs timeoutNs) noexcept;
    void CompleteThrough(std::uint64_t serial) noexcept;
    void DrainSubmissions() noexcept;

    const std::uint64_t identity;
    const std::thread::id owner = std::this_thread::get_id();
    const DeviceInfo info;
    const DeviceCapabilities capabilities;
    std::unique_ptr<BufferBackend> backend;
    bool lost = false;
    bool alive = true;
    bool draining = false;
    std::size_t openCommandLists = 0;
    std::uint64_t lastSubmission = 0;
    std::uint64_t completedSubmission = 0;
    std::deque<PendingSubmission> pending;
    std::uint64_t frameSerial = 0;
    std::shared_ptr<Texture> frameTexture;
    SubmissionToken frameSubmission;
    SubmitMode frameSubmitMode = SubmitMode::Synchronous;
    std::uint64_t lastAsyncFrameSubmission = 0;
    std::vector<Slot> slots;
    std::vector<PipelineSlot> pipelines;
    std::vector<TextureSlot> textures;
    std::uint64_t nextBindingId = 1;
    BindingMap<SamplerStorage> samplers;
    BindingMap<BindingLayout> bindingLayouts;
    BindingMap<BindingSet> bindingSets;
};

bool Allows(const Texture &texture, TextureUse use) noexcept;
bool HasUsage(TextureUsage usage, TextureUsage required) noexcept;
[[nodiscard]] bool HasUsage(BufferUsage usage, BufferUsage required) noexcept;
[[nodiscard]] bool ValidUse(BufferUse use) noexcept;
[[nodiscard]] bool Allows(BufferUsage usage, BufferUse use) noexcept;
[[nodiscard]] bool InRange(std::uint64_t offset, std::uint64_t size,
                           std::uint64_t total) noexcept;
} // namespace voryx::rhi_detail
