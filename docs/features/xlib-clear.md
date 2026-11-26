# F05/F12 X11 清屏规格

Xlib 适配器接入宿主持有的 Display 和 Window，仅负责 Vulkan Xlib Surface 与客户区查询。Runtime 根据窗口标签和编译选项选择工厂。Vulkan 共享模块统一处理呈现能力探测、交换链、清屏和同步。非 Linux 的 Xlib 请求返回 SurfaceIncompatible，Linux 中关闭适配器时返回 BackendNotBuilt。

- 复用 Win32 已有的同步 FIFO、双 fence 回收与失败语义。
- X11 窗口解除映射或父窗口不可见时暂停，重新映射后按需重建。尺寸变化时重建。
- T34 在 X11 显示执行多帧呈现、缩放、解除映射及恢复。安装测试从迁移前缀查找包并独立构建窗口示例。
- X11 依赖按构建开关传递给静态 SDK 的链接消费端。软件 ICD 与原生 Linux GPU 分别记录。

实际用法见 [X11 清屏指南](../xlib-clear.md)。
