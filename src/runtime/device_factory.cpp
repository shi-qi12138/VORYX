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

#include "backend_selection.hpp"
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
Result<std::unique_ptr<IDevice>>
CreateExplicitDevice(const DeviceDesc &desc) noexcept
{
    const auto backend = desc.backend;
#if !VORYX_PLATFORM_WINDOWS
    if (backend == BackendType::DX12) {
        return Failure(ErrorCode::Unsupported);
    }
#endif
    const bool built = (backend == BackendType::Null && VORYX_HAS_NULL)
                       || (backend == BackendType::Vulkan && VORYX_HAS_VULKAN)
                       || (backend == BackendType::OpenGL && VORYX_HAS_OPENGL);
    if (!built) {
        return Failure(ErrorCode::BackendNotBuilt);
    }
    const auto identity = detail::AllocateDeviceIdentity();
    if (identity == 0) {
        return Failure(ErrorCode::InternalError);
    }
#if VORYX_HAS_NULL
    if (backend == BackendType::Null) {
        return detail::CreateNullDevice(identity, desc.debugName);
    }
#endif
#if VORYX_HAS_OPENGL
    if (backend == BackendType::OpenGL) {
        return detail::CreateOpenGLDevice(identity, desc);
    }
#endif
#if VORYX_HAS_VULKAN
    return detail::CreateVulkanDevice(identity, desc);
#else
    return Failure(ErrorCode::BackendNotBuilt);
#endif
}
} // namespace

Result<std::unique_ptr<IDevice>> CreateDevice(const DeviceDesc &desc) noexcept
{
    return detail::SelectBackend<std::unique_ptr<IDevice>>(
        desc.backend, "CreateDevice", [&](BackendType backend) {
            auto candidate = desc;
            candidate.backend = backend;
            return CreateExplicitDevice(candidate);
        });
}
} // namespace voryx
