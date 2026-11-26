# F05/F12 原生 Wayland 清屏规格

宿主持有 wl_display、已配置的 wl_surface 和窗口角色。Runtime 选择 Wayland 工厂，Vulkan 私有适配器创建表面。共享呈现器接收宿主已确认的物理尺寸，并复用原有清屏、同步及交换链恢复路径。

- `ForWayland` 借用同一连接的两个原生对象。Presenter 必须先释放。
- `SetFramebufferExtent` 保存期望像素尺寸。首帧和尺寸为零时暂停。恢复正尺寸后重建。Win32/X11 继续查询原生客户区。
- T35 在原生 Wayland socket 上验证首次 configure、最大化后的新尺寸、显式暂停、恢复和销毁，禁止依赖 X11 DISPLAY。迁移后的 SDK 示例也需独立构建并运行。
- 生成的 xdg-shell C 协议代码属于宿主示例，留在构建目录。SDK 后端仅包含 Vulkan Wayland 声明。

详见 [Wayland 接入指南](../wayland-clear.md)。
