#pragma once

#include <voryx/rhi/presenter.hpp>

#include "triangle_spirv.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace voryx::sample
{
inline Result<void> ConfigureTriangle(IPresenter &presenter)
{
    // 使用局部顶点数据，验证 SDK 在调用返回前完成复制。
    const std::array vertices{
        ColorVertex{0.0f, -0.7f, 1.0f, 0.0f, 0.0f},
        ColorVertex{0.75f, 0.6f, 0.0f, 1.0f, 0.0f},
        ColorVertex{-0.75f, 0.6f, 0.0f, 0.0f, 1.0f},
    };
    return presenter.SetTriangleList({vertices,
                                      sample_data::triangleVertexSpirv,
                                      sample_data::triangleFragmentSpirv});
}

inline bool VerifyTriangleSetup(IPresenter &presenter)
{
    const auto unconfigured = presenter.DrawAndPresent({});
    if (unconfigured
        || unconfigured.GetError().errorCode != ErrorCode::InvalidState) {
        std::fputs("Expected draw before configuration to fail.\n", stderr);
        return false;
    }
    const auto configured = ConfigureTriangle(presenter);
    if (!configured) {
        std::fprintf(stderr, "Triangle setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
        return false;
    }
    // 后续有效绘制还会验证无效替换没有破坏已经上传的三角形。
    const auto invalid = presenter.SetTriangleList({});
    return !invalid
           && invalid.GetError().errorCode == ErrorCode::InvalidArgument;
}

inline Result<void> ConfigureTexture(IPresenter &presenter,
                                     std::size_t pitch = 0)
{
    const std::array vertices{
        TexturedVertex{-0.8f, -0.8f, 0.0f, 0.0f},
        TexturedVertex{0.8f, -0.8f, 1.0f, 0.0f},
        TexturedVertex{-0.8f, 0.8f, 0.0f, 1.0f},
        TexturedVertex{0.8f, -0.8f, 1.0f, 0.0f},
        TexturedVertex{0.8f, 0.8f, 1.0f, 1.0f},
        TexturedVertex{-0.8f, 0.8f, 0.0f, 1.0f},
    };
    // 使用四个不对称颜色，便于目视核对纹理的方向。
    const std::array<std::uint8_t, 16> pixels{
        255, 32, 16, 255, 16, 255, 32, 255, 32, 16, 255, 255, 255, 224, 32, 255,
    };
    return presenter.SetTexturedTriangleList(
        {vertices,
         {{2, 2}, pitch, std::as_bytes(std::span(pixels))},
         sample_data::texturedVertexSpirv,
         sample_data::texturedFragmentSpirv});
}

inline bool VerifyTextureSetup(IPresenter &presenter)
{
    const auto configured = ConfigureTexture(presenter);
    if (!configured) {
        std::fprintf(stderr, "Texture setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
        return false;
    }
    const auto invalid = ConfigureTexture(presenter, 7);
    return !invalid
           && invalid.GetError().errorCode == ErrorCode::InvalidArgument;
}

inline Result<void> ConfigureDepth(IPresenter &presenter,
                                   float nearDepth = 0.25f)
{
    // 先提交近处的红色三角形，后提交远处的绿色三角形。
    const std::array vertices{
        DepthVertex{-0.4f, -0.75f, nearDepth, 1.0f, 0.0f, 0.0f},
        DepthVertex{0.85f, -0.75f, nearDepth, 1.0f, 0.0f, 0.0f},
        DepthVertex{0.2f, 0.8f, nearDepth, 1.0f, 0.0f, 0.0f},
        DepthVertex{-0.85f, -0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
        DepthVertex{0.4f, -0.75f, 0.75f, 0.0f, 1.0f, 0.0f},
        DepthVertex{-0.2f, 0.8f, 0.75f, 0.0f, 1.0f, 0.0f},
    };
    return presenter.SetDepthTriangleList({vertices,
                                           sample_data::depthVertexSpirv,
                                           sample_data::triangleFragmentSpirv});
}

inline bool VerifyDepthSetup(IPresenter &presenter)
{
    const auto configured = ConfigureDepth(presenter);
    if (!configured) {
        std::fprintf(stderr, "Depth setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
        return false;
    }
    const auto invalid = ConfigureDepth(presenter, 1.1f);
    return !invalid
           && invalid.GetError().errorCode == ErrorCode::InvalidArgument;
}

inline constexpr std::array<std::uint32_t, 6> quadIndices{0, 1, 2, 2, 1, 3};

inline Result<void>
ConfigureIndexed(IPresenter &presenter,
                 std::span<const std::uint32_t> indices = quadIndices)
{
    // 两个三角形共用右上和左下顶点。
    const std::array vertices{
        ColorVertex{-0.75f, -0.65f, 1.0f, 0.0f, 0.0f},
        ColorVertex{0.75f, -0.65f, 0.0f, 1.0f, 0.0f},
        ColorVertex{-0.75f, 0.65f, 0.0f, 0.0f, 1.0f},
        ColorVertex{0.75f, 0.65f, 1.0f, 1.0f, 0.0f},
    };
    return presenter.SetIndexedTriangleList(
        {vertices, indices, sample_data::triangleVertexSpirv,
         sample_data::triangleFragmentSpirv});
}

inline bool VerifyIndexedSetup(IPresenter &presenter)
{
    const auto configured = ConfigureIndexed(presenter);
    if (!configured) {
        std::fprintf(stderr, "Indexed setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
        return false;
    }
    const std::array<std::uint32_t, 6> outside{0, 1, 2, 2, 1, 4};
    const auto invalidIndex = ConfigureIndexed(presenter, outside);
    const auto invalidCount =
        ConfigureIndexed(presenter, std::span(quadIndices).first(5));
    return !invalidIndex
           && invalidIndex.GetError().errorCode == ErrorCode::InvalidArgument
           && !invalidCount
           && invalidCount.GetError().errorCode == ErrorCode::InvalidArgument;
}

inline Result<void> ConfigureBoundTexture(IPresenter &presenter,
                                          bool verify = false)
{
    const std::array<std::uint8_t, 16> pixels{
        255, 32, 16, 255, 16, 255, 32, 255, 32, 16, 255, 255, 255, 224, 32, 255,
    };
    auto texture = presenter.CreateTexture2D(
        {{2, 2}, 0, std::as_bytes(std::span(pixels))});
    if (!texture) {
        return Result<void>::Failure(std::move(texture).GetError());
    }
    auto sampler = presenter.CreateSampler(
        {TextureFilter::Nearest, TextureFilter::Nearest,
         TextureAddressMode::Repeat, TextureAddressMode::Repeat});
    if (!sampler) {
        return Result<void>::Failure(std::move(sampler).GetError());
    }
    auto binding =
        presenter.CreateTextureBinding({texture.Value(), sampler.Value()});
    if (!binding) {
        return Result<void>::Failure(std::move(binding).GetError());
    }
    std::array vertices{
        TexturedVertex{-0.8f, -0.8f, 0.0f, 0.0f},
        TexturedVertex{0.8f, -0.8f, 2.0f, 0.0f},
        TexturedVertex{-0.8f, 0.8f, 0.0f, 2.0f},
        TexturedVertex{0.8f, -0.8f, 2.0f, 0.0f},
        TexturedVertex{0.8f, 0.8f, 2.0f, 2.0f},
        TexturedVertex{-0.8f, 0.8f, 0.0f, 2.0f},
    };
    const BoundTexturedTriangleListDesc draw{
        vertices, binding.Value(), sample_data::texturedVertexSpirv,
        sample_data::texturedFragmentSpirv};
    auto first = presenter.SetBoundTexturedTriangleList(draw);
    if (!first) {
        return first;
    }
    if (verify) {
        const auto busy = presenter.Destroy(binding.Value());
        if (busy || busy.GetError().errorCode != ErrorCode::ResourceBusy) {
            return Result<void>::Failure(
                {ErrorCode::InternalError, "VerifyBindingLifetime", {}, {}});
        }
        auto reset = presenter.ResetDraw();
        if (!reset) {
            return reset;
        }
        const auto unconfigured = presenter.DrawAndPresent({});
        if (unconfigured
            || unconfigured.GetError().errorCode != ErrorCode::InvalidState) {
            return Result<void>::Failure(
                {ErrorCode::InternalError, "VerifyResetDraw", {}, {}});
        }
    }
    // 修改几何后复用原绑定。窗口关闭时由 Presenter 逆依赖顺序释放资源。
    for (auto &vertex : vertices) {
        vertex.x *= 0.9f;
    }
    return presenter.SetBoundTexturedTriangleList(draw);
}

inline bool VerifyBoundTextureSetup(IPresenter &presenter)
{
    const auto configured = ConfigureBoundTexture(presenter, true);
    if (!configured) {
        std::fprintf(stderr, "Binding setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
    }
    return static_cast<bool>(configured);
}

inline Result<void> ConfigureResources(IPresenter &presenter)
{
    constexpr std::array<std::uint8_t, 16> pixels{
        255, 32, 16, 255, 16, 255, 32, 255, 32, 16, 255, 255, 255, 224, 32, 255,
    };
    auto texture = presenter.CreateTexture2D(
        {{2, 2}, 0, std::as_bytes(std::span(pixels))});
    if (!texture) {
        return Result<void>::Failure(std::move(texture).GetError());
    }
    auto sampler = presenter.CreateSampler(
        {TextureFilter::Nearest, TextureFilter::Nearest});
    if (!sampler) {
        return Result<void>::Failure(std::move(sampler).GetError());
    }
    // std140 中的单个 vec4 依次存放水平缩放、垂直缩放、水平偏移和垂直偏移。
    constexpr std::array transform{0.5f, 0.5f, 0.35f, 0.0f};
    auto uniform =
        presenter.CreateUniformBuffer(std::as_bytes(std::span(transform)));
    if (!uniform) {
        return Result<void>::Failure(std::move(uniform).GetError());
    }
    constexpr std::array slots{
        BindingLayoutEntry{3, BindingType::UniformBuffer,
                           BindingVisibility::Vertex},
        BindingLayoutEntry{7, BindingType::SampledTexture,
                           BindingVisibility::Fragment},
        BindingLayoutEntry{11, BindingType::Sampler,
                           BindingVisibility::Fragment},
    };
    auto layout = presenter.CreateBindingLayout({slots});
    if (!layout) {
        return Result<void>::Failure(std::move(layout).GetError());
    }
    const std::array entries{
        BindingSetEntry{11, sampler.Value()},
        BindingSetEntry{3, uniform.Value()},
        BindingSetEntry{7, texture.Value()},
    };
    auto bindings = presenter.CreateBindingSet({layout.Value(), entries});
    if (!bindings) {
        return Result<void>::Failure(std::move(bindings).GetError());
    }
    constexpr std::array vertices{
        TexturedVertex{-1.0f, -1.0f, 0.0f, 0.0f},
        TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
        TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
        TexturedVertex{1.0f, -1.0f, 1.0f, 0.0f},
        TexturedVertex{1.0f, 1.0f, 1.0f, 1.0f},
        TexturedVertex{-1.0f, 1.0f, 0.0f, 1.0f},
    };
    return presenter.SetResourceTriangleList(
        {vertices, bindings.Value(), sample_data::resourceVertexSpirv,
         sample_data::resourceFragmentSpirv});
}

inline bool VerifyResourceSetup(IPresenter &presenter)
{
    const auto configured = ConfigureResources(presenter);
    if (!configured) {
        std::fprintf(stderr, "Resource setup failed in %s (%lld).\n",
                     configured.GetError().operation.c_str(),
                     static_cast<long long>(
                         configured.GetError().backendCode.value_or(0)));
    }
    return static_cast<bool>(configured);
}

enum class DrawMode
{
    Clear,
    Color,
    Texture,
    Depth,
    Indexed,
    Binding,
    Resources,
};

inline Result<void> ConfigureDrawMode(IPresenter &presenter, DrawMode mode)
{
    switch (mode) {
        case DrawMode::Color:
            return ConfigureTriangle(presenter);
        case DrawMode::Texture:
            return ConfigureTexture(presenter);
        case DrawMode::Depth:
            return ConfigureDepth(presenter);
        case DrawMode::Indexed:
            return ConfigureIndexed(presenter);
        case DrawMode::Binding:
            return ConfigureBoundTexture(presenter);
        case DrawMode::Resources:
            return ConfigureResources(presenter);
        case DrawMode::Clear:
            return Result<void>::Success();
    }
    return Result<void>::Failure(
        {ErrorCode::InvalidArgument, "ConfigureDrawMode", {}, {}});
}

inline Result<void> Present(IPresenter &presenter, ClearColor color,
                            bool triangle)
{
    return triangle ? presenter.DrawAndPresent(color)
                    : presenter.ClearAndPresent(color);
}
} // namespace voryx::sample
