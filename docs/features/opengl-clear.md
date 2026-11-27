# OpenGL 离屏清屏与像素读回

关联 F06/F09/F11/F18，目标契约版本 `0.1.0-draft.23`。

- 通过共享 RHI 创建单层、单级、单采样的 `RGBA8_UNorm` 颜色附件。纹理由 OpenGL 后端持有，同时建立与该附件绑定的帧缓冲。支持 `ColorAttachment` 及可选的 `CopySrc`。
- 录制颜色用途屏障和清除式 Pass。OpenGL 在同步提交中清除整个附件。准备读回前把颜色附件转换到 `CopySrc`。主机读回紧密排列的 RGBA 四字节像素，首行对应左上角。
- 共用命令验证与资源寿命。批次在执行前完成共享校验，OpenGL 也会整批拒绝尚未支持的命令。当前 Pass 支持一个颜色附件。深度、Shader 绘制、采样和窗口呈现后续验收。
- 共同契约验证重复清除、错误批次恢复、附件状态和句柄归属。Windows WGL 和 Linux 软件 EGL 验证实际像素。迁移安装后的独立示例清除 64×64 附件并逐像素检查。

接入见 [OpenGL 后端](../opengl-backend.md) 与 [公共 RHI 绘制接口](../rhi-draw-api.md)。后续已交付的三角形能力见 [OpenGL 绘制规格](opengl-draw.md)。
