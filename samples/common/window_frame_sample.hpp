#pragma once

#include "triangle_spirv.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <voryx/rhi/device.hpp>
#include <voryx/rhi/presenter.hpp>

namespace voryx::sample
{
namespace frame_sample
{
template<typename T>
T Take(Result<T> result)
{
    if (!result) {
        const auto &error = result.GetError();
        throw std::runtime_error(
            error.operation + ": "
            + std::string(GetErrorCodeName(error.errorCode)));
    }
    return std::move(result).Value();
}

template<typename T>
void Expect(Result<T> result, ErrorCode code, const char *caseName)
{
    if (result || result.GetError().errorCode != code) {
        throw std::runtime_error(std::string(caseName) + " expected "
                                 + std::string(GetErrorCodeName(code)));
    }
}

inline FrameInfo Acquire(IFramePresenter &presenter)
{
    for (int attempt = 0; attempt < 8; ++attempt) {
        auto result = presenter.AcquireFrame();
        if (result) {
            return result.Value();
        }
        const auto code = result.GetError().errorCode;
        if (code != ErrorCode::Timeout && code != ErrorCode::NotReady
            && code != ErrorCode::SwapchainOutOfDate) {
            return Take(std::move(result));
        }
    }
    throw std::runtime_error("Could not acquire a window frame");
}

struct WindowGeometry
{
    explicit WindowGeometry(IDevice &device, const FrameInfo &frame)
    {
        constexpr std::array slots{
            BindingLayoutEntry{3, BindingType::UniformBuffer,
                               BindingVisibility::Vertex},
            BindingLayoutEntry{7, BindingType::SampledTexture,
                               BindingVisibility::Fragment},
            BindingLayoutEntry{11, BindingType::Sampler,
                               BindingVisibility::Fragment},
        };
        layout = Take(device.CreateBindingLayout({slots}));
        constexpr std::array inputs{
            VertexAttribute{0, VertexFormat::Float3, 0},
            VertexAttribute{1, VertexFormat::Float2, 12}};
        GraphicsPipelineDesc desc{};
        desc.vertexSpirv = sample_data::resourceDepthVertexSpirv;
        desc.fragmentSpirv = sample_data::resourceFragmentSpirv;
        desc.vertexStrideBytes = 20;
        desc.vertexAttributes = inputs;
        desc.colorFormat = frame.colorFormat;
        desc.bindingLayout = layout;
        desc.depthTest = true;
        desc.depthWrite = true;
        pipeline = Take(device.CreateGraphicsPipeline(desc));
        depth = Take(device.CreateTexture({frame.extent, Format::D32_Float,
                                           TextureUsage::DepthAttachment,
                                           "frame depth"}));
        constexpr std::array<std::uint8_t, 16> pixels{
            255, 40, 20,  255, 0,   220, 30, 255,
            10,  20, 240, 255, 255, 200, 0,  255,
        };
        texture = Take(device.CreateTexture2D(
            {{2, 2}, 0, std::as_bytes(std::span(pixels))}));
        sampler = Take(device.CreateSampler(
            {TextureFilter::Nearest, TextureFilter::Nearest}));
        constexpr std::array transform{0.65f, 0.65f, 0.1f, 0.0f};
        uniform = Take(device.CreateBuffer(
            {sizeof(transform), BufferUsage::Constant | BufferUsage::CopyDst,
             "frame transform"},
            std::as_bytes(std::span(transform))));
        const std::array entries{
            DeviceBindingSetEntry{
                3, UniformBufferBinding{uniform, 0, sizeof(transform)}},
            DeviceBindingSetEntry{7, texture},
            DeviceBindingSetEntry{11, sampler},
        };
        set = Take(device.CreateBindingSet({layout, entries}));
        constexpr std::array<std::array<float, 5>, 4> vertices{{
            {-1, -1, 0.2f, 0, 0},
            {1, -1, 0.2f, 1, 0},
            {-1, 1, 0.2f, 0, 1},
            {1, 1, 0.2f, 1, 1},
        }};
        constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 1, 3, 2};
        vertex = Take(device.CreateBuffer(
            {sizeof(vertices), BufferUsage::Vertex | BufferUsage::CopyDst,
             "frame vertices"},
            std::as_bytes(std::span(vertices))));
        index = Take(device.CreateBuffer(
            {sizeof(indices), BufferUsage::Index | BufferUsage::CopyDst,
             "frame indices"},
            std::as_bytes(std::span(indices))));
    }

    void Record(ICommandList &list, const FrameInfo &frame) const
    {
        Take(list.Begin());
        const std::array buffers{
            BufferBarrier{vertex, BufferUse::Initial, BufferUse::VertexRead},
            BufferBarrier{index, BufferUse::Initial, BufferUse::IndexRead},
            BufferBarrier{uniform, BufferUse::Initial, BufferUse::ConstantRead},
        };
        Take(list.Barrier(buffers));
        const std::array attachments{
            TextureBarrier{frame.color, TextureUse::Present,
                           TextureUse::ColorAttachment},
            TextureBarrier{depth, TextureUse::Initial, TextureUse::DepthWrite},
        };
        Take(list.Barrier(attachments));
        Take(list.BeginRenderPass(
            {frame.color, {0.1f, 0.15f, 0.3f, 1}, depth, 1}));
        Take(list.SetPipeline(pipeline));
        Take(list.SetBindingSet(set));
        Take(list.SetVertexBuffer(vertex));
        Take(list.SetIndexBuffer(index, IndexType::UInt16));
        Take(list.SetViewport({0, 0, static_cast<float>(frame.extent.width),
                               static_cast<float>(frame.extent.height)}));
        Take(list.SetScissor({0, 0, frame.extent.width, frame.extent.height}));
        Take(list.DrawIndexed(6));
        Take(list.EndRenderPass());
    }

    void Release(IDevice &device) const
    {
        Take(device.Destroy(set));
        Take(device.Destroy(pipeline));
        Take(device.Destroy(layout));
        Take(device.Destroy(uniform));
        Take(device.Destroy(vertex));
        Take(device.Destroy(index));
        Take(device.Destroy(sampler));
        Take(device.Destroy(texture));
        Take(device.Destroy(depth));
    }

    DeviceBindingLayoutHandle layout;
    DeviceBindingSetHandle set;
    DeviceSamplerHandle sampler;
    PipelineHandle pipeline;
    TextureHandle depth;
    TextureHandle texture;
    BufferHandle uniform;
    BufferHandle vertex;
    BufferHandle index;
};

inline void ReturnToPresent(ICommandList &list, TextureHandle color)
{
    const std::array barriers{TextureBarrier{color, TextureUse::ColorAttachment,
                                             TextureUse::Present}};
    Take(list.Barrier(barriers));
    Take(list.End());
}
inline void RecordWindowClear(ICommandList &list, TextureHandle color)
{
    Take(list.Begin());
    const std::array barrier{TextureBarrier{color, TextureUse::Present,
                                            TextureUse::ColorAttachment}};
    Take(list.Barrier(barrier));
    Take(list.BeginRenderPass({color, {0.2f, 0.3f, 0.4f, 1}, {}, 1}));
    Take(list.EndRenderPass());
    ReturnToPresent(list, color);
}

inline void VerifyAsyncWindowFrames(IFramePresenter &presenter, IDevice &device)
{
    if (device.GetCapabilities().maxPendingWindowFrames < 2) {
        throw std::runtime_error(
            "Expected two asynchronous window frame slots");
    }
    auto firstList = Take(device.CreateCommandList());
    const auto firstFrame = Acquire(presenter);
    WindowGeometry scene(device, firstFrame);
    scene.Record(*firstList, firstFrame);
    ReturnToPresent(*firstList, firstFrame.color);
    const std::array firstBatch{firstList.get()};
    const auto first =
        Take(device.Submit({firstBatch, firstFrame.token, SubmitMode::Async}));
    Expect(presenter.CancelFrame(firstFrame.token), ErrorCode::InvalidState,
           "cancel submitted async frame");
    Take(presenter.Present(firstFrame.token, first));
    firstList.reset();
    Expect(device.Destroy(firstFrame.color), ErrorCode::InvalidHandle,
           "asynchronous present consumed the color handle");

    // 无须等待上一帧。持续呈现直到两个帧槽都经历复用。
    auto latest = first;
    for (int count = 0; count < 9; ++count) {
        auto immediate = presenter.AcquireFrame(0);
        const auto frame = [&]() {
            if (!immediate) {
                const auto error = immediate.GetError().errorCode;
                if (error == ErrorCode::NotReady || error == ErrorCode::Timeout
                    || error == ErrorCode::SwapchainOutOfDate) {
                    return Acquire(presenter);
                }
            }
            return Take(std::move(immediate));
        }();
        auto list = Take(device.CreateCommandList());
        RecordWindowClear(*list, frame.color);
        const std::array batch{list.get()};
        const auto submitted =
            Take(device.Submit({batch, frame.token, SubmitMode::Async}));
        Expect(presenter.Present(frame.token, latest), ErrorCode::InvalidState,
               "previous asynchronous submission token");
        latest = submitted;
        Take(presenter.Present(frame.token, submitted));
    }
    Take(device.Wait(latest, 5'000'000'000));
    if (!Take(device.IsComplete(first))) {
        throw std::runtime_error(
            "Earlier window submission remains incomplete");
    }
    scene.Release(device);

    // 留下一帧给后续尺寸变化、旧式呈现或 Presenter 析构回收。
    const auto lastFrame = Acquire(presenter);
    auto lastList = Take(device.CreateCommandList());
    RecordWindowClear(*lastList, lastFrame.color);
    const std::array batch{lastList.get()};
    auto submitted =
        Take(device.Submit({batch, lastFrame.token, SubmitMode::Async}));
    Take(presenter.Present(lastFrame.token, submitted));
}
} // namespace frame_sample

inline bool VerifyWindowFrames(IPresenter &presenter)
{
    using namespace frame_sample;
    try {
        auto &device = presenter.GetDevice();
        auto list = Take(device.CreateCommandList());
        const auto cancelled = Acquire(presenter);
        Expect(presenter.AcquireFrame(), ErrorCode::InvalidState,
               "double acquire");
        Expect(presenter.ClearAndPresent({}), ErrorCode::InvalidState,
               "legacy while acquired");
        Expect(device.Destroy(cancelled.color), ErrorCode::Unsupported,
               "borrowed destroy");
        Take(list->Begin());
        const std::array invalidBarrier{TextureBarrier{
            cancelled.color, TextureUse::Present, TextureUse::Present}};
        Expect(list->Barrier(invalidBarrier), ErrorCode::InvalidArgument,
               "present self transition");
        const std::array barrier{TextureBarrier{
            cancelled.color, TextureUse::Present, TextureUse::ColorAttachment}};
        Take(list->Barrier(barrier));
        Expect(presenter.CancelFrame(cancelled.token), ErrorCode::ResourceBusy,
               "cancel while referenced");
        Take(list->Reset());
        Take(presenter.CancelFrame(cancelled.token));
        Expect(presenter.CancelFrame(cancelled.token), ErrorCode::InvalidHandle,
               "cancel stale");
        Expect(device.Destroy(cancelled.color), ErrorCode::InvalidHandle,
               "cancelled image");

        const auto frame = Acquire(presenter);
        WindowGeometry scene(device, frame);
        scene.Record(*list, frame);
        Take(list->End());
        const std::array<ICommandList *, 1> batch{list.get()};
        Expect(device.Submit({batch, frame.token}), ErrorCode::InvalidState,
               "missing transition to present");
        Take(list->Reset());

        scene.Record(*list, frame);
        ReturnToPresent(*list, frame.color);
        Expect(device.Submit({batch}), ErrorCode::InvalidState,
               "missing frame token");
        Expect(device.Submit({batch, cancelled.token}),
               ErrorCode::InvalidHandle, "stale submit frame");
        auto extra = Take(device.CreateCommandList());
        Take(extra->Begin());
        const std::array extraBarrier{TextureBarrier{
            frame.color, TextureUse::Present, TextureUse::ColorAttachment}};
        Take(extra->Barrier(extraBarrier));
        ReturnToPresent(*extra, frame.color);
        const std::array<ICommandList *, 2> invalidBatch{list.get(),
                                                         extra.get()};
        Expect(device.Submit({invalidBatch, frame.token}),
               ErrorCode::InvalidState, "use frame after returning to present");
        Take(extra->Reset());
        const auto submitted = Take(device.Submit({batch, frame.token}));
        Expect(device.Submit({batch, frame.token}), ErrorCode::InvalidState,
               "resubmit frame");
        Expect(presenter.CancelFrame(frame.token), ErrorCode::InvalidState,
               "cancel submitted frame");

        // 同设备的离屏提交从不同序号返回，不能冒充本帧的提交。
        Take(list->Reset());
        Take(list->Begin());
        Take(list->End());
        const auto unrelated = Take(device.Submit({batch}));
        Expect(presenter.Present(frame.token, unrelated),
               ErrorCode::InvalidState, "wrong submission");
        Take(presenter.Present(frame.token, submitted));
        Expect(presenter.Present(frame.token, submitted),
               ErrorCode::InvalidHandle, "present consumed frame");
        Expect(device.Destroy(frame.color), ErrorCode::InvalidHandle,
               "presented image");
        scene.Release(device);
        VerifyAsyncWindowFrames(presenter, device);
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Window device frame verification failed: %s\n",
                     error.what());
        return false;
    }
}

inline bool VerifyResizedAsyncFrame(IFramePresenter &presenter,
                                    Extent2D expected)
{
    using namespace frame_sample;
    try {
        auto &device = presenter.GetDevice();
        for (int attempt = 0; attempt < 4; ++attempt) {
            const auto frame = Acquire(presenter);
            if (frame.extent.width != expected.width
                || frame.extent.height != expected.height) {
                throw std::runtime_error("Rebuilt swapchain extent mismatch");
            }
            auto list = Take(device.CreateCommandList());
            RecordWindowClear(*list, frame.color);
            const std::array batch{list.get()};
            const auto token =
                Take(device.Submit({batch, frame.token, SubmitMode::Async}));
            auto shown = presenter.Present(frame.token, token);
            if (shown) {
                return true;
            }
            if (shown.GetError().errorCode != ErrorCode::SwapchainOutOfDate) {
                Take(std::move(shown));
            }
        }
        throw std::runtime_error("Rebuilt swapchain could not present");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "In-flight resize verification failed: %s\n",
                     error.what());
        return false;
    }
}

inline bool VerifyWindowShutdown(IFramePresenter &presenter)
{
    using namespace frame_sample;
    try {
        bool wrongThreadRejected = false;
        std::thread other([&]() {
            const auto result = presenter.Shutdown(0);
            wrongThreadRejected =
                !result
                && result.GetError().errorCode == ErrorCode::WrongThread;
        });
        other.join();
        if (!wrongThreadRejected) {
            throw std::runtime_error("Shutdown accepted the wrong thread");
        }

        // 先前已有呈现请求，再留下一个已提交但未呈现的活动帧。
        auto &device = presenter.GetDevice();
        const auto frame = Acquire(presenter);
        auto list = Take(device.CreateCommandList());
        RecordWindowClear(*list, frame.color);
        const std::array batch{list.get()};
        const auto token =
            Take(device.Submit({batch, frame.token, SubmitMode::Async}));
        Expect(presenter.Shutdown(0), ErrorCode::ResourceBusy,
               "shutdown with retained list");
        Expect(presenter.AcquireFrame(), ErrorCode::InvalidState,
               "acquire while shutting down");
        Expect(presenter.Present(frame.token, token), ErrorCode::InvalidState,
               "present while shutting down");
        Expect(device.CreateCommandList(), ErrorCode::InvalidState,
               "create while shutting down");
        Expect(list->Begin(), ErrorCode::InvalidState,
               "record while shutting down");
        const auto commandState = Take(list->GetState());
        if (commandState != CommandState::Pending
            && commandState != CommandState::Completed) {
            throw std::runtime_error(
                "Invalid command state while shutting down");
        }
        list.reset();

        const auto polled = presenter.Shutdown(0);
        if (!polled && polled.GetError().errorCode != ErrorCode::Timeout) {
            Take(polled);
        }
        Take(presenter.Shutdown(5'000'000'000));
        Take(presenter.Shutdown(0));
        Expect(device.IsComplete(token), ErrorCode::InvalidState,
               "submission token after shutdown");
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Window shutdown verification failed: %s\n",
                     error.what());
        return false;
    }
}

inline bool VerifyWindowShutdown(IPresenter &presenter)
{
    if (!VerifyWindowShutdown(static_cast<IFramePresenter &>(presenter))) {
        return false;
    }
    const auto cleared = presenter.ClearAndPresent({});
    return !cleared && cleared.GetError().errorCode == ErrorCode::InvalidState;
}

inline bool VerifyWindowFramePaused(IFramePresenter &presenter)
{
    const auto result = presenter.AcquireFrame();
    return !result
           && result.GetError().errorCode == ErrorCode::SurfaceUnavailable;
}
} // namespace voryx::sample
