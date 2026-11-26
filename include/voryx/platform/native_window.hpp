#pragma once

#include <cstdint>

namespace voryx
{
struct NativeWindowDesc
{
    enum class Kind : std::uint8_t
    {
        Invalid,
        Win32,
        Xlib,
        Wayland,
    };

    /// 借用本进程的 HWND 和 HINSTANCE 值，不接管窗口。
    static NativeWindowDesc ForWindows(std::uintptr_t window,
                                       std::uintptr_t instance) noexcept
    {
        return {Kind::Win32, window, instance};
    }

    /// 借用 Display 地址和 X11 Window ID，连接与窗口均由宿主持有。
    static NativeWindowDesc ForXlib(std::uintptr_t display,
                                    std::uint64_t window) noexcept
    {
        return {Kind::Xlib, window, 0, display};
    }

    /// 借用同一连接上的 wl_display 与已经配置的 wl_surface。
    static NativeWindowDesc ForWayland(std::uintptr_t display,
                                       std::uintptr_t surface) noexcept
    {
        return {Kind::Wayland, surface, 0, display};
    }

    Kind kind = Kind::Invalid;
    std::uint64_t window = 0;
    std::uintptr_t instance = 0;
    std::uintptr_t display = 0;
};

} // namespace voryx
