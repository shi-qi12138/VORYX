# F05/F09/F10/F12 窗口帧首批

- `CreateFramePresenter` 返回通用 `IFramePresenter`，当前由 Vulkan 提供窗口设备和交换链。旧 `IPresenter` 派生自通用接口，继续提供固定布局示例。GetDevice 提供同一设备的借用入口，帧不占普通纹理槽位。
- 交换链图像借用至 Present 或 CancelFrame。FrameToken 和颜色句柄按设备和帧序号校验，回收后旧句柄继续失效。
- 帧提交要求包含颜色 Pass，最终回到 Present，整批预验证失败保留原状态。呈现使用关联提交与原生呈现 fence，取消归还已经获取的原生图像。
- T47 在 Win32、X11 和原生 Wayland 执行带纹理、常量、深度和索引的公开设备命令，覆盖状态、取消、暂停恢复和迁移安装。
- 从旧交换链退休到获取同步完成共用有限预算。零等待超时保留提交或已取得的图像，下次调用继续等待。
- T48 在 Null/Vulkan 验证通用离屏附件拒绝 Present，错误后仍可正常读回。
- 默认基线为同步、单活动帧。窗口双帧异步的范围和验证见 [窗口双帧提交规格](window-async-frames.md)。显式限时关闭另见 [关闭规格](window-shutdown.md)。

[实际接口](../window-frame-api.md) · [测试矩阵](../validation.md) · [环境报告](../environment.md)
