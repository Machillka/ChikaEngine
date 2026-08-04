# ChikaEngine Development Log

## Maintenance Rule

凡是修改仓库文件，都必须在本文件增加或更新一条带日期的记录。记录至少包含：

- 日期与影响模块。
- 修改了什么文件和行为。
- 为什么修改，以及关键架构判断。
- 实际执行的验证命令与结果。
- 尚未完成的范围、风险和下一步。

新记录按时间倒序排列。这里记录已经落地的事实，不替代 `docs/<module>/plan` 或阶段规划卡片。

---

## 2026-08-04 - Editor/Skybox 与 Physics 分支整合

### Metadata

- Area: Integration / Editor / Render / Physics / Portability / Docs
- Status: Complete

### Changes

- 在临时 `integrate` 分支按顺序合并 `origin/feat/editor` 和 `origin/feat/physics`，保留 Editor/Skybox 的 4 个提交及其后的 5 个 Physics 提交历史。
- `docs/develop.md` 冲突按时间顺序保留双方完整记录，没有使用整文件 ours/theirs 丢弃任一分支历史。
- `InspectorPanel.cpp` 保留 Editor 的可读 label 与 Physics 的 Collider authoring，并以 `CopyToFixedBuffer` 覆盖反射字符串、Collision Profile、Physics Material 和 GameObject Name 四处固定缓冲区写入。
- 不纳入 `feat/physics` 中硬编码个人 Windows 绝对路径的 `.codex/hooks.json`；引擎代码、Physics 文档和测试均完整保留。
- 规范 4 个 Physics 步骤文档的 EOF 空行，使 merge 补丁通过 `git diff --check`。

### Reason and Architecture

- `feat/editor` 是 `feat/physics` 的祖先，但分阶段合并提供了独立的 Editor/Skybox 与 Physics 验收门，同时保留原始提交可追溯性。
- Skybox/HDR/EXR 完整加载链位于 Editor 分支历史中；合并恢复 Asset、Resource、RHI、RenderGraph、Project 和入口配置的完整闭环，而不是用路径特例掩盖缺失实现。
- Physics merge 保留 Runtime ownership、body lifecycle、contact stream、Collider/Rigidbody authoring 和 Transform authority 的模块边界。

### Verification

- Editor/Skybox 阶段：全新 `build-integration` 配置与完整构建通过，CTest 21/21 通过。
- Physics 阶段：增量重新配置和完整构建通过，CTest 27/27 通过，其中 6 个测试带 `physics` 标签。
- Game 3 帧 smoke 返回 0；Python 与 Jolt 初始化成功，NightSky OpenEXR 在 worker 上成功转换为 `512x512x6` float16 Cubemap。
- `git diff --cached --check`：通过。

### Remaining Work

- Editor 仍需人工视觉确认 NightSky 最终上传后的画面、Collider/Rigidbody Inspector 交互和 Hierarchy Create Child。
- Apple M4 / MoltenVK 的 GPU timestamp capability gate 仍需单独修复；它不影响本次 Skybox 资源转换和 Physics 自动测试结论。

---

## 2026-08-04 - Jolt Compute 后端跨平台构建修复

### Metadata

- Area: CMake / Physics / ThirdParty Integration / Portability / Docs
- Status: Complete

### Changes

- `engine/ThirdParty/CMakeLists.txt`
  - 在接入 Jolt 子目录前显式关闭 `JPH_USE_DX12`、`JPH_USE_VK`、`JPH_USE_MTL` 和 `JPH_USE_CPU_COMPUTE`。
  - 保留 Jolt 刚体物理库和现有 Physics 模块接口，不修改 vendored Jolt 源码。

### Reason and Architecture

- 当前 Jolt 版本默认启用全部 ComputeSystem 后端；其中 macOS Metal/Vulkan shader 构建需要额外的 `dxc`、`spirv-cross` 或 Metal 编译工具，导致不使用 ComputeSystem 的 ChikaEngine 仍无法在干净构建目录完成配置或编译。
- ChikaEngine Physics 模块只消费 Jolt 刚体、碰撞和约束能力，没有使用 Jolt ComputeSystem。关闭这些可选后端不会改变 Physics runtime ownership、body lifecycle 或 contact event 契约。
- 选项在调用 Jolt `add_subdirectory` 前以 cache 变量固定，确保新旧构建目录行为一致。

### Verification

- `cmake -S . -B build-integration -G Ninja -DCMAKE_BUILD_TYPE=Debug`：通过；没有要求 Metal、DXC 或 SPIR-V Cross 工具链，且 Python Runtime Home 正确解析为 Homebrew Framework base prefix。
- `cmake --build build-integration --parallel 4`：通过；TinyEXR、Jolt、Game、Editor、Benchmark 和全部测试目标成功编译链接。
- `ctest --test-dir build-integration --output-on-failure`：21/21 通过。
- `ChikaGame --project .../ChikaProject.json --mode development --smoke-frames 3`：返回 0；Python 与 Jolt 初始化成功，NightSky OpenEXR 成功异步转换为 `512x512x6` RGBA16F Cubemap。

### Remaining Work

- 如果未来引入 Jolt ComputeSystem，应按目标平台单独启用所需后端，并补充 shader toolchain、运行时能力检测和跨平台验证。
- Apple M4 / MoltenVK 的 queue family 报告 `timestampValidBits == 0`，当前 GPU profiler 仍调用 `vkCmdWriteTimestamp`；该 validation error 与 Jolt Compute 和 Skybox 资源加载无关，需要在 RHI profiler capability gate 中单独修复。

---

## 2026-08-04 - Embedded Python Runtime Home 跨平台修复

### Metadata

- Area: CMake / Scripts / Portability / Docs
- Status: Complete

### Changes

- `CMakeLists.txt`
  - 不再通过 `Python3_STDLIB` 的固定目录层级推算 `CHIKA_PYTHON_HOME`。
  - 使用 CMake 已选定的隔离 Python 执行 `sys.base_prefix`，将真实 CPython base prefix 编译到 `ChikaScripts`。
  - 增加命令失败或空输出检查，使 Python Home 错误在 CMake 配置期即失败。

### Reason and Architecture

- macOS Homebrew Framework Python 的 `Python3_STDLIB` 为 `<base>/lib/python3.14`，原逻辑只取一层父目录，错误将 `<base>/lib` 传给 `PyConfig.home`，导致 CPython 无法定位 `encodings` 标准库模块。
- `sys.base_prefix` 由实际解释器报告，避免在 Windows `Lib`、Unix `lib/pythonX.Y` 和 macOS Framework 布局之间编码平台特例。
- 修复只改变 Scripts 模块的编译期 Python runtime 路径，不改变脚本 API、模块所有权或 Runtime 初始化顺序。

### Verification

- `cmake -S . -B build`：通过，输出 `Python Runtime Home: /opt/homebrew/opt/python@3.14/Frameworks/Python.framework/Versions/3.14`。
- `rg -n "CHIKA_PYTHON_HOME" build/build.ninja`：确认生成宏为 Python base prefix，未以 `/lib` 结尾。
- `cmake --build build --parallel 4`：通过，`ChikaScripts`、`ChikaGame` 和 `ChikaEditor` 成功重新链接。
- `./build/bin/ChikaGame --project .../ChikaProject.json --mode development --smoke-frames 3`：返回 0，日志确认 `Python Script Engine initialized successfully.`。
- `ctest --test-dir build --output-on-failure`：17/17 通过。

### Remaining Work

- Game smoke 在 Apple M4 / MoltenVK 上报告 `vkCmdWriteTimestamp` 的 `timestampValidBits == 0` validation error；该问题与 Python 修复无关，需在 RHI GPU timestamp capability gate 中单独处理。

## 2026-08-04 - Editor 固定字符缓冲区跨平台修复

### Metadata

- Area: Editor / Portability / Docs
- Status: Complete

### Changes

- `engine/editor/src/InspectorPanel.cpp`
  - 新增 `CopyToFixedBuffer` C++20 辅助函数，按目标数组容量截断 `std::string_view`，并显式写入 `\0` 终止符。
  - 替换反射字符串编辑和 GameObject 名称编辑中的两处 MSVC 专用 `strncpy_s` 调用。

### Reason and Architecture

- Apple Clang/macOS libc 不提供 MSVC Secure CRT 的三参数 `strncpy_s` 数组重载，导致 `ChikaEditor` 在 macOS 上编译失败。
- 修复仅限 Editor UI 的字符串到 ImGui 固定缓冲区边界，不改变 Runtime 模块依赖、所有权或公开 API。
- 未直接替换为 `strncpy`，避免源字符串过长时缓冲区缺少终止符。

### Verification

- `cmake --build build --parallel 4`：通过，`ChikaEditor` 成功编译并链接。
- `ctest --test-dir build --output-on-failure`：17/17 通过。
- `git diff --check`：通过。

### Remaining Work

- ImGui 编辑缓冲区仍保持 256 字节上限，过长名称会被安全截断；本次不扩展为动态字符串 ImGui adapter。
- 本次未启动图形界面进行 Editor 交互 smoke test。
## 2026-07-30 - 完成 Physics Step 2.2 Transform Authority 与运动插值

### Metadata

- Area: Core / Physics / Framework / Render Bridge / Test / Docs
- Related step: `docs/physics/dev/steps/2.2-transform-motion-contract.future.md`
- Status: Complete

### Goal

冻结 Static、Kinematic、Dynamic 三种 Body 的 Transform 权威方向，补齐 Kinematic target、Dynamic teleport、常用刚体运动 API、active Dynamic snapshot 和 render-only interpolation，并移除每个固定步回读所有 Body 的同步方式。

### Changes

- `FixedStepAccumulator.hpp`
  - 增加 0..1 interpolation alpha、last/total dropped physics time 与 last catch-up step count。
  - 固定步比较加入与 step 成比例的小 epsilon，避免 144 FPS 浮点累积在一秒窗口少执行一步。
- `Scene::GetPhysicsTimingStatistics()` 与 Profiler counters
  - 对外提供 fixed step、interpolation alpha、last/total dropped time、last/max step count，并记录 `Physics.FixedSteps` / `Physics.DroppedTimeUs`。
- `PhysicsDescs.h` / `PhysicsCommandBuffer.hpp/.cpp`
  - 新增包含 pose、linear/angular velocity、sleep state 的 `PhysicsBodySnapshot`。
  - 命令缓冲补齐 angular velocity、torque、angular impulse 与 activation，继续遵守 destroy -> create/rebuild -> pose -> motion 的 phase 顺序。
- `IPhysicsBackend` / `PhysicsJoltBackend`
  - 增加 angular/torque/sleep-wake adapter。
  - `CollectActiveDynamicBodySnapshots()` 使用 Jolt active rigid-body list，只为 active Dynamic Body 复制 backend-neutral 值快照；不再按 registry 遍历并锁定 Static、Kinematic 和 sleeping Body。
- `PhysicsScene`
  - 保存 main-thread snapshot cache、active handle set 和 explicit teleport dirty set。
  - Rigidbody getter 与 Framework 同步读取缓存；sleeping body 不进入常规 transform upload，Wake 后重新出现。
  - Teleport 明确支持 `resetVelocity` 与 `Wake / KeepState / DoNotWake`，无论 body 是否继续 active 都能产生一次权威 pose 更新。
- `PhysicsSubsystem` / `Scene`
  - 在全部 gameplay `FixedTick` 后增加统一 `PrepareTransforms()`：Static 改动 rebuild；Kinematic 改动 target；Dynamic 直接 pose 写入恢复为上一步 physics snapshot，scale 改动仍走 shape rebuild。
  - post-step 只回写 active Dynamic snapshot，并保存 previous/current；Scene 把 accumulator alpha 交给 render bridge。
  - render-facing world matrix 支持物理父节点层级，插值值不写回 gameplay Transform，也不进入序列化。
- `RenderSubsystem` / `CameraComponent` / `LightComponent`
  - Mesh proxy、runtime camera 和 light 统一消费 render-facing matrix；Editor/gameplay Transform 仍保持当前固定步物理姿态。
- `Rigidbody`
  - 增加 cached linear/angular velocity getter、angular velocity、force/torque、linear/angular impulse、`MoveKinematic`、`Teleport`、`WakeUp`、`Sleep`、`IsSleeping`。
  - API 注释明确 Force、Torque、Impulse 与 Angular Impulse 的单位；旧 `Impulse()` 保留为 `AddImpulse()` 兼容入口。
- `Collider`
  - parented physics body 遇到 non-uniform parent world scale 时输出 authoring diagnostic，避免层级矩阵静默扭曲 backend shape。
- `PhysicsMotionTests.cpp` / `CoreBoundaryTests.cpp` / `tests/CMakeLists.txt`
  - 新增独立 `ChikaPhysicsMotionTests` 并加入 `physics` label。
  - 覆盖 30/60/144 FPS oracle、三类 authority、Kinematic push、Dynamic direct-write/teleport、render interpolation、sleep/wake、force/impulse/torque、axis lock、parent scale 与 accumulator metrics。

### Reason / Architecture

Transform 不能同时被 gameplay、physics 和 renderer 反向修改，否则 Dynamic 会出现“一帧瞬移再被覆盖”，Kinematic 接触速度也无法由固定步 target 正确计算。现在 authority 判断只发生在 `PhysicsSubsystem` 的固定 PreStep 边界：组件只表达 authoring 或命令，`PhysicsScene` 维护 backend-neutral 状态，Jolt 只负责模拟。

渲染插值不能修改权威 Transform。`PhysicsSubsystem` 因此保存 transient previous/current snapshot，按 Scene accumulator alpha 生成 render-facing world matrix；RenderWorld 只接收矩阵值。普通 Update、碰撞查询和下一次 physics step 继续读取 current fixed-step Transform，插值不会反馈到物理。

Kinematic API 不让调用方传任意 deltaTime。调用方只提交 world target，`PhysicsScene::PreStep(fixedDeltaTime)` 在执行命令时把 Scene 的真实固定步传给 Jolt `MoveKinematic`，从而保持接触速度与模拟步长一致。

### Verification

- `$env:PYTHONUTF8='1'; cmake --build build --target ChikaPhysicsMotionTests ChikaCoreBoundaryTests`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika.PhysicsMotion|Chika.CoreBoundary"`：2/2 通过。
- `cmake --build build`：完整 Debug 构建通过，包含 `ChikaEditor`、`ChikaGame` 与全部测试目标。
- `ctest --test-dir build --output-on-failure -L physics`：6/6 通过。
- `ctest --test-dir build --output-on-failure`：27/27 通过。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。

### Remaining / Next

- Step 3.1 继续实现命名 collision layer/profile 与 Ignore/Overlap/Block 响应矩阵。
- 当前 snapshot 同步仍在 Scene 主线程完成；异步跨帧 physics、network prediction/rollback 明确不属于本步骤。
- Capsule backend shape、Physics Material asset、CCD tunneling/performance gate 仍由 Step 4.1 完成。

---

## 2026-07-30 - 完成 Physics Step 2.1 Collider / Rigidbody Authoring

### Metadata

- Area: Physics / Framework / Editor / Serialization / Test / Docs
- Status: Complete

### Goal

把碰撞几何与刚体动力学从旧 `Rigidbody` 单组件中拆开，形成 Collider-owned Body lifecycle 与 Rigidbody dynamics overlay，并让旧 Scene、Inspector、Gizmo、contact/query identity 和自动化测试形成完整闭环。

### Changes

- 新增 `Collider.hpp/.cpp`：承载 Box/Sphere/Capsule authoring、center、尺寸、Trigger、layer、friction/restitution、profile/material 名称与 query participation；没有 Rigidbody 时创建 Static Body。
- 重写 `Rigidbody.hpp/.cpp`：移除默认 Box 与碰撞字段，仅保留 motion type、mass、damping、gravity factor、CCD、allow sleep、axis lock、runtime Body handle 和 velocity/force/impulse 命令；缺少 active Collider 时输出明确诊断。
- Collider 统一处理 Start、Add、Remove、Enable、Disable、Dirty 的 deferred create/rebuild/destroy；Rigidbody 禁用后 Collider 自动重建为 Static，重新启用后恢复配置 motion type。
- `PhysicsBodyCreateDesc` 扩展 backend-neutral dynamics/query 字段；Jolt adapter 接入 mass inertia override、damping、gravity factor、LinearCast CCD、sleep、allowed DOF 和 query BodyFilter，并使用 decorated shape 实现真实 center offset。
- `PhysicsScene` 为每个 Collider 建立 generation-safe identity；atomic Body rebuild 保留 Collider handle，contact packet 和 Raycast hit 均返回有效 Collider handle。
- `Gizmo` 新增 Sphere/Capsule wire drawing；Collider backend desc 与 Gizmo 共用 signed center scale、absolute shape scale 规则，Box 逐轴缩放、Sphere 取最大轴、Capsule 高度取 Y 且半径取 X/Z 最大值。
- `JsonLoadArchive` 增加只读字段存在性检查；`GameObject` 检测旧 Rigidbody collider 字段并在缺少新 Collider 时迁移，迁移后的 Scene 保存只输出新 schema。
- Inspector 新增 Collider/Rigidbody 专用 authoring panel，根据 shape 条件显示尺寸，支持 motion、CCD、sleep、axis lock、数值范围和可读诊断；Inspector 与 Hierarchy Add Component 阻止重复 Collider/Rigidbody。
- 新增 `ChikaColliderAuthoringTests`，并更新 Core/Physics contract 与 lifecycle 旧断言；测试覆盖三种 motion 组合、非法/缺失/unsupported 诊断、稳定 Collider identity、命令化启停、shape/Gizmo/query、旧 scene migration 与新 schema round-trip。

### Reason / Architecture

碰撞形状决定 Body 是否存在，Rigidbody 只决定已有碰撞体如何运动，因此 Body 的结构生命周期必须由 Collider 拥有。这样 Collider-only 可自然表达静态场景，禁用或删除 Rigidbody 不会错误删除碰撞体；同时所有 backend mutation 仍停留在 fixed-step command buffer，组件不直接持有或调用 Jolt Body。

Collider handle 与 Body handle 的生命周期不同：修改 authoring 会 atomic rebuild Body，因此 Body identity 必须更新；Collider component 本身没有被替换，所以 Collider identity 必须保留。当前在首次 Body slot 上建立独立类型 handle，并在 registry replacement 中沿用，使 contact/query payload 可稳定指向 authoring Collider。

旧 schema 迁移发生在 GameObject 组件反序列化阶段：先读取新 Rigidbody 字段，再只读检测遗留碰撞字段；如果组件列表中已经存在 Collider 则不重复创建。该策略保持旧场景可加载，同时保证下一次保存完成一次性 schema 升级。

### Verification

- `$env:PYTHONUTF8='1'; cmake --build build --target ChikaEditor ChikaCoreBoundaryTests ChikaPhysicsContractTests ChikaPhysicsLifecycleTests ChikaPhysicsContactTests ChikaPhysicsBroadcastTests ChikaColliderAuthoringTests ChikaSceneIntegrationTests -j 4`：通过。
- `ctest --test-dir build --output-on-failure -L physics`：5/5 通过。
- `ctest --test-dir build --output-on-failure -R "Chika.(CoreBoundary|SceneIntegration)"`：2/2 通过。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `git diff --check`：通过。

### Remaining / Next

- Static/Kinematic/Dynamic Transform authority、Kinematic target、Dynamic teleport、active-body snapshot 与渲染插值已由本日后续 Step 2.2 记录完成。
- Capsule backend shape、Convex/Mesh cooking、stable Physics Material asset/combine mode 和 CCD tunneling/performance gate 仍由 Step 4.1 完成。
- `collisionProfile` 已进入 authoring schema，但项目级响应矩阵与完整 query filter 仍由 Step 3.1/3.2 完成。
- 反射生成器仍会输出既有的 MSVC 标准库 Clang diagnostic；设置 `PYTHONUTF8=1` 后生成、编译和链接成功。

---

## 2026-07-17 - 完成 Physics Step 1.2 Collision / Trigger Broadcast

### Metadata

- Area: Physics / Framework / Script / Test / Docs
- Status: Complete

### Goal

把 Step 1.1 生成的 canonical `PhysicsPairEvent` 安全接入 Framework 主线程，使 C++ Component、Python Script 与 Scene observer 都能收到完整且无重复的 Collision/Trigger Enter、Stay、Exit。

### Changes

- 新增 `PhysicsCallbackEvents.hpp`：定义 Framework owner-local `PhysicsContactEvent`，仅包含 stable GameObject ID、generation-safe Body/Collider handle、contact value、有效位、termination reason、alive 标记与 fixed step index。
- `Component` 新增 `OnCollisionEnter/Stay/Exit` 与 `OnTriggerEnter/Stay/Exit` 六个虚函数；`GameObject` 按 active/enabled/started 状态创建 receiver snapshot，并隔离单个 C++ callback 异常。
- `PhysicsSubsystem::DispatchEvents()` 在每个 fixed step 的 Simulate 与 Transform sync 后 drain 事件，固定执行 A owner、B owner、Scene observer；B view 交换 self/other 并反转 normal、relative velocity。
- A callback 后重新解析参与对象：pending-destroy 或 Body 已移除的参与方不再接收 callback，存活对端仍获得 stable ID/handle 和 `otherAlive=false` 的一次 Exit。
- `ScriptComponent` 接入六个 snake_case Python callback，并使用独立只读 `mappingproxy` payload；脚本异常只终止当前脚本 callback，不阻断后续组件或 observer。
- 新增 `PhysicsBroadcastTests.cpp` 与 `physics_broadcast_probe.py`，覆盖 Collision/Trigger 全阶段、A/B/observer 顺序、normal 方向、pause/resume、inactive/disabled、EventBus unsubscribe mutation、component remove、销毁 self/other、BodyDestroyed Exit、脚本只读 payload 与异常隔离。
- CTest 新增 `Chika.PhysicsBroadcast`，设置 physics label、仓库根目录 working directory 与 `PYTHONDONTWRITEBYTECODE=1`，避免测试污染源码目录。
- Step 1.2 卡片、步骤索引和物理路线图已同步为 Implemented / M1 Complete，并记录 dispatch、mutation 与 subscription 生命周期契约。

### Reason / Architecture

Jolt ContactListener 可能在后端 worker 上执行，不能直接访问 Framework 对象或运行 gameplay。Step 1.1 因此只负责复制和归一化 POD contact stream；本步骤把 Scene/GameObject 查找与 callback 严格放在 `PhysicsSubsystem` 主线程 post-step 阶段，保持 Physics 层不依赖 Framework。

canonical Scene event 和 owner-local view 用途不同：前者每个 pair/phase 只发布一次，供工具与跨对象系统观察；后者分别从 A、B 的 self 视角投影，供 gameplay 使用。固定 A -> B -> observer 顺序、receiver snapshot 和参与者重新解析共同保证 callback mutation 可预测且不会悬空访问。

`StopPlay`/Physics cleanup 继续通过 `ResetSceneState()` 清空 pair cache 与待发事件；EventBus subscription 仍由订阅者和 Scene 生命周期拥有，不能因一次 Play session 结束而被 PhysicsSubsystem 擅自清空。

### Verification

- `$env:PYTHONUTF8='1'; cmake -S . -B build`：通过。
- `$env:PYTHONUTF8='1'; cmake --build build -- -j1`：全量构建通过，包含 Game、Editor、Benchmark 和全部测试目标。
- `ctest --test-dir build --output-on-failure -R "Chika\.(PhysicsContract|PhysicsLifecycle|PhysicsContact|PhysicsBroadcast|SceneIntegration|CoreBoundary)"`：6/6 通过。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `clang-format` 已仅作用于本次修改的 C++ 文件；`git diff --check` 通过。

### Remaining / Next

- Step 2.1 拆分 Collider/Rigidbody authoring 后，当前 invalid Collider handle 占位才能映射为独立 Collider identity。
- contact impulse 仍没有 post-solve provider；不可用时继续由 `hasImpulse=false` 明确表达。
- Scene observer 与 owner-local callback 是两个入口，gameplay 不应同时订阅两者执行同一业务逻辑。
- 反射生成器仍会打印既有的非致命 MSVC 标准库 Clang diagnostic，但代码生成、编译和链接成功；本步骤未修改反射器。

---

## 2026-07-16 - 完成 Physics Step 1.1 Contact State Stream

### Metadata

- Area: Physics / Test / Docs
- Status: Complete

### Goal

把 Jolt 多线程 contact callbacks 转换为 backend-neutral、只在主线程 post-step 消费、按 canonical pair 去重的 Collision/Trigger Enter、Stay、Exit 流，并让 sleep、真实分离和 Body 销毁具有不同的终止语义。

### Changes

- 新增 `PhysicsEvents.hpp`：
  - 定义 `RawContactPacket`、`PhysicsPairEvent`、排序后的 `PhysicsPairKey`、sub-shape feature key。
  - point、normal、penetration、relative velocity、impulse 均有独立有效位；pre-solver impulse 明确为 unavailable。
  - canonical event 缓存 A/B Body、Collider 占位 Handle 与 GameObject ID，并携带 Collision/Trigger、fixed-step 和 termination reason。
- 重构 `IPhysicsBackend` 与 Jolt adapter：
  - `Simulate` 接收 Scene fixed-step index；`DrainRawContactPackets` 替代旧的双份 self-oriented `PollCollisionEvents`。
  - ContactListener 实现 Added、Persisted、Removed；Removed 只读取 Added/Persisted 时缓存的 identity，不访问 Jolt Body。
  - callback 仅复制值数据到互斥队列；Update 返回后才查询 Body exists/active 与 `WereBodiesInContact`，分类为 separation、remaining sub-contact、deactivation 或 missing Body。
  - engine Handle 到 Jolt BodyID 的映射只留在 backend，用于 post-step 状态补充；Reset 同时清空 Body、raw queue 和 listener contact identity。
- 扩展 `PhysicsScene`：
  - 维护按 Body/Collider Handle 排序的 pair cache，以 sub-shape feature set 聚合 contact，并为同一 pair/fixed-step/phase 去重。
  - canonical normal 固定从 A 指向 B，relative velocity 与 normal 在 canonical swap 时同时反向。
  - sleep/deactivation removal 不产生伪 Exit；真实分离产生 `Separated`，Body destroy/rebuild 产生一次 `BodyDestroyed` Exit。
  - destroy 前暂存 pair Exit，backend destroy 失败时撤销；事件只有在 `PhysicsSystem::Update` 返回并 publish 后才可由 `DrainPairEvents()` 取得。
  - 增加 active pair、pending event、raw packet、emitted event 和 suppressed deactivation Exit 统计。
- 新增 `PhysicsContactTests.cpp` 与 `Chika.PhysicsContact`：
  - 覆盖 Box Collision 的 Enter/Stay/Exit、Trigger 序列、canonical owner/normal、数据有效位、pair 去重/排序。
  - 覆盖接触中销毁、registry identity 清理、PreStep 后不可见/PostStep 后可见，以及 180 fixed-step sleep suppression。
- 更新 `PhysicsContractTests`、Step 1.1/1.2 卡片、步骤索引与物理路线图，使文档状态和公共 API 名称与实现一致。

### Reason / Architecture

Jolt contact callback 运行在物理工作线程且 Body 被锁定，Removed 阶段甚至可能已经销毁 Body。因此 callback 不能解析 Scene owner、发布 EventBus 或执行 gameplay mutation。实现把流程明确拆为三层：

```text
Jolt callback
  -> RawContactPacket + cached sub-shape identity (worker-safe)
  -> backend post-step removal classification
  -> PhysicsScene canonical pair cache
  -> ready PhysicsPairEvent queue
  -> Step 1.2 Framework dispatch
```

Jolt 在 Body sleep 时会移除 contact constraint；如果直接把 Removed 映射为 Exit，静止物体会反复 Exit/Enter。当前由 post-step active 状态与 `WereBodiesInContact` 共同分类：deactivation 保留 pair state，真实 separation 才移除最后一个 active feature 并 Exit。Body destroy 则由 Scene 在 backend destroy 前使用已缓存 owner identity 主动生成一次 Exit，避免下一步 Removed 访问 stale registry 或重复发布。

### Verification

- `cmake --build build --target ChikaPhysicsContactTests`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika.PhysicsContact"`：1/1 通过。
- `cmake --build build --target ChikaPhysicsLifecycleTests ChikaPhysicsContractTests`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika.Physics(Contract|Lifecycle|Contact)"`：3/3 通过。
- `cmake --build build`：完整工程构建通过。
- `ctest --test-dir build --output-on-failure -R "Chika.(CoreBoundary|PhysicsContract|PhysicsLifecycle|PhysicsContact|SceneIntegration)"`：5/5 通过。
- `clang-format --dry-run --Werror <本次 C++ 文件>`：通过。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `git diff --check`：通过，仅报告仓库既有 Windows CRLF 转换提示。

### Remaining / Next

- Step 1.2 将在 PhysicsSubsystem 的 Simulate/Transform Sync 后 drain canonical events，投影 A/B self-oriented view，并接入 Scene EventBus、Component 和 Script；本步骤没有提前发布 gameplay callback。
- Collider Handle 继续保持 invalid 占位，直到 Step 2.1 引入独立 Collider registry/authoring。
- Jolt Added/Persisted 位于 solver 前，真实 impulse 暂无数据源并保持 `hasImpulse=false`；不得把数值 0 当作已计算冲量。
- `FilterChanged` termination reason 已预留，实际 layer/profile 安全更新与 contact refresh 由 Step 3.1 完成。
- 当前 Windows/Clang 配置未启用 ThreadSanitizer；线程边界由 callback queue/identity mutex 与 Scene post-step 单线程访问约束，并由集成测试锁定可见时序。

---

## 2026-07-16 - 完成 Physics Step 0.2 Body Lifecycle 与 Command Buffer

### Metadata

- Area: Physics / Framework / Test / Docs
- Status: Complete

### Goal

将 Body create、destroy、rebuild 和运动修改统一到 fixed-step `PreStep`，建立 Scene-owned Registry 与确定性命令顺序，消除 Rigidbody immediate create、延迟 destroy 和同 owner 重复 Body 的生命周期竞态。

### Changes

- 新增 `PhysicsCommandBuffer.hpp/.cpp`：
  - command variant 覆盖 Create、Destroy、Rebuild、Teleport、KinematicTarget、Velocity、Force、Impulse。
  - 队列线程安全且有固定容量，记录 pending、peak、enqueued、rejected 和 cleared。
  - Scene drain 后对同 owner 的结构命令保留最后意图，再按 `Destroy -> Create/Rebuild -> Transform -> Velocity/Force` 稳定执行。
- 新增 `PhysicsBodyRegistry.hpp/.cpp`：
  - Scene 分配 index + process-unique generation handle。
  - record 保存 opaque backend token、owner、Collider Handle 占位、MotionType 和 active state，并维护 owner -> handle 索引。
  - reservation、commit、replace、remove 分离，创建失败不会注册 Handle，slot 复用不会接受 stale Handle。
- 重构 `IPhysicsBackend` 与 Jolt adapter：
  - engine handle 与 backend token 分离，Jolt `BodyID` 不进入 Framework。
  - Backend 追踪自身 token 集合，destroy/clear 都执行完整 remove + destroy。
  - 新增 immediate adapter 操作：Force、Teleport、KinematicTarget；Force/Teleport 支持 wake policy。
- 重构 `PhysicsScene`：
  - 新增 queue API、`PreStep`、execution trace 与 lifecycle statistics。
  - Rebuild 先 create-new；创建失败保留旧 Body，成功才 retire-old 并替换 owner mapping。
  - owner-target command 可作用于同一 PreStep 新建/重建的 Body；handle-target command 严格拒绝旧 generation。
  - `ResetSceneState` 幂等清空 command、Body、backend token、raw contact queue 和 transform snapshot。
- 迁移 Framework：
  - Rigidbody 在 Edit 模式不创建 runtime Body；`Start`、Enable/Dirty、Disable/Destroy 只提交 owner-targeted command。
  - Scene StartPlay 前清理旧状态，StopPlay 恢复 snapshot 前清空物理状态；未执行首个 fixed step 的 pending create 也会被取消。
  - PhysicsSubsystem 增加 queue、Force 和 Reset 包装。
- 新增 `ChikaPhysicsLifecycleTests`，覆盖 atomic rebuild、duplicate owner、失败 create、stale handle、phase order、kinematic/force/impulse、queue overflow、幂等 reset，以及 1000 次 Rigidbody 启停和 1000 次 Dirty 压力。
- 更新 `PhysicsContractTests` 以覆盖新的 command target 和默认 queue capacity。

### Reason / Architecture

Engine Handle 与 Jolt BodyID 的生命周期不同：前者用于 gameplay 长期引用，后者只在一个 backend world 内有效。Step 0.2 将 engine Handle ownership 上移到 Scene Registry，并让 Jolt 只返回 opaque token，从而使 owner mapping、generation、rebuild transaction 和 backend resource cleanup 各自只有一个 owner。

命令既支持 Handle target，也支持 owner target。Handle target 不允许 fallback，保证旧 Handle 不会误操作 replacement；owner target 在执行阶段解析，允许 Rigidbody 在 Body 尚未创建时先提交 Velocity/Force，并由 phase order 保证 Create/Rebuild 先执行。

### Verification

- `$env:PYTHONUTF8='1'; cmake -S . -B build`：通过。
- `$env:PYTHONUTF8='1'; cmake --build build --target ChikaEditor ChikaPhysicsLifecycleTests ChikaPhysicsContractTests ChikaSceneIntegrationTests ChikaCoreBoundaryTests -- -j1`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika\\.(PhysicsContract|PhysicsLifecycle|CoreBoundary|SceneIntegration)"`：4/4 通过。
- Debug stress：1000 次 Rigidbody disable/enable、1000 次 Dirty rebuild 后，registry active body 与 backend body 均保持 1；StopPlay 后均为 0。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `git diff --check`、public header Jolt boundary、legacy lifecycle symbol 和 trailing whitespace 检查：通过。

### Remaining / Next

- Step 1.1 建立 raw contact packet 到 canonical pair state 的转换，并在 Body destroy/filter change 时生成确定的 pair cleanup 输入。
- Step 1.2 才向 EventBus、Component 和 Script 广播 Collision/Trigger Enter、Stay、Exit。
- `CreateBodyImmediate` 仍为 Step 0.1 契约测试和初始化夹具保留，但 runtime Framework 已不再调用。
- 当前反射生成器仍会打印非致命 MSVC 标准库 Clang diagnostic；Framework/Editor 生成与链接正常，本步骤没有扩大该工具问题。

---

## 2026-07-16 - 完成 Physics Step 0.1 契约与 Runtime Ownership

### Metadata

- Area: Physics / Framework / Test / Docs
- Status: Complete

### Goal

在保留现有 Jolt 模拟路径的前提下，完成 Step 0.1：冻结后端无关的初始化、能力和 Handle 契约，把 Jolt 进程级全局状态从单个 PhysicsScene world 中分离，并用自动化测试锁定多 Scene 与 stale handle 行为。

### Changes

- 新增 `PhysicsHandles.hpp`：定义不可混用的 `PhysicsBodyHandle`、`PhysicsColliderHandle`，统一 invalid sentinel，并以 index + 进程级唯一 generation 拒绝 stale 和 wrong-scene handle。
- 新增 `PhysicsRuntime.hpp/.cpp`：使用 move-only Lease 引用计数管理 Jolt allocator、Factory 和 type registration；首个 lease 初始化，最后一个 lease 释放，并提供只读统计用于契约测试。
- 重构 `PhysicsDescs.h`、`IPhysicsBackend.h` 和 `PhysicsScene`：
  - 增加 `PhysicsResult`、`PhysicsStatus`、`PhysicsBodyCreateResult` 与 backend capability。
  - 清理重复 shape enum，修正碰撞事件字段拼写，默认重力改为 `{0, -9.81, 0}`。
  - 初始化、创建、模拟、查询、销毁和 mutation 对失败状态、invalid handle 与重复 Shutdown 安全。
- 将 `PhysicsJoltBackend`、`JoltLayer` 头移入 `src/` 私有边界，并把 `ChikaThirdParty` 改为 `ChikaPhysics` 的 PRIVATE 依赖；Physics public headers 不再泄漏 `JPH::*` 或 Jolt include path。
- Jolt adapter 增加 engine handle registry，Body user data 保存 engine handle；Raycast、CollisionEvent 和 Framework 不再传播 raw BodyID。Body 销毁同时执行 `RemoveBody` 与 `DestroyBody`，关闭 world 前清空所有 Body 与 registry。
- Jolt capability 明确报告 Box、Sphere、closest Raycast；Capsule、constraint、CCD 保持 unsupported，Capsule 创建返回 `UnsupportedFeature`，不再回退为 Sphere。
- `Rigidbody` 与 `PhysicsSubsystem` 迁移到强类型 Handle 和可诊断创建结果；Subsystem 初始化失败会记录 diagnostic，`Rigidbody.hpp` 只依赖 Handle 头。
- 新增 `ChikaPhysicsContractTests` 并接入 CTest，覆盖默认契约、双 Scene 与顺序重启、Runtime 注册计数、失败/重复初始化与关闭、非法 body descriptor、stale/wrong-scene handle、slot generation、capability、Raycast 映射和公开头边界。

### Reason / Architecture

Jolt Factory 和 type registration 是进程级资源，PhysicsSystem world 则是 Scene 级资源。若每个 backend instance 都独立注册和删除全局 Factory，先销毁任意一个 Scene 都可能让另一个 Scene 失效。Lease 把这两个生命周期明确分层，且释放顺序由 RAII 保证。

Jolt BodyID 同时包含 index/sequence，但它属于具体 backend world，不能作为 Framework 的长期身份。Engine registry 现在保存 BodyID，并只向外返回 engine-owned index + generation；generation 在进程内单调分配，从而同时解决 slot 复用后的悬空引用和跨 Scene 误命中。

### Verification

- `$env:PYTHONUTF8='1'; cmake -S . -B build`：通过。
- `$env:PYTHONUTF8='1'; cmake --build build --target ChikaEditor ChikaPhysicsContractTests ChikaCoreBoundaryTests ChikaSceneIntegrationTests -- -j1`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika\\.(PhysicsContract|CoreBoundary|SceneIntegration)"`：3/3 通过。
- 隐藏启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- 公开头扫描由 `Chika.PhysicsContract` 执行：未发现 `Jolt/`、`JPH::`、`PhysicsJoltBackend` 或 `JoltLayer` 泄漏。
- `git diff --check`、旧 enum/拼写/raw-handle 搜索和新增文件 trailing whitespace 检查：通过。

### Remaining / Next

- Step 0.2 负责把创建、销毁、Transform、速度和冲量统一为 fixed-step command buffer；本步骤没有提前改变现有立即创建语义。
- Step 1.1/1.2 才实现 contact state、Collision/Trigger Enter/Stay/Exit 和主线程广播；当前 `ContactListener` 采集能力不等于完整消息系统。
- Capsule、material、CCD、sleep 和 constraint 仍按 Step 4.1/4.2 实施。
- 当前反射生成器会打印其解析 MSVC 标准库的非致命 Clang diagnostic，但生成、Framework/Editor 编译和启动均成功；后续应在反射工具卡片中单独治理。

---

## 2026-07-16 - 规划物理层调整与碰撞消息闭环

### Metadata

- Area: Physics / Framework / Editor / Test / Docs
- Status: Planning Complete（未修改代码）

### Goal

基于当前 Jolt backend、PhysicsScene、PhysicsSubsystem、Rigidbody、Scene fixed-step 和 EventBus 的真实实现，规划一条从基础模拟走向完整引擎物理层的可执行路线。

### Changes

- 新增 `docs/physics/plan/physics-layer-roadmap.md`，记录当前已实现能力、关键缺口、目标 ownership、fixed-step 顺序、事件语义、过滤策略和系统验收标准。
- 新增 `docs/physics/dev/steps/README.md`，作为 12 张物理步骤卡片的实施顺序与状态入口。
- 新增 Step 0.1-6.1 卡片，覆盖：
  - Physics contract、Jolt Runtime RAII、generation-safe handle。
  - Body lifecycle、command buffer 和 registry。
  - Contact pair state、Collision/Trigger Enter/Stay/Exit 与主线程广播。
  - Collider/Rigidbody 分离、Transform authority、插值和常用运动 API。
  - Layer/Profile/Ignore-Overlap-Block、Raycast/Sweep/Overlap。
  - Shape、Physics Material、CCD、sleep、constraint 和 CharacterController。
  - Debug draw、Profiler、headless regression 和 benchmark gates。
- 所有卡片均标记 `Planned`，本次没有修改 C++、CMake、资产或测试代码。

### Reason / Architecture

当前 Jolt `ContactListener` 已能收集 Added contact，但 PhysicsScene/PhysicsSubsystem 未消费事件，Persisted/Removed 和 Trigger 语义也未实现。规划将流程拆成两层：Jolt worker callback 只收集 raw packet；PhysicsScene 在模拟后维护 canonical pair state；PhysicsSubsystem 再在主线程向两个对象投影 self-oriented callback，并通过 EventBus 广播一次 canonical pair event。

路线先处理 Runtime ownership、Handle 与 Body lifecycle，因为销毁、复用和 sleep 语义是正确 Enter/Stay/Exit 的前提；之后才拆分 Collider/Rigidbody、扩展查询和高级物理能力。公共契约保持后端无关，不把 `JPH::*` 泄漏到 Framework、Editor 或 Script。

### Verification

- 步骤卡片检查：`Cards=12 Planned=12`。
- 相对 Markdown 卡片链接检查：全部可解析。
- Physics docs trailing whitespace 检查：通过。
- `git diff --check`：通过。
- 未运行 build/test：本次仅修改 Markdown 规划，没有修改可编译代码。

### Remaining / Next

- 按 `docs/physics/dev/steps/README.md` 顺序从 Step 0.1 开始实现，禁止先跳到高级 shape、constraint 或 character feature。
- 每完成一张卡片必须同步实现状态、实际验证、`docs/develop.md` 和新增/变化的公共 API 文档。
- M0-M3 是核心可用闭环；M4-M5 是扩展；Step 6.1 的测试与观测要求需要随各步骤增量维护。

---

## 2026-07-16 - 完成 Create Child 延迟提交修复

### Metadata

- Area: Editor / Hierarchy / Test / Docs
- Status: Complete

### Goal

修复 Hierarchy 在遍历 `Scene::_gameobjects` 时立即创建 child 导致的迭代器失效，并补齐失败回滚、自动展开和非 Edit Mode 提示。

### Changes

- 新增 `HierarchyActions.hpp/.cpp`，以稳定 parent ID 提交 Create Child；提交时重新解析 parent，并集中处理 Edit Mode 校验、创建、挂载和失败回滚。
- `SceneHierarchyPanel` 的对象右键菜单只记录请求，在整棵树遍历结束后才修改 Scene。
- 创建成功后才更新 selection 和 dirty，并记录一次性 parent 展开 ID；失效展开 ID 会在绘制前清理。
- Play/Pause Mode 下对象菜单、顶部 Create GameObject 按钮和空白区域菜单保留禁用入口，并说明需要返回 Edit Mode。
- Root GameObject 创建同样只在返回有效 ID 后更新 selection/dirty，失败时输出 Editor 日志。
- 新增 `ChikaHierarchyActionTests`，覆盖无效 parent、50 次重复创建、父子关系、序列化 parent ID、唯一 ID 和 Play Mode 拒绝修改。

### Architecture

```text
ImGui menu -> pending parent ID -> finish hierarchy traversal
  -> CommitCreateChild(scene, parentId)
  -> resolve -> create -> SetParent
  -> success: selection + dirty + one-shot expand
  -> failure: rollback + log
```

Scene/Transform 继续拥有对象和层级关系；Editor helper 只编排 authoring mutation，没有引入通用 command framework 或修改序列化格式。

### Verification

- `cmake --build build --target ChikaEditor ChikaHierarchyActionTests ChikaSceneIntegrationTests`：通过。
- `build/bin/ChikaHierarchyActionTests.exe`：退出码 0。
- `build/bin/ChikaSceneIntegrationTests.exe`：退出码 0。
- 隐藏窗口启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `git diff --check`：通过，仅有工作区既有的 LF/CRLF 转换提示。

### Remaining / Next

- 自动测试不模拟 ImGui 鼠标输入；提交前仍建议人工验证对象右键 Create Child、父节点自动展开和 Play Mode 禁用提示。
- Undo/redo、复制、重命名和通用 Editor transaction 不属于本次范围。

---

## 2026-07-16 - 记录 Create Child 延迟修复步骤

### Metadata

- Area: Editor / Hierarchy / Docs
- Status: Planning Complete（未修改代码）

### Changes

- 更新 `docs/editor/dev/hierarchy-context-authoring.dev.md`，把步骤状态从完全实现纠正为 `Create Child` 待修复。
- 记录 Hierarchy 遍历期间向 `Scene::_gameobjects` 追加对象导致迭代器失效的根因。
- 将修复规划为“绘制时记录 parent ID、遍历后提交”的延迟命令，并补充失败回滚、selection、dirty、自动展开和 Edit Mode 提示要求。
- 增加重复创建、父子关系一致性、失败可观测性和 Scene 集成测试等验收标准。

### Reason / Architecture

底层 `Scene::CreateGameobject()` 和 `Transform::SetParent()` 已有集成测试覆盖，缺陷位于 Editor 绘制阶段直接修改被遍历容器。修复应限制在 SceneHierarchy 表现/交互层，不扩大为 Scene API 或通用 undo/redo 重构。

### Verification

- 只读执行 `build/bin/ChikaSceneIntegrationTests.exe`：退出码 0，确认底层 hierarchy API 正常。
- `git diff --check -- docs/editor/dev/hierarchy-context-authoring.dev.md docs/develop.md`：通过，仅有工作区既有的 LF/CRLF 转换提示。

### Remaining / Next

- 本次只维护步骤卡片，没有实现修复。
- 下一步按卡片顺序实现延迟 Create Child 请求及对应验收。

---

## 2026-07-16 - Inspector 可读标签与拖动输入统一

### Metadata

- Area: Editor / Inspector UI
- Status: Complete

### Goal

隐藏反射字段在 Inspector 显示名称中的前导下划线，并让数值控件统一支持鼠标拖动调整和键盘精确输入。

### Changes

- `InspectorPanel.cpp` 增加只作用于 UI 的标签格式化：去除 `_`、按 camelCase 分词并把首字母大写，例如 `_colliderRadius` 显示为 `Collider Radius`、`nearClip` 显示为 `Near Clip`。
- 原始反射字段名和材质参数名保留在 ImGui `##` 后作为稳定 ID；Reflection、场景序列化和 Shader 参数契约均未修改。
- 反射 `int/float/Vector3/Vector4/Quaternion` 与材质数值参数使用 Drag 控件。鼠标拖动可以连续调节，单击并松开且未发生拖动时直接切换为文本输入；Ctrl+单击和双击仍可使用。
- Metallic、Roughness、OcclusionStrength 和 NormalScale 从纯 Slider 统一为带范围约束的 DragFloat，保留原有 `[0, 1]` / `[0, 4]` 合法范围。
- 颜色字段继续使用 ColorEdit，数值分量同样支持拖动和直接输入，并保留取色弹窗。
- `VulkanAdapter.cpp` 启用 ImGui `ConfigDragClickToInputText`，让上述单击输入行为对整个 Editor 的 Drag 数值控件生效。

### Architecture

显示标签和交互样式仍由 Editor Inspector 决定。Runtime 字段名、Reflection metadata、序列化键和 Material parameter identity 不感知 UI 格式化，避免为了可读性破坏资产兼容性。

### Verification

- `cmake --build build --target ChikaEditor`：通过，包含 `InspectorPanel.cpp`、`VulkanAdapter.cpp` 重新编译和 `ChikaEditor.exe` 链接。
- `git diff --check -- engine/editor/src/InspectorPanel.cpp engine/editor/src/VulkanAdapter.cpp docs/develop.md`：通过，仅有工作区既有的 LF/CRLF 转换提示。

### Remaining / Next

- 当前格式化覆盖 Inspector 的反射属性和材质参数；其他面板后续出现用户可编辑数值时应复用同一标签/Drag 约定。

---

## 2026-07-15 - 统一编辑器颜色取色与数值输入

### Metadata

- Area: Editor / Inspector / Material UI
- Status: Complete

### Goal

让 Inspector 中所有可编辑颜色统一使用带预览色块和取色弹窗的 RGB/RGBA 控件，同时保留浮点数值输入；普通向量仍继续使用原来的 `DragFloat3/4`。

### Changes

- `InspectorPanel.cpp` 增加统一的 `DrawColor3()`、`DrawColor4()` 和颜色控件 flags。
- 名称包含 `color` 或 `emissive` 的反射 `Vector3/Vector4` 属性会自动使用颜色控件；当前 `LightComponent::color` 因此获得取色和 RGB 数值输入。
- 材质 `Vec4` 颜色参数复用相同控件。RGBA 取色器显示 Alpha Bar，`emissive` 参数保留 HDR 数值输入，允许输入大于 1.0 的分量。
- 非颜色向量不受影响，避免把 Transform、方向和尺寸等数据误当作颜色。

### Architecture

颜色控件选择仍位于 Inspector 表现层，不向 Reflection、Framework 或 Material 数据模型写入编辑器专用元数据。控件产生变化后继续通过既有反射 setter 或运行时材质实例更新路径提交数据，场景 dirty 和材质实例隔离语义保持不变。

### Verification

- `cmake --build build --target ChikaEditor`：`InspectorPanel.cpp` 编译成功并生成 object；链接阶段因为 PID 46020 正在运行并锁定 `build/bin/ChikaEditor.exe` 而失败，错误为 `failed to write output 'bin\\ChikaEditor.exe': permission denied`。未擅自终止用户进程。
- 2026-07-16 关闭旧进程后重新执行相同构建命令：通过，`ChikaEditor.exe` 链接成功。
- `git diff --check -- engine/editor/src/InspectorPanel.cpp`：通过，仅有工作区既有的 LF/CRLF 转换提示。
- 确认生成的 `InspectorPanel.cpp.obj` 时间已更新到本次构建。

### Remaining / Next

- 可继续在 Inspector 中手动检查 Light RGB、材质 BaseColor RGBA 和 Emissive HDR 输入体验。
- 本次“所有颜色”指 Inspector 中的用户数据字段，不包含 Log、Profiler、Gizmo 等只读编辑器主题颜色。

---

## 2026-07-15 - 修复天空盒视角移动时的挤压缩放

### Metadata

- Area: Render / Skybox / Math Contract / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

修复相机转动或移动观察方向时，天空盒纹理在屏幕边缘发生挤压、缩放的问题，同时保持相机平移不影响天空方向。

### Cause

`MakeSkyboxInverseViewProjection()` 把 `Mat4::Inverse()` 的转置结果直接当作数学 inverse 返回，`UpdateSkyboxData()` 又按 row-major -> GLSL column-major 契约转置一次。最终 Shader 收到 inverse-transpose，而不是 inverse view-projection。中心射线仍接近正确，但边缘射线会错误包含 projection 的 near/far 项，因此旋转观察时产生非刚性形变。

### Changes

- `RenderPipelinePasses.cpp` 在 Skybox 局部恢复真正的数学 inverse，再交给既有 UBO 上传转置；没有全局修改 `Mat4::Inverse()`，避免扩大到 Transform、Gizmo 等调用方。
- `RenderGraphSkyboxPassTests.cpp` 修正测试 FOV 的弧度输入和 Vulkan Y flip，使测试矩阵与 Runtime 一致。
- 新增屏幕边缘射线测试：验证方向符合 FOV/aspect，并且改变 near clip 不改变天空采样方向。
- Shader、Cubemap 资源、采样器和 RenderGraph pass 顺序均不需要修改。

### Architecture

```text
CPU row-major projection * rotation-only view
  -> mathematical inverse
  -> one upload transpose
  -> GLSL column-major inverse view-projection
  -> edge ray depends only on FOV/aspect/rotation
```

### Verification

- 修复前 `Chika.RenderGraphSkyboxPass` 新回归测试按预期失败：edge ray 不匹配 FOV/aspect，且依赖 near/far clip。
- 局部修复后同一测试通过。
- `cmake --build build`：通过；Game、Editor、Benchmark、Runtime 与测试 target 均完成链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(RenderGraphSkyboxPass|EnvironmentProjection|EnvironmentResource|RenderBaseline|ShaderInterface|ProjectDescriptor)$"`：6/6 通过。
- `ChikaGame.exe --smoke-frames 120 --project ChikaProject.json`：退出码 0；NightSky 异步转换、RGBA16F Cubemap 上传和 environment ready 均正常，未出现 Vulkan 错误。
- 相关 C++ 文件执行 `clang-format --dry-run --Werror`：通过。

### Remaining / Next

- `Mat4::Inverse()` 的全局命名与返回存储契约仍值得独立审计，但不属于本次 Skybox 最小修复范围。

---

## 2026-07-15 - NightSky 默认天空盒与非阻塞环境加载

### Metadata

- Area: Asset Jobs / Render Environment / Project / Assets / Test / Docs
- Related step: `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Complete

### Goal

删除 Step 6.2A 为验收生成的程序化天空盒样例，默认运行配置改用真实 `NightSkyHDRI008_2K_HDR.exr`；同时消除同步 EXR decode 和 equirectangular projection 对首帧的阻塞。

### Changes

- 新增 `NightSkyHDRI008_2K_HDR.texture`，以稳定 GUID 引用 2048x1024 OpenEXR，输出 512x512x6 RGBA16F。
- `ChikaProject.json` 的 `alwaysCook` 与 `runtime.environment.skybox` 均切换到 NightSky descriptor。
- 删除 `procedural-studio.hdr/.exr`、两个程序化 descriptor、`ChikaProject.exr.json` 和 `ChikaEnvironmentSampleGenerator`；保留通用 `CHIKA_BUILD_TOOLS` 开关。
- `EnvironmentResourceResolver` 新增 `Loading` 状态，并通过现有 `AssetManager::LoadTextureAsync` 调度 EXR decode/projection。
- loading 期间不触发失败 fallback，也不发布半成品 Texture；Renderer 使用 `fallbackColor` 继续首帧。future ready 后，主线程再执行 ResourceManager/RHI upload。
- EnvironmentResourceTests 新增单 worker gate 测试，确定性证明 Resolver 在 worker 被占用时立即返回，之后能够转为 Ready。

### Architecture

```text
main/render thread                     asset worker
------------------                     ------------
Resolve environment
  -> schedule async load ------------> EXR decode
  <- Loading immediately               equirectangular projection
render fallbackColor                   create CPU TextureCube
next frame polls future <------------- ready handle
ResourceManager/RHI upload
publish Environment.Skybox
```

CPU 像素工作可以并行，但 RHI resource creation/upload 仍保持在渲染主线程。这既消除启动卡顿，又不把 Vulkan 对象生命周期扩散到 Asset worker。

### Verification

- `cmake --build build --target ChikaEnvironmentResourceTests ChikaProjectDescriptorTests ChikaGame`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika\.(EnvironmentResource|ProjectDescriptor)$"`：2/2 通过。
- `cmake --build build`：通过；Game、Editor、Benchmark、Runtime 与全部测试 target 均完成编译链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(AssetJobs|AssetPipeline|TextureLoader|EnvironmentProjection|EnvironmentResource|RenderGraphSkyboxPass|ProjectDescriptor|ShaderInterface)$"`：8/8 通过。
- 相关 C++ 文件执行 `clang-format --dry-run --Werror`：通过。
- `ChikaGame.exe --smoke-frames 180 --project ChikaProject.json`：通过；首帧立即记录 asynchronous loading，NightSky 在 worker 上约 938 ms 转换为 512x512x6 RGBA16F/12582912 bytes，后续帧上传并记录 environment ready。

### Remaining / Next

- 首次加载仍消耗约 938 ms 后台 CPU 和 12 MiB staging，但不再阻塞窗口或首帧。
- 后续 Cooker 可缓存转换后的 Cubemap，进一步消除每次冷启动的后台转换成本。

---

## 2026-07-15 - Step 6.2A HDR/EXR Skybox 导入与实际采样完整实现

### Metadata

- Area: Asset / AssetDatabase / Resource / RHI / Render Environment / Project / Tools / Assets / Test / Docs
- Related step: `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Complete

### Goal

让 `.texture` descriptor 可以把六面 HDR/EXR 或单张 2:1 equirectangular HDR/EXR 统一转换为浮点 TextureCube，并由 Project 配置、RenderGraph 和现有 Skybox Pass 实际采样。转换必须保留大于 `1.0` 的线性亮度，且不能把投影转换放入逐帧渲染路径。

### Changes

- Asset projection：
  - 新增 `EnvironmentProjection.hpp/.cpp`，按 `px,nx,py,ny,pz,nz` 与 Vulkan cube sampling 约定生成方向。
  - direction 通过 `atan2/asin` 映射到 lat-long UV；U wrap、V clamp，重采样使用双线性过滤。
  - 支持 Float16/Float32，保持源 storage，不执行 exposure、tone mapping、gamma 或 clamp-to-1。
  - 2:1 比例允许一像素容差；拒绝空 payload、NaN/Inf、LDR/sRGB 输入、非法尺寸和超过 16384 的 face。
- Descriptor 与 dependency：
  - `.texture` 新增 `projection=none|equirectangular` 与 `outputFaceSize`；equirectangular 只允许 HDR/EXR 单源。
  - `TextureData` 保存 projection 与 source path 诊断信息。
  - `AssetRecord` 保存 descriptor 的 source/cubeFaces dependency 和 write time；依赖变化会强制 reload descriptor，meta 同步写入 dependency 列表，供 Cooker 后续递归展开。
- Resource/RHI：
  - `RHICapabilities` 增加 2D/Cubemap 最大尺寸，Vulkan 从 `VkPhysicalDeviceLimits` 填充。
  - ResourceManager 在创建 GPU image 前校验 active RHI limit，并用独立状态区分尺寸超限。
  - 投影结果继续走既有 `Upload Resources`、6-layer cube view 和 Blackboard `Environment.Skybox`，Shader 无 equirectangular 分支。
- Project、示例与诊断：
  - 默认 `ChikaProject.json` 改为稳定 GUID 的 4096x2048 Radiance HDR descriptor，输出 1024x1024x6 RGBA16F。
  - 新增 `ChikaProject.exr.json`，用于 1024x512 OpenEXR -> 256x256x6 RGBA16F 的独立 smoke。
  - 新增只依赖 `stb + tinyexr` 的 `ChikaEnvironmentSampleGenerator`；示例完全程序化生成，采用 CC0-1.0，不包含外部图像数据。
  - 样例包含云层、地平线、方向灯、下半球棋盘与方向校准线，避免再次用平滑纯色素材误判清晰度。
  - 日志区分 primary HDR/EXR、packaged LDR fallback 和 fallbackColor，并记录转换耗时、输出格式与显存字节数。
- Tests：
  - 新增 `Chika.EnvironmentProjection`，覆盖六面中心方向、经线 wrap、HDR 值、非法比例、NaN 和尺寸限制。
  - 扩展 TextureLoader、AssetPipeline、EnvironmentResource、ProjectDescriptor 与 RenderGraphSkyboxPass，覆盖 descriptor、dependency hot reload、float cube staging、RHI limit、项目切换和 Blackboard 元数据。

### Architecture

```text
ChikaProject(.exr).json
  -> stable .texture AssetReference
  -> AssetDatabase dependency graph / hot reload
  -> HDR(stbi_loadf) or EXR(TinyEXR) linear 2D payload
  -> EnvironmentProjection (load/reload only)
     direction -> lat-long UV -> wrap/clamp bilinear
  -> Float16/Float32 TextureCube, six tight layers
  -> ResourceManager RHI-limit validation + staging upload
  -> Upload Resources RG handle / cached import
  -> Blackboard Environment.Skybox
  -> existing samplerCube Skybox Pass
  -> HDRSceneColor -> exposure / ACES / display gamma
```

Asset 层拥有投影算法和 CPU payload；RHI 只报告能力并创建资源；RenderPipeline 始终消费 TextureCube。这样 Project、Forward/Deferred 和 CPU/GPU-driven 不需要知道源文件是 six-face、HDR 还是 EXR。

### Verification

- `cmake --build build`：通过，Game、Editor、Benchmark、Runtime、Tests 和工具均完成链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(EnvironmentProjection|TextureLoader|AssetPipeline|EnvironmentResource|RenderGraphSkyboxPass|ProjectDescriptor)$"`：6/6 通过。
- `ChikaGame.exe --smoke-frames 2 --project ChikaProject.json`：通过；HDR 转换约 3.0 秒，输出 1024x1024x6 RGBA16F、50331648 bytes，资源状态 ready。
- `ChikaGame.exe --smoke-frames 2 --project ChikaProject.exr.json`：通过；最终 EXR smoke 转换约 185 ms，输出 256x256x6 RGBA16F、3145728 bytes，资源状态 ready。
- ChikaEditor Forward/Deferred x CPU/GPU-driven：4/4 smoke 通过，四份日志均确认 float Cubemap upload 和 environment ready，未发现 `VUID`、Validation Error 或 `VK_ERROR`。
- 可见 Editor 截图确认方向校准线随天空背景显示，画面不是 default LDR fallback 或 fallbackColor；rotation-only matrix 单测继续证明平移不改变天空、旋转改变采样方向。
- 本机没有 `renderdoccmd`，因此未生成 `.rdc`；真实 Vulkan 创建日志、六层 RGBA16F metadata、staging 值、Validation 日志与可见截图已完成等价集成验收。

### Remaining / Next

- Step 6.2A 范围内没有剩余代码项。
- 1024 face 的 Debug 首次转换约 3 秒且占 48 MiB；Step 6.3 或 Cooker 阶段应缓存转换产物，避免每次冷启动重算。
- 下一步进入 Step 6.3：Irradiance、Specular Prefilter 与 BRDF LUT；普通 Skybox mip 不能冒充 roughness-dependent prefilter。

---

## 2026-07-15 - Step 6.1A HDR/EXR 浮点纹理契约完整实现

### Metadata

- Area: Asset / Resource / RHI / Render Environment / Test / Docs
- Related step: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
- Status: Complete

### Goal

让 Radiance `.hdr` 与 OpenEXR `.exr` 从文件解码到 Resource staging 的整个链路保留 linear float 数据和大于 `1.0` 的亮度，同时保持现有 LDR 纹理路径不回归。本步骤自身不实现单张 equirectangular 环境图转 Cubemap，也不修改 Skybox Shader；后续 Step 6.2A 已在同日完成该闭环。

### Changes

- `engine/Runtime/Asset/include/ChikaEngine/AssetLayouts.hpp`
  - 新增后端无关的 `TexturePixelStorage`（`UNorm8/Float16/Float32`）、`TextureSourceEncoding` 与 `TextureLoadStatus`。
  - `TextureData` 增加 `rowBytes`、`layerBytes`，并提供 overflow-safe 的 tight payload 计算和总字节校验。
- `engine/Runtime/Asset/src/TextureDecoder.hpp/.cpp`、`TextureLoader.cpp/.hpp`
  - LDR 继续走 `stbi_load`；Radiance HDR 改用 `stbi_is_hdr/stbi_loadf`，不再经过 8-bit 中间缓冲。
  - 使用正式 TinyEXR v3 API 解码单 part、flat scanline/tiled RGB(A) EXR；缺失 alpha 填 `1.0`，拒绝 deep、multipart、不支持的 channel/layout/compression。
  - `format=auto|rgba16f|rgba32f` 决定 payload storage；HDR/EXR `auto` 为 Float16。有限负值保留，NaN/Inf 拒绝，超出 Float16 范围时提示改用 `rgba32f`。
  - 浮点 texture 的 `srgb=true`、未实现的 `generateMips=true/mipLevels>1`、六面尺寸不一致与 LDR/HDR/EXR 混合编码均明确拒绝。
- `AssetDatabase.cpp`、`AssetManager.hpp/.cpp`
  - `.exr` 正式分类为 Texture；AssetManager 保存最近一次详细纹理加载状态，供环境资源状态映射使用。
- `RHIDesc.hpp`、`RenderResourceRequest.hpp`、`ResourceManager.hpp/.cpp`
  - 新增 `RHIFormatBytesPerTexel()`，ResourceManager 根据 Asset storage 映射 `RGBA8_*`、`RGBA16_Float`、`RGBA32_Float`。
  - 创建 GPU texture 前同时校验 Asset layout、RHI format bytes-per-texel、row/layer/total staging bytes；upload request 显式携带 `rowBytes/layerBytes`。
  - 增加 `TextureUploadStatus`，区分 invalid payload 与 GPU allocation/upload failure；日志记录 source、storage、RHI format、extent、layer/mip 和 staging bytes。
- `EnvironmentResources.hpp/.cpp`
  - `EnvironmentResourceStatus` 增加 texture decode、unsupported EXR、invalid payload 状态；GPU failure 继续独立报告。
- `tests/unit/TextureLoaderTests.cpp`、`EnvironmentResourceTests.cpp`、`tests/CMakeLists.txt`
  - 测试运行时生成确定性 Radiance HDR/OpenEXR fixture，不提交来源不明的大体积素材。
  - 覆盖大于 `1.0` 的动态范围、EXR RGB/alpha fallback、有限负值、NaN、unsupported EXR、Float16/Float32 byte layout、六面顺序/尺寸/混合编码、LDR 回归和环境错误状态。
- `Assets/Textures/Skybox/default-skybox.texture`
  - 把未实际生成 mip 的 `generateMips` 改为 `false`，避免 descriptor 声明与 mip 0-only payload 不一致。

### Architecture

```text
LDR source -----------------> stb_image UNorm8 --------------------+
Radiance .hdr -> stbi_loadf -> linear RGBA32 temporary -> F16/F32 |
OpenEXR ------> TinyEXR ----> linear RGBA32 temporary -> F16/F32 |
                                                                  v
TextureData(storage + rowBytes + layerBytes + sourceEncoding)
  -> AssetManager detailed load status
  -> ResourceManager validates storage/RHI byte agreement
  -> TextureDesc + tight staging payload + TextureUploadRequest
  -> TextureGPU(real RHI format)
  -> EnvironmentResourceResolver detailed status
```

Asset 层只描述 CPU 像素 storage，不保存 `VkFormat` 或依赖 RHI；Resource 层是 Asset storage 到 RHI format 的唯一映射点。HDR/EXR 的半精度转换发生在加载阶段，运行时每帧不做格式转换。Skybox Pass 仍统一消费 `samplerCube`，因此 6.1A 没有增加 Shader 格式分支。

### Verification

- `cmake --build build --target ChikaAsset ChikaEnvironmentResourceTests`：通过。
- `cmake --build build --target ChikaTextureLoaderTests ChikaEnvironmentResourceTests`：通过。
- `ctest --test-dir build -R "Chika\.(TextureLoader|EnvironmentResource)$" --output-on-failure`：2/2 通过。
- `cmake --build build`：通过；Editor、Game、Benchmark、Runtime libraries 与全部测试 target 均成功编译链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(TextureLoader|EnvironmentResource|AssetPipeline|RenderBaseline)$"`：4/4 通过。
- `ctest --test-dir build --output-on-failure -E "Chika\.(JobStress|JobSystem)$"`：17/17 通过。
- 非压力全集仅排除 `Chika.JobStress` 时在外层 120 秒超时且没有返回结果；进一步排除已有独立超时风险的 `Chika.JobSystem` 后其余测试全部通过。该 Jobs 问题与本次纹理链路无关，未在 6.1A 中掩盖或重构。

### Remaining Work

- Step 6.2A 已完成单张 2:1 equirectangular HDR/EXR 到 Cubemap 的确定性转换、项目配置与实际 Skybox sampling。
- 真实 float Cubemap 已通过格式/extent/staging 自动测试、Vulkan Validation runtime smoke 与截图验证；当前机器没有 `renderdoccmd`，`.rdc` capture 仅保留为可选人工复核。
- 普通 Cubemap mip 生成、diffuse irradiance、specular prefilter 与 BRDF LUT 仍属于后续卡片，不应在 Loader 中伪造。

---

## 2026-07-15 - TinyEXR Submodule 与构建目标接入

### Metadata

- Area: ThirdParty / CMake / Asset Foundation / Docs
- Related step: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
- Status: Complete

### Goal

按仓库现有 ThirdParty 规划，把 TinyEXR 作为正式 Git submodule 放入 `engine/ThirdParty`，并接入统一 CMake 构建图，为后续 EXR float texture decoder 提供稳定依赖。本次不实现 EXR 业务解码。

### Changes

- `.gitmodules` / `engine/ThirdParty/tinyexr`
  - 新增官方 `https://github.com/syoyo/tinyexr.git` submodule。
  - 固定到 `v3.2.0`，gitlink commit 为 `6f470c9ab24bf3992bc512ce07e8ecb00d9bf105`。
  - 许可证为 BSD-3-Clause；依赖版本由主仓库 gitlink 固定，不跟随远端分支漂移。
- `engine/ThirdParty/CMakeLists.txt`
  - 不调用上游仅供 compile-test 使用的 `CMakeLists.txt`，避免它修改全局 C++ 标准、warning 和 sample 选项。
  - 新增 `tinyexr` 静态库和 `tinyexr::tinyexr` alias，编译官方 v3 C11 `src/*.c` 与内置 zstd adapter。
  - 使用 TinyEXR in-tree DEFLATE 路径，不额外增加系统 zlib/libdeflate 依赖；不构建 examples、tests、CLI、CUDA/Vulkan 可选 backend。
  - 将 `tinyexr::tinyexr` 加入 `ChikaThirdParty`，保持现有 Runtime target 的统一 ThirdParty 入口。
- `README.md` / `docs/cmake/ref/build-after-pull.md`
  - 补充 TinyEXR submodule、版本、license、target 与构建边界。
- `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
  - 标记 TinyEXR dependency foundation 已完成；EXR decoder、float payload 和 GPU upload 仍保持 planned。

### Architecture

```text
.gitmodules
  -> engine/ThirdParty/tinyexr @ v3.2.0
  -> tinyexr static C11 target
  -> tinyexr::tinyexr alias
  -> ChikaThirdParty interface
  -> ChikaAsset (future EXR decoder consumer)
```

依赖接入与 EXR 业务实现分离：ThirdParty target 只负责可重复编译和公开 `include/exr.h`；Asset 层后续负责 decoder API、错误转换和 TextureData 语义。

### Verification

- `cmake --build build --target tinyexr`：通过；编译 TinyEXR v3 C11 hosted CPU/stdio 源集与内置 zstd adapter，未构建 optional GPU backend，最终生成 `tinyexr.lib`。
- `cmake --build build`：通过；Editor、Game、Benchmark、Runtime libraries 和全部测试 target 均成功链接。
- `ctest --test-dir build -R "Chika\\.(AssetPipeline|EnvironmentResource|ProjectDescriptor)$" --output-on-failure`：3/3 通过。

### Remaining Work

- Step 6.1A 仍需实现 EXR decode、float payload、format-aware staging 与 GPU upload。
- 后续 Asset target 应显式链接实际使用的 dependency；当前先通过仓库统一 `ChikaThirdParty` 接入，保持现有依赖结构。
- 若未来启用 TinyEXR libdeflate、threads 或 GPU backend，必须新增独立性能/平台验证，不能隐式打开。

---

## 2026-07-15 - HDR/EXR Skybox 输入规划

### Metadata

- Area: Asset / Resource / RHI / Render / Docs
- Related steps: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`, `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Superseded by completed Step 6.1A and Step 6.2A implementations

### Goal

纠正“`.hdr` 扩展名可读取等于 HDR 已支持”的误解，并规划从 Radiance HDR/OpenEXR 输入到浮点 Cubemap、RenderGraph 导入和 Skybox 采样的完整闭环。本次只修改文档，不修改代码、Shader、CMake、配置或素材。

### Current Finding

- 当前 PNG/JPEG/TGA 等 LDR 输入通过 `stbi_load` 解码为 RGBA8，可由现有资源链上传。
- `.hdr` 当前也走 `stbi_load(..., 4)`，高动态范围被量化为 8-bit；`.exr` 没有正式解码入口。
- `TextureData` 只有 `std::vector<uint8_t>`，ResourceManager 只在 `RGBA8_SRGB/RGBA8_UNorm` 中选择；RHI 虽已有 `RGBA16_Float/RGBA32_Float`，但尚未贯通纹理资产上传。
- 仓库中的 TinyEXR 只位于 `tinygltf/examples`，不能作为稳定的引擎依赖边界直接使用。

### Documentation Changes

- 新增 Step 6.1A，规划 float pixel storage、Radiance `stbi_loadf`、正式 TinyEXR target、format-aware staging/upload 和动态范围测试。
- 新增 Step 6.2A，规划 six-face HDR/EXR 与单张 2:1 equirectangular HDRI 两种输入、CPU import-time Cubemap conversion、项目配置、fallback 和四路径采样验收。
- 更新 Step 6.1，明确当前可靠能力是 LDR RGBA8，`.hdr` 仅能读取但不保真，`.exr` 未实现。
- 更新 Step 6.2，明确现有 Skybox Pass 格式无关，但当前视觉验收只覆盖 LDR Cubemap。
- 更新 Step 6.3，使 IBL 依赖已经验证的 float Cubemap，不重复承担 decoder/projection，并禁止把普通 mip 冒充 specular prefilter。

### Architecture Decision

```text
6.1A: HDR/EXR decoder
  -> backend-independent float TextureData
  -> format-aware Resource upload
  -> RGBA16_Float / RGBA32_Float

6.2A: .texture projection contract
  -> six faces or equirectangular source
  -> deterministic import-time Cubemap conversion
  -> Environment.Skybox
  -> existing format-independent Skybox Pass

6.3: float Cubemap
  -> irradiance / specular prefilter / BRDF LUT
```

Asset 层不保存 Vulkan format；Resource 层负责映射到 RHI format。HDR 解码和 projection 只在 asset load/reload 时执行，不进入逐帧渲染路径。Skybox 继续统一消费 TextureCube，不在 shader 中增加 equirectangular 特例。

### Verification

- `rg` 已确认 Step 6.1A -> 6.2A -> 6.3 的依赖、Next 链接和 HDR/EXR 能力边界表述一致。
- `git diff --check`：通过；仅报告工作区既有的 LF/CRLF 转换警告，没有 whitespace error。
- 本次没有运行 build/ctest，因为没有修改任何编译或运行文件。

### Remaining Work

- Step 6.1A 与 Step 6.2A 已按顺序实现，HDR-preserving Radiance/OpenEXR Skybox 能力已通过自动测试与 runtime smoke 验收。
- TinyEXR 的正式来源、版本、license 和 CMake target 已通过 submodule 落地。
- 已提交可重建的 CC0 程序化 HDR/EXR sample；单元测试继续使用小型确定性 fixture。
- Cubemap 普通 mip、IBL irradiance/specular prefilter 与 Cooker 缓存仍需后续卡片实现。

---

## 2026-07-15 - Skybox 配置加载与 LDR 色彩修复

### Metadata

- Area: Project / Editor / Game / Asset / Render / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

修复运行程序只显示纯色 fallback、默认 Cubemap 颜色被洗成灰白的问题，并把天空盒从 Editor 入口硬编码迁移为 Editor/Game 共用的 Project Runtime 配置。

### Root Cause

- 诊断截图中纯蓝区域像素固定为 `99,147,175`，与 `fallbackColor={0.1,0.2,0.3}` 经 ACES/gamma 后的结果一致；它不是任何一张 Cubemap face。
- LLDB 在旧二进制的配置函数返回后确认 `EnvironmentSettings.enabled == false`，反汇编对应写入为 `movb $0x0`，导致 Resolver 保持 `Disabled`，资源没有进入上传和 Skybox Pass。工作区源码写的是 `true`，说明运行产物与源码发生漂移；仅靠入口局部硬编码无法形成可验证配置契约。
- 配置启用后 Cubemap 已能随方向采样，但默认六面来源是 LDR PNG，而 descriptor 使用 `srgb=false`。sRGB 字节被当成线性亮度后再次经过 ACES 和 gamma，使亮部范围压缩并呈现灰白。

### Changes

- `ProjectDescriptor.hpp/.cpp`
  - `ProjectRuntimeSettings` 增加完整 `EnvironmentSettings`。
  - 解析 `runtime.environment.enabled/skybox/intensity/useFallback/fallbackColor`，并验证对象类型、非负有限强度、四分量有限颜色及无 fallback 时必须存在资源引用。
  - `BuildRuntimeBootConfig()` 继续按值投影同一份配置，避免 Runtime 持有 JSON 或 Project 可变状态。
- `ChikaProject.json`
  - 显式启用默认 Skybox，并使用稳定 Texture GUID 与诊断路径。
  - 默认天空盒加入 `alwaysCook` 根集合；Cubemap 六面依赖的 Cook 展开仍由后续 Cooker 规则负责。
- `engine/Editor/src/main.cpp`
  - 删除默认 Cubemap 的局部硬编码，支持 `--project <path>`，默认读取 `ChikaProject.json`。
  - Editor 把 Project 中的环境配置应用到 Renderer；非法 Project 直接返回错误码 `2`。
- `engine/Game/src/main.cpp`
  - Game 在加载启动 Scene 前应用 `RuntimeBootConfig.runtime.environment`，与 Editor 共用解析和渲染链。
- `Assets/Textures/Skybox/default-skybox.texture`
  - LDR PNG Cubemap 改为 `srgb=true`，由 Vulkan sRGB image format 完成采样前线性化。
- 测试
  - `ProjectDescriptorTests` 覆盖环境配置解析、Development Runtime 投影和非法配置拒绝。
  - `EnvironmentResourceTests` 验证仓库默认 LDR Skybox 上传为 `RGBA8_SRGB`；独立线性环境 fixture 仍保留 `srgb=false` 语义测试。

### Architecture

```text
ChikaProject.json runtime.environment
  -> ProjectDescriptor validation
  -> ProjectRuntimeSettings
       -> EditorApplication
       -> RuntimeBootConfig -> GameApplication
  -> Renderer::SetEnvironmentSettings
  -> EnvironmentResourceResolver
  -> AssetManager -> ResourceManager upload/cache
  -> RenderGraph Environment.Skybox
  -> Skybox Pass -> HDR -> ACES/gamma
```

Project 只保存稳定引用和渲染策略；Asset/Resource/RHI 所有权边界不变。`EnvironmentSettings.enabled` 默认仍为 false，只有项目显式配置才启用环境资源。

### Verification

- `cmake --build build --target ChikaEditor ChikaGame ChikaProjectDescriptorTests ChikaEnvironmentResourceTests`：通过。
- `ctest --test-dir build -R "Chika\.(ProjectDescriptor|EnvironmentResource|RenderGraphSkyboxPass|ShaderInterface|RenderBaseline)$" --output-on-failure`：5/5 通过。
- LLDB 在修复后二进制首帧确认 `m_settings->environment.enabled == true`，GUID、expected type 和路径与 `ChikaProject.json` 一致。
- `ChikaGame.exe --smoke-frames 2`：退出码 `0`，记录 `Environment skybox resource is ready`。
- 隐藏运行 `ChikaEditor.exe` 的 Forward/Deferred × CPU/GPU-driven 四种组合：均正常退出并记录 `Environment skybox resource is ready`，未发现 `VUID`、`Validation Error` 或 `ERROR`。
- 实际截图验证：启用前天空区域固定为 fallback 的 `99,147,175`；启用并改用 sRGB 后，天空视角像素随位置和方向变化，不再是 fallback clear。
- `clang-format -i`：本次 C++ 文件已格式化；`git diff --check`：通过。

### Remaining Work

- 当前配置粒度是 Project Runtime；Scene 级环境覆盖、Inspector authoring、运行时切换与保存尚未实现。
- 默认基准相机俯视地面，启动视图主要看到 Cubemap 的低对比度上半天空；这不影响资源采样，但后续视觉回归应增加独立的高对比度测试 Cubemap 或固定天空视角。
- IBL、HDR 源格式、Cubemap mip 生成、rotation 控件仍属于后续环境光照步骤。

---

## 2026-07-15 - Skybox Render Pass 完整接入

### Metadata

- Area: RenderGraph / RenderPipeline / Shader / Editor / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

完成 Skybox 从 `Environment.Skybox` 到 HDR Scene Color 的实际渲染闭环，并统一 CPU/GPU-driven、Forward/Deferred 四条路径的顺序、LoadOp、SceneDepth 和 fallback 行为。

### Changes

- `Assets/Shaders/skybox.vert` / `skybox.frag` 及导入产物
  - fullscreen triangle 由 `gl_VertexIndex` 生成，不创建 mesh/VBO。
  - 独立 `SkyboxData` UBO 保存 rotation-only inverse view-projection、intensity、Deferred depth 开关与统一 clear-depth 参数。
  - Fragment shader 输出线性 HDR；曝光和 tone mapping 仍只由 Post Process 负责。
- `RenderPipelinePasses.hpp/.cpp`
  - 增加 `AddSkybox()` 与 `AddGpuDrivenForward()`，调用方明确传入 HDR `LoadOp` 和是否采样 `SceneDepth`。
  - 增加 `MakeSceneDepthDescription()`，统一首次创建、RenderGraph import 和 resize 的 `DepthStencilAttachment | Sampled` 契约。
  - 增加 `MakeSkyboxInverseViewProjection()`，在 CPU 侧去掉 View translation。
- `RenderPipeline.hpp/.cpp`
  - 创建/释放 Skybox UBO、shader、pipeline 和 reflection binding；仅在 Cubemap、pipeline 与全部 binding 有效时创建 pass。
  - Forward：`Skybox(Clear HDR) -> Scene(Load HDR, Clear Depth)`。
  - Deferred：`GBuffer -> Deferred Lighting(Clear HDR) -> Skybox(Load HDR, Sample Depth) -> Transparent`。
  - CPU 与 GPU-driven 共享同一 fallback/LoadOp 决策；没有可绑定 Cubemap 时首个 scene writer 始终 clear `fallbackColor`。
- `EnvironmentResources.hpp/.cpp`
  - 增加默认 Cubemap fallback 的 `ReadyFallback` / `FallbackUnavailable` 状态。
  - 缺失或类型错误时按 `useFallback` 决定是否解析仓库内默认 Cubemap；fallback 也失效时不发布 Blackboard handle。
- `RenderGraph.hpp/.cpp`
  - DebugSnapshot 增加 texture read、color LoadOp 和 depth attachment，便于直接审计 Skybox depth/HDR 契约。
- `engine/Editor/src/main.cpp`
  - 当时由编辑器基准场景显式启用默认 Skybox；后续已由本日“Skybox 配置加载与 LDR 色彩修复”记录迁移到 Project Runtime 配置。
  - 增加 `--deferred` 与 `--gpu-driven` 参数，支持四条渲染路径 smoke。
- 测试
  - 新增 `RenderGraphSkyboxPassTests.cpp`，覆盖四路径顺序、Forward/Deferred LoadOp、depth sampled/no-depth-write、无 Skybox clear、resize descriptor 和相机平移不变性。
  - 扩展资源测试，覆盖默认 fallback、关闭 fallback、fallback 失效和无效 descriptor 拒绝。
  - 扩展 shader reflection 与 GPU 数据布局测试。

### Architecture

```text
EnvironmentSettings
  -> EnvironmentResourceResolver
  -> Blackboard[Environment.Skybox]
  -> PassModules::AddSkybox
       -> SkyboxData UBO (rotation-only inverse VP, intensity, depth policy)
       -> Cubemap reflection binding
       -> optional Blackboard[SceneDepth] sampled read
       -> Blackboard[HDRSceneColor] clear/load write
  -> Scene / Transparent
  -> Post Process exposure + tone mapping
  -> Overlay
```

关键边界：

- Asset/ResourceManager 继续拥有 Cubemap 和 RHI view；Skybox pass 只消费本帧 Blackboard handle。
- Skybox 没有 depth attachment。Forward 不读深度；Deferred 把 SceneDepth 从 `DepthWrite` 转为 `ShaderResource` 后只采样 clear/far-depth 像素。
- fallback 决策发生在建图前。资源不可用时不创建 Skybox pass，也不会在未初始化 HDR 上使用 `LoadOp::Load`。
- `SkyboxData` 与 `SceneData` 分离，防止环境显示参数污染场景/材质公共接口。

### Verification

- `glslc Assets/Shaders/skybox.vert ...` / `skybox.frag ...`：通过。
- 引擎 `ShaderReflection::Reflect()` 生成两份 SPIR-V sidecar；`Chika.ShaderInterface` 能解析 `skybox`、`EnvironmentSkybox`、`SceneDepth`。
- `clang-format -i <本次 C++ 文件>`：完成；`git diff --check`：通过。
- `cmake --build build`：通过。
- `ctest --test-dir build -R "Chika\.(RenderGraphSkyboxPass|EnvironmentResource|ShaderInterface|RenderBaseline|RenderPhase4)" --output-on-failure`：5/5 通过。
- `ctest --test-dir build -E "Chika\.(JobSystem|JobStress)" --output-on-failure`：16/16 通过。
- `ctest --test-dir build -R "Chika\.JobStress$" --output-on-failure`：1/1 通过。
- 编辑器 Vulkan smoke：Forward CPU、Deferred CPU、Forward GPU-driven、Deferred GPU-driven 均记录 `Environment skybox resource is ready`，未出现 `VUID`、`Validation Error`；测试窗口在 6-8 秒后主动终止，因此进程 `ExitCode=-1` 是 smoke harness 的预期结果。
- 全量 `ctest --test-dir build --output-on-failure` 未完成：与本次渲染改动无依赖的 `Chika.JobSystem` 单测在当前机器连续等待超过 90 秒，导致总任务超时；其余 17 项均已分别通过。

### Remaining Work

- 本步骤不包含 IBL、Cubemap HDR 格式升级、mip 生成、动态天空或 rotation 控件；这些继续由 6.3 及后续卡片承担。
- 尚未保存 RenderDoc 帧捕获；RenderGraph snapshot、自动测试和 Vulkan validation 已覆盖本次代码验收。
- `Chika.JobSystem` 的独立超时需要在 Jobs 模块任务中排查，不应在 Skybox 改动中掩盖或重构。

---

## 2026-07-15 - Skybox 环境资源解析与 RenderGraph 导入

### Metadata

- Area: Asset / Resource / Render / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete（仅完成 6.2 的资源解析与导入切片）

### Goal

把 `RenderSettings::environment.skybox` 的稳定资产引用解析为可采样的 GPU Cubemap，并以唯一的 RenderGraph 逻辑资源发布到 Blackboard `Environment.Skybox`。本次不创建 Skybox shader 或 render pass。

### Changes

- `RenderSettings.hpp`
  - `EnvironmentSettings` 增加 `Asset::AssetReference skybox` 和 `fallbackColor`。
  - 配置层只保存稳定资产身份和渲染策略，不保存 RHI handle。
- `Renderer.hpp`
  - 增加 `SetEnvironmentSettings()`，配置在下一次 RenderGraph 构建时生效。
- `EnvironmentResources.hpp/.cpp`
  - 新增 `EnvironmentResourceResolver`，集中处理 Asset 加载、Resource 上传缓存、Cubemap 契约校验和 stale handle 恢复。
  - 新增 `PublishEnvironmentSkybox()`，负责发布 Blackboard 语义。
  - 状态明确区分 disabled、缺少引用、Asset 加载失败、Resource 上传失败、纹理契约错误和 ready。
- `ResourceLayout.hpp` / `ResourceManager.hpp/.cpp`
  - `TextureGPU` 保存真实 `RHI_Format`，使 RenderGraph import descriptor 不依赖猜测。
  - 增加 `TryGetTexture()`，避免热重载后解引用已经失效的 Resource handle。
- `RenderPipeline.hpp/.cpp`
  - 在 drain texture upload jobs 前解析 Skybox，保证首次加载能进入本帧 upload 队列。
  - `AddUploadPasses()` 返回 `TextureHandle -> RGTextureHandle` 映射。
  - Skybox 命中 pending upload 时复用 upload destination RG handle；缓存已经就绪时才单独 import。
- `Assets/Textures/Skybox/default-skybox.texture(.meta)`
  - 将已有六张 face 组成默认线性 Cubemap，并提供稳定 GUID，供 smoke test 和后续 Skybox pass 使用。
- `EnvironmentResourceTests.cpp`
  - 覆盖默认 Cubemap 解析、首次上传、缓存命中、ready import、stale handle 恢复、2D 纹理拒绝和 Blackboard 发布。
- `AGENTS.md`
  - 固化本开发日志的强制维护规则。
- `docs/render/dev/steps/6.1...` / `6.2...`
  - 同步 `TextureGPU` 新契约、第一段实现状态、已完成清单和剩余 Skybox pass 工作。

### Architecture

```text
EnvironmentSettings.skybox (AssetReference)
  -> EnvironmentResourceResolver
  -> AssetManager::LoadTexture()
  -> ResourceManager::UploadTexture()
  -> TextureGPU (texture/view/sampler/format/dimension)
  -> pending upload RG handle or ready texture import
  -> RenderGraphBlackboard[Environment.Skybox]
```

所有权边界：

- `EnvironmentSettings` 拥有可持久化配置。
- `AssetManager` 拥有 CPU Asset 和 `Asset::TextureHandle`。
- `ResourceManager` 拥有 `TextureGPU` 及 RHI 资源。
- `EnvironmentResourceResolver` 只缓存非 owning handle；发现 stale handle 后通过 ResourceManager 重新上传。
- RenderGraph 只拥有本帧逻辑资源关系，不销毁 ResourceManager 的 imported texture。

同帧首次上传不能把同一物理纹理导入两次。复用 upload destination RG handle 后，后续 Skybox pass 才能得到明确的 `Upload Resources -> Skybox` 写后读依赖和正确的状态转换。

### Verification

- `cmake --build build`：通过。
- `ctest --test-dir build -R "Chika\.(EnvironmentResource|RenderBaseline|RenderPhase4)" --output-on-failure`：3/3 通过。
- `ctest --test-dir build --output-on-failure`：17/17 通过。
- `clang-format --dry-run --Werror engine/Runtime/Render/include/ChikaEngine/EnvironmentResources.hpp engine/Runtime/Render/src/EnvironmentResources.cpp`：通过。
- 隐藏窗口启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。

### Remaining Work

- 尚未创建 Skybox shader、UBO、pipeline 或 render pass，因此当前画面不会显示 Cubemap。
- 尚未修改 Forward/Deferred 的 HDR `LoadOp` 和 Deferred `SceneDepth` sampled usage。
- `useFallback` 和 `fallbackColor` 已进入配置契约，但实际画面回退要在 Skybox pass 接入时完成。
