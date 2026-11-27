#include <voryx/runtime/device_factory.hpp>

#include <error_context.hpp>

#if VORYX_HAS_NULL
#include <null_factory.hpp>
#endif
#if VORYX_HAS_VULKAN
#include <vulkan_factory.hpp>
#endif

#if VORYX_HAS_OPENGL
#include <opengl_factory.hpp>
#endif

#include "device_identity.hpp"
#include <memory>

namespace voryx
{
namespace
{
Result<std::unique_ptr<IDevice>> Failure(ErrorCode code) noexcept
{
    return Result<std::unique_ptr<IDevice>>::Failure(
        detail::WithOperation({code, {}, {}, {}}, "CreateDevice"));
}
} // namespace

Result<std::unique_ptr<IDevice>> CreateDevice(const DeviceDesc &desc) noexcept
{
    if (desc.backend > BackendType::DX12) {
        return Failure(ErrorCode::InvalidArgument);
    }
#if !VORYX_PLATFORM_WINDOWS
    if (desc.backend == BackendType::DX12) {
        return Failure(ErrorCode::Unsupported);
    }
#endif
    const bool useNull = desc.backend == BackendType::Null;
    const bool useOpenGL = desc.backend == BackendType::OpenGL;
    const bool useVulkan = desc.backend == BackendType::Vulkan
                           || desc.backend == BackendType::Auto;
    if (!useNull && !useVulkan && !useOpenGL) {
        return Failure(ErrorCode::BackendNotBuilt);
    }
    if ((useNull && !VORYX_HAS_NULL) || (useVulkan && !VORYX_HAS_VULKAN)
        || (useOpenGL && !VORYX_HAS_OPENGL)) {
        return Failure(ErrorCode::BackendNotBuilt);
    }
    const auto identity = detail::AllocateDeviceIdentity();
    if (identity == 0) {
        return Failure(ErrorCode::InternalError);
    }
    if (useNull) {
#if VORYX_HAS_NULL
        return detail::CreateNullDevice(identity, desc.debugName);
#endif
    }
#if VORYX_HAS_OPENGL
    if (useOpenGL) {
        return detail::CreateOpenGLDevice(identity, desc);
    }
#endif
#if VORYX_HAS_VULKAN
    return detail::CreateVulkanDevice(identity, desc);
#else
    return Failure(ErrorCode::BackendNotBuilt);
#endif
}
} // namespace voryx
