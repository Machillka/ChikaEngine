# ChikaEngine

ChikaEngine 是一个使用 C++20 开发的学习型游戏引擎。

## 已实现模块

- **Engine Runtime**：提供统一的应用主循环，并管理窗口、时间、输入、脚本、任务、资产、渲染和场景的初始化与关闭。
- **Core**：提供数学类型、日志与断言、文件/内存流、反射、JSON/二进制序列化、句柄、`SlotMap` 和固定步长累加器。
- **Framework**：实现 `Scene`、`GameObject`、组件生命周期、层级变换、Prefab、事件总线以及 Edit/Play/Pause 模式。
- **Gameplay Components**：实现摄像机、灯光、网格渲染器、骨骼动画、刚体和 Python 脚本组件。
- **Asset**：实现 GUID/Meta 资产数据库、资产引用、导入器注册、缓存、异步加载、热重载和卸载；支持纹理、glTF/GLB 网格与动画、Shader、材质和 Shader Template。
- **Resource**：将 CPU 资产上传为 GPU 网格、纹理和材质资源，并管理缓存、上传请求与资源释放。
- **Job System**：实现多工作线程调度、工作窃取、依赖任务、子任务、主线程任务、等待协助、并行循环、统计和关闭策略。
- **Profiler**：实现 CPU Scope、帧、计数器、即时事件、线程缓冲、帧历史、GPU Timing 关联和 Perfetto Trace 导出。
- **Render**：实现 `RenderWorld` 快照、视锥剔除、Render Queue、排序与批处理、GPU Instancing、Forward/Deferred、阴影、透明物体、多光源和后处理。
- **Render Graph**：实现图形、计算、复制和呈现 Pass，包含依赖编译、Pass 剔除、资源生命周期、Barrier 和调试快照。
- **GPU-Driven Rendering**：实现 GPU 视锥剔除、可见实例整理、间接绘制、CPU/GPU 可见性校验和能力回退。
- **RHI**：提供后端无关的设备与命令接口，并实现 Vulkan 后端的资源、Pipeline、Descriptor、动态渲染、同步、时间戳和 Swapchain 管理。
- **Physics**：基于 Jolt 实现物理场景、刚体、Box/Sphere 碰撞体、碰撞层、速度、冲量、射线检测和 Transform 同步。
- **Platform / Input / Time**：基于 GLFW 实现窗口、全屏与 Resize、键鼠状态，以及帧时间、时间缩放、暂停和 FPS 统计。
- **Scripting**：嵌入 Python，通过 pybind11 注册引擎类型并驱动脚本组件生命周期。
- **Project / Game**：实现项目描述文件、运行模式配置、启动场景加载和独立 `ChikaGame` 入口。
- **Editor**：基于 ImGui 实现 Docking、Viewport、场景层级、Inspector、日志、渲染统计、Profiler Timeline 和运行模式控制。
- **Benchmark / Tests**：实现确定性场景、Serial/Jobs/GPU 渲染基准、Profiler/Job 基准、JSON 结果输出，以及 Core、Scene、Asset、Render、Profiler、Job 和 Runtime Boundary 测试。
