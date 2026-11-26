# Vulkan 离屏异步提交

自 `0.1.0-draft.19` 起，Vulkan 设备支持显式选择离屏异步提交。C++ 应用继续通过 `Voryx::runtime` 接入，无需依赖私有后端头。Windows、Linux 采用同一套接口。 [SDK 接入](sdk-integration.md)

## 选择模式

| 项目 | 契约 |
| --- | --- |
| `SubmitDesc.commandLists` | 必填，非空，命令列表来自同一设备，均处于 `Executable`，不得重复 |
| `SubmitDesc.frame` | 离屏提交使用默认空值。窗口提交的流程和能力见 [窗口帧接口](window-frame-api.md) |
| `SubmitDesc.mode` | 默认为 `SubmitMode::Synchronous`，成功返回时已完成。使用 `SubmitMode::Async` 时，成功表示队列已接收，返回 `SubmissionToken` |
| `DeviceCapabilities.maxPendingSubmissions` | 本设备可保留的异步批次数，离屏与窗口提交共用。Vulkan 当前为 8，Null 为 0。为 0 时，请求异步返回 `Unsupported` |

同一设备的多批复制或离屏绘制可连续提交。调用方仍需为资源转换录制屏障，后续批次的前置用途以先前已接受批次的最终用途为准。共享同一资源的 GPU 工作无需在两批之间主机等待。设备、命令和完成查询仍在创建设备的线程使用。 [缓冲接口](rhi-buffer-api.md) · [绘制接口](rhi-draw-api.md) · [窗口帧接口](window-frame-api.md)

## 提交和读取结果

下面的 `command` 是通过 `device.CreateCommandList()` 创建且已经调用 `End()` 的离屏命令列表。

```cpp
#include <array>
#include <voryx/rhi/device.hpp>

voryx::Result<voryx::SubmissionToken>
QueueOffscreen(voryx::IDevice &device, voryx::ICommandList &command)
{
    const std::array batch{&command};
    const auto mode = device.GetCapabilities().maxPendingSubmissions > 0
                          ? voryx::SubmitMode::Async
                          : voryx::SubmitMode::Synchronous;
    return device.Submit({batch, {}, mode});
}
```

1. 检查 `QueueOffscreen` 返回的 `Result`，成功后保存令牌。若异步队列已满，设备先检查最早批次。仍未完成则返回 `ResourceBusy`，本次命令仍为 `Executable`。等待已有令牌或稍后轮询，再用原批次重试。
2. `IsComplete(token)` 查询完成状态，返回 `false` 时可继续录制其他命令。需要读取结果时可用 `Wait(token, timeoutNs)`。超时单位为纳秒，`0` 立即查询，`std::numeric_limits<voryx::TimeoutNs>::max()` 表示不限时等待。收到 `Timeout` 时保留令牌，可继续查询或等待。
3. 观察到完成后，可读取目标缓冲或读回处于 `CopySrc` 的离屏颜色附件。需要复用命令列表时，执行 `Reset()`、`Begin()` 并重新录制。完整像素用法见 [独立 SDK 测试程序](../tests/integration/sdk_consumer/pipeline.cpp)。

较新的令牌完成即确认同一队列中更早的批次完成，资源随确认回收。同一设备后续同步提交成功也会确认此前的异步批次。反复查询已完成的令牌仍会成功。默认令牌返回 `InvalidHandle`，其他设备的令牌返回 `WrongDevice`。Vulkan 报告 `DeviceLost` 后，释放命令、销毁设备并重建资源。 [同队列 fence 顺序](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-fences) · [Fence 查询](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetFenceStatus.html) · [Fence 等待](https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForFences.html)

## 资源何时可释放

- 命令列表在提交后进入 `Pending`，`GetState()` 和 `Reset()` 会查询实际完成状态。尚未完成时 `Reset()` 返回 `ResourceBusy`。命令包装可在提交成功后销毁，原生命令和资源由设备的提交记录继续持有。
- 未提交命令仍引用资源时，销毁返回 `ResourceBusy`。仅剩在途直接引用时，销毁缓冲、离屏附件、管线或绑定集会立即使句柄失效，底层对象在完成后回收。
- 只要管线或绑定集仍持有依赖，对应布局、采样器、采样纹理和常量缓冲的销毁仍可能返回 `ResourceBusy`。若持有方的句柄已销毁，等待其在途批次完成后重试。
- 在途批次引用的缓冲和附件，包括经绑定集引用的资源，在设备观察到相关提交完成前拒绝主机读写，返回 `ResourceBusy`。先用令牌查询或等待，再读写有效句柄。
- 设备析构会排空在途提交，耗时没有固定上限。需要控制主机等待时长时，使用带超时的 `Wait` 管理尚待完成的令牌。Vulkan 窗口设备可通过 Presenter 显式限时关闭，同时排空从该设备提交的离屏工作。独立无窗口设备使用令牌等待和设备析构。 [窗口关闭接口](window-frame-api.md#显式限时关闭)
