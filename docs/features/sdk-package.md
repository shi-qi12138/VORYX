# F18：可安装并被独立工程消费的 SDK

状态：实现规格。交付内容为静态库与公共头，而不是依赖仓库脚本运行的应用。调用说明见 [SDK 接入](../sdk-integration.md)。

## 模块与所有权

- Core、RHI 公共契约、Runtime 与后端分别维护 target。
- 对外入口为 Voryx::core/rhi_api/runtime；内部链接目标仅供静态链接，不开放私有头。
- 用户独立拥有 main、窗口、设备和主循环，样例不包含后端实现。
- 安装库、公共头、Config/Version/Targets 与独立样例源码，不打包开发工具或 GoogleTest。

## 参数与兼容性

VORYX_INSTALL 控制安装，VORYX_ENABLE_NULL 决定是否编入 Null。安装子目录使用相对路径，prefix 支持迁移。开发包版本为 0.1.0，使用精确匹配，不同提交不承诺 ABI 兼容，头文件和库必须来自同一次构建。MSVC 自有静态库采用嵌入调试信息，不依赖构建目录的编译器 PDB。

## 验收

integration.installed_sdk 完成安装、复制前缀、导入位置校验，只以安装目录 find_package 并编译运行包内示例；不能包含 src 或回退构建树。该验收证明 SDK 接入链路，不证明 GPU/WSI、部署兼容矩阵或稳定 ABI。
