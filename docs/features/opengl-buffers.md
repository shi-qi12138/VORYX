# OpenGL 无窗口缓冲规格

关联 F11、F06、F09、F18，目标契约版本 `0.1.0-draft.22`。

- 通过 `CreateDevice` 显式创建桌面 OpenGL 3.3 Core 设备。Windows 使用私有 WGL 隐藏窗口，Linux 使用桌面 OpenGL EGL 1.5 和 Mesa 无窗口平台。上下文与函数加载封装在后端私有模块。
- 复用 RHI 的缓冲句柄、范围、用途状态、命令录制、整批校验和设备线程限制。原生执行包含 OpenGL 缓冲分配、主机读写和复制，默认为同步完成。
- 浏览设备能力可识别本批支持范围。窗口呈现、绘制、验证层模式、纹理和异步提交单独验收。
- 验证覆盖共用缓冲契约、两个设备上下文的隔离与销毁、SDK 安装迁移后的同一工作流，以及 Windows 和 Linux 实际 OpenGL 驱动。

接入以实际 [OpenGL 指南](../opengl-backend.md) 和 [环境报告](../environment.md) 为准。
