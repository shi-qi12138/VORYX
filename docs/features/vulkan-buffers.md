# F12 第一阶段：Vulkan 真实缓冲执行

状态：首批实现与测试已落地，契约 0.1.0-draft.5，关联 F12/T32。本阶段交付无窗口 Vulkan 1.2 设备、原生缓冲分配、提交、同步及读回；窗口清屏和三角形是后续图形阶段，不把缓冲测试当成绘制完成。

## 边界与行为

- 将已有句柄、引用、命令状态和整批校验提取到后端无关 RHI 实现；Null/Vulkan 只负责各自存储与执行，不互相依赖。
- Vulkan 实现使用 VkBuffer/VkDeviceMemory、VkCommandBuffer、队列和 fence，执行实际 vkCmdCopyBuffer；不以 CPU memcpy 替代设备复制。
- 当前采用 host-visible 内存与默认同步提交。离屏异步能力见 [异步提交规格](rhi-async-submit.md)。非 coherent 内存显式 flush/invalidate，提交末尾建立设备写入到主机读取的依赖。CopyDst 重叠写入由目标范围上的原生屏障排序，局部主机写入保留相邻的 GPU 结果。
- 原生提交成功后等待 fence，再提交逻辑状态和释放引用；无法确定执行结果时进入设备丢失状态，不假装可重试或回滚 GPU。
- 显式 validation 要求验证层存在；错误通过日志回调与原始 VkResult 保留。不支持的环境返回错误，不自动使用 Null。
- Vulkan SDK/头与链接依赖保持私有，安装包通过标准依赖查找恢复 loader 链接关系。

## 验收

原有 Null 契约必须保持通过；Vulkan 使用相同缓冲/命令用例及独立 GPU 测试标签，检查实际字节和验证层错误。区分硬件 GPU、虚拟设备与软件 ICD，软件结果不冒充硬件验收。Windows/MSVC、Linux 构建、SDK 安装消费和原生依赖分别记录。

具体运行流程见 [Vulkan 使用说明](../vulkan-backend.md)，配置/设备/驱动与报告见 [环境证据](../environment.md)。Windows 硬件与 WSL 软件均运行相同缓冲契约和安装后独立消费；非 coherent 专用场景与真实设备丢失尚未独立验证。
