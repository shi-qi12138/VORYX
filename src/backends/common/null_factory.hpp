#pragma once

#include <voryx/core/result.hpp>
#include <voryx/rhi/device.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace voryx::detail
{
Result<std::unique_ptr<IDevice>>
CreateNullDevice(std::uint64_t identity, std::string_view debugName) noexcept;
} // namespace voryx::detail
