# 缓冲数据路径示例

本示例通过公共接口执行上传、Barrier、CopyBuffer、Submit、Wait、ReadBuffer，并比较四个字节；它是 SDK 使用者，不包含后端实现。

仓库构建后运行 samples/null_buffers 下的 voryx_null_buffers 可执行文件，成功输出 Buffer copy verified. 并返回 0。VS 中选择同名启动目标即可断点调试。

安装 SDK 会附带本目录的 main.cpp 和独立 CMakeLists.txt，支持脱离仓库编译。接入步骤见 [SDK 文档](../../docs/sdk-integration.md)，接口约束见 [缓冲 RHI](../../docs/rhi-buffer-api.md)。

默认显式使用 Null；在 Vulkan 构建中传 --vulkan --validation 可执行真实 GPU 缓冲复制。软件 ICD 须额外传 --allow-software。任何错误返回 1，未知参数返回 2；日志会输出实际设备和软件标记。此示例不创建窗口，不能作为图形绘制验收。
