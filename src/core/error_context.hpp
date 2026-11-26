#pragma once

#include <voryx/core/error.hpp>

#include <new>
#include <string_view>

namespace voryx::detail
{
/// 补充诊断失败时保留原错误码，不让分配诊断文本掩盖真正原因。
inline Error WithOperation(Error error, std::string_view operation) noexcept
{
    if (error.operation.empty()) {
        try {
            error.operation.assign(operation);
        } catch (const std::bad_alloc &) {
            // 空诊断仍是有效错误，原始分类保持不变。
        }
    }
    return error;
}
} // namespace voryx::detail
