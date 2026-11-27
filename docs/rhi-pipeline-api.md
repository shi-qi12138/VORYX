# RHI 图形管线与顶点布局

本批提供 `IDevice::CreateGraphicsPipeline` 与 `Destroy(PipelineHandle)`。Null 保存不可变状态供 CPU 契约验证，Vulkan 同步创建原生 RenderPass、PipelineLayout 与 GraphicsPipeline。OpenGL 同步编译并链接 GLSL，创建离屏颜色绘制程序。窗口 Presenter 的现有绘制复用 Vulkan 原生实现。设备命令现已支持 Pass、管线与几何绑定、非索引与索引绘制，以及附件同步读回。 [设备绘制指南](rhi-draw-api.md)

## 创建流程

安装 SDK 后包含 `<voryx/runtime/device_factory.hpp>` 和 `<voryx/rhi/pipeline.hpp>`，链接 `Voryx::runtime`。以下片段假设 `device` 已由 `CreateDevice` 创建，两个 SPIR-V 数组由离线 Shader 工具生成。

```cpp
using namespace voryx;
constexpr std::array attributes{
    VertexAttribute{0, VertexFormat::Float2, 0},
    VertexAttribute{1, VertexFormat::Float3, 8},
};
GraphicsPipelineDesc desc{};
desc.vertexSpirv = vertexSpirv;
desc.fragmentSpirv = fragmentSpirv;
desc.vertexStrideBytes = 20;
desc.vertexAttributes = attributes;
desc.debugName = "colored mesh";

auto pipeline = device->CreateGraphicsPipeline(desc);
if (!pipeline) {
    return 1;
}
auto released = device->Destroy(pipeline.Value());
if (!released) {
    return 1;
}
```

完整可编译的设备绑定和绘制示例用于独立安装消费测试。后续应用应保存成功创建的句柄以复用管线。更新状态需要创建另一条管线。 [完整示例](../tests/integration/sdk_consumer/pipeline.cpp) · [SDK 安装](sdk-integration.md)

## Shader 输入

Vulkan 使用 `vertexSpirv` 和 `fragmentSpirv`，OpenGL 使用 `vertexGlsl` 和 `fragmentGlsl`。同一个描述可同时提供两组，每组必须有完整的顶点和片元阶段。至少提供一组。Null 按同一规则验收描述并保存输入副本。提供给其他后端的 GLSL 不由 Null 或 Vulkan 编译。

OpenGL 源码使用 `std::string_view`，每阶段最多 1 MiB，采用 `main` 入口，内嵌 NUL 返回 `InvalidArgument`。创建调用返回后即可释放源码和布局数组。编译或链接失败返回 `InvalidArgument`，在 `Error::message` 中提供阶段及驱动诊断。缺少当前后端所需的载荷返回 `Unsupported`。 [OpenGL GLSL 示例](opengl-backend.md#离屏三角形)

从 `draft.23` 升级时用相同构建的头文件和库重新编译应用，因为 `GraphicsPipelineDesc` 增加了两个字段。既有 SPIR-V 调用可沿用原字段。

## 当前输入契约

| 字段 | 已实现规则 |
| --- | --- |
| `vertexSpirv / fragmentSpirv` | 分别为顶点和片元阶段，单个 `main` 入口，SPIR-V 1.0 至 1.5，每模块最多 262144 个 32 位字。输入在返回前消费。 |
| `vertexGlsl / fragmentGlsl` | OpenGL 3.3 Core 的 GLSL 顶点及片元源码，片元颜色使用 location 0。 |
| `vertexStrideBytes` | 单顶点流，最大 2048 字节且为 4 的倍数。存在属性时必须非零。无属性时为 0，用于 Shader 自生成顶点。 |
| `vertexAttributes` | 最多 16 个不同 location，location 为 0 至 15。属性格式为 `Float2 / Float3 / Float4`，偏移为 4 的倍数，完整属性必须落在 stride 内。允许显式填充、属性顺序与内存顺序不同。 |
| `colorFormat` | `RGBA8_UNorm`、`RGBA8_sRGB` 或 `BGRA8_UNorm`，默认 `RGBA8_UNorm`。 |
| `cullMode / frontFace` | `None / Front / Back`，默认 `None`。绕序为 `CounterClockwise / Clockwise`，默认逆时针。 |
| `blendMode` | `Opaque / Alpha / PremultipliedAlpha`。默认不混合，混合采用 Add 与 OneMinusSrcAlpha 目的因子。Alpha 的 RGB 源因子为 SrcAlpha，预乘为 One，两种模式的 Alpha 源因子均为 One。 |
| `depthTest / depthWrite` | 默认均关闭，写入要求测试已启用。启用时采用 `D32_Float`，清除值和附件匹配由后续 Pass 契约负责。 |
| `depthCompare` | `Less / LessEqual / Always`，默认 `LessEqual`。 |
| `debugName` | 调用期间借用。Null 复制保存，Vulkan 首批仅用于接口描述，不承诺原生调试标签。 |

固定使用 TriangleList、单采样、单颜色附件，颜色 RGBA 全写，视口与裁剪为动态状态。OpenGL 首批支持 `RGBA8_UNorm`、`Opaque`、`CullMode::None` 与关闭深度的管线。活动 Uniform、Uniform Block 和绑定布局返回 `Unsupported`。设备管线通过 bindingLayout 接收一组常量缓冲、采样纹理与采样器资源。PushConstant 留待后续。 [设备绑定](rhi-binding-api.md)Presenter 自有的纹理布局继续通过已有接口配置，Presenter 的资源句柄与设备资源分别归属创建者。 [已有窗口绑定](vulkan-binding-layout.md)

Shader 的结构、指令边界、阶段与入口在创建前检查。应用必须使用离线工具生成并验证完整 Shader 语义，确保顶点属性、阶段输入输出和附件格式匹配。当前实现未提供完整 SPIR-V 语义验证或 Shader 反射。Null 的创建成功只证明结构契约通过。建议随离线 Shader 编译执行 SDK 中的 `spirv-val --target-env vulkan1.2`。 [SPIR-V 规范](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)

## 生命周期与错误

- 所有创建、销毁与设备析构在创建设备的线程执行，错误线程返回 `WrongThread`。
- 无效描述返回 `InvalidArgument`。后端不支持所需附件格式或混合能力时返回 `Unsupported`。
- 空或失效句柄返回 `InvalidHandle`。其他设备的句柄返回 `WrongDevice`。槽位重用会推进代数，旧句柄继续失效。
- `GetCapabilities()` 新增 `maxGraphicsPipelines`、`maxVertexAttributes` 和 `maxVertexStrideBytes`，当前上限分别为 256、16、2048。达到管线容量或原生内存不足时返回 `OutOfMemory`。
- 创建失败不发布句柄。Vulkan 的部分创建对象由 RAII 回收。设备析构统一回收未显式销毁的管线。
- `Destroy({})` 应改写为明确的 `Destroy(BufferHandle{})` 或 `Destroy(PipelineHandle{})`，避免重载歧义。新增虚函数后宿主需与 SDK 一起重新编译。

## 验收

OpenGL 用例验证 GLSL 编译诊断、链接异常、失败后创建、输入寿命、非零顶点偏移和像素方向。Null 和 Vulkan 另验证成对描述及后端载荷选择。 [OpenGL 绘制规格](features/opengl-draw.md)

Null 与 Vulkan 共用用例验证布局边界、Shader 阶段、无效输入后的继续创建、句柄代数、归属和线程。独立安装消费程序通过 `find_package` 编译并运行公共管线接口。真实 GPU 使用带填充的自定义顶点布局，让同一管线分别绘制红色和绿色几何，再读回像素检查绕序剔除、Alpha 与预乘混合。现有颜色、纹理、深度、索引、资源绑定和窗口恢复用例回归同一套原生管线实现。 [功能规格](features/rhi-pipeline.md) · [验证证据](environment.md)
