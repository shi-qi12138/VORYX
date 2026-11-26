# M1：最小缓冲 RHI 与 Null 执行路径

状态：实现已落地，关联 F03/F04/F06/F09/F14。准确用法见 [缓冲 API](../rhi-buffer-api.md)，证据见 [环境报告](../environment.md)。Texture/绘制/交换链未实现。

## 边界

- Voryx::rhi_api 只提供公共契约，依赖 Core；Null 是独立私有后端；Voryx::runtime 负责工厂与设备标识，不让 RHI 反向依赖后端。
- CreateDevice(DeviceDesc) 成为已定义的低层入口；显式 Null 可创建。Auto 不回退 Null；未编入的真实后端返回 BackendNotBuilt，Linux DX12 返回 Unsupported。
- 为验证数据复制实际生效，本阶段新增同步 ReadBuffer（CopySrc 用途）；只扩展 Buffer 读回，不承诺 Texture 读回。

## 行为

- 句柄和提交标记保存不可公开改写的设备归属。Buffer 句柄带槽位及 64 位代数；销毁后立即失效，代数耗尽的槽位不复用。
- Buffer 使用 CPU 存储；初始数据和 WriteBuffer 返回前复制。ReadBuffer 复制到调用方输出，不返回内部地址。
- ICommandList 按 Initial→Recording→Executable→Completed 工作；通用枚举保留 Pending，Null 同步执行，不伪造异步 GPU。
- 记录时引用资源，Destroy/Write/Read 在有未提交引用时返回 ResourceBusy。Reset 或命令析构释放引用；提交成功移交并完成后释放。
- Barrier 的 before 状态按整批提交顺序校验；任何一个命令无效则整批无状态或字节修改。录制失败同样不追加部分命令。
- 复制要求非零合法范围、CopySrc/CopyDst 用途及状态；同缓冲复制本阶段返回 Unsupported。
- 所有可失败入口检查线程，捕获分配失败并返回 OutOfMemory；诊断不足时允许只返回错误码。能力和设备信息为不可变借用。
- 设备/命令对象不可复制或移动实体，只移动 unique_ptr；析构在创建线程。命令意外晚于设备销毁时仍可安全析构，其操作返回 InvalidState。
- Null 提交同步完成，IsComplete/Wait 对合法 token 立即成功，不证明真实 GPU 的在途释放或同步。

## 验收

测试实际字节复制、数据持有、复用后旧句柄、跨设备/旧设备对象、线程、错误恢复、重复提交、批量失败原子性、引用释放和关闭后命令安全。四套既有构建配置运行相同契约测试；公共头独立编译，样例可用 VS 调试。接口文档、支持状态和范围同步修订。
