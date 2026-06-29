# 独立 Game Runtime 路线图

## Metadata

- Date: 2026-06-15
- Area: Runtime/Application/Asset/Framework/Build/Packaging
- Status: In Progress - Phase 1 Complete
- Goal: 从当前仅有 `ChikaEditor` 的开发态工程，增量建立可 Cook、可 Package、脱离 Editor 和源资产运行的独立游戏产物。

## 当前边界

当前已经具备可复用基础：

- `Application` 和 `EngineContext` 已统一管理主循环与模块生命周期。
- `SceneManager` 已支持 Scene 序列化加载和 Play Mode。
- `AssetDatabase` 已维护 GUID/meta，`ImporterRegistry` 已维护导入职责。
- `AssetManager` 和 `ResourceManager` 已建立 CPU/GPU 资产加载路径。

Phase 0 已完成：

- 已新增独立 `ChikaGame`，复用 `Application`/`EngineContext` 且不链接 Editor/ImGui。
- 已将 Editor UI 后端从通用 Render/RHI API 外移，并建立通用 Overlay 扩展点。
- 已建立 Game/Editor/Tool/Test 构建选项、Game-only Release 构建和 Runtime 静态边界测试。

Phase 1 已完成：

- Scene 已成为带 GUID/meta 的正式资产，Game 可按 GUID 加载启动 Scene 并进入 Play。
- 已建立统一 `AssetReference`，首批 Component、Material 和 Shader Template 使用 GUID 引用。
- 已建立 Project Descriptor、RuntimeMode 和 Boot Config，EngineContext 不再写死 `Assets/` 能力。
- Scene 提供正式 Runtime Camera/Light；Game 不再借用 Editor View 或硬编码默认光源。

当前阻塞：

- 没有资产依赖图、传递依赖闭包、Cooked Registry 和 Package 目录。
- 运行时仍依赖 `.meta`、Importer、Shader Compiler 和源资产目录。

## 目标数据流

下面的数据流区分开发态和发布态，避免正式 Runtime 继续承担资产发现和导入职责。

```text
Development:
Project Descriptor
  -> Source Asset Database
  -> Import / Dependency Graph
  -> Cook Manifest
  -> Cooked Content + Cooked Asset Registry
  -> Package Staging

Standalone Runtime:
ChikaGame
  -> Project Descriptor
  -> Cooked Asset Registry
  -> Startup Scene GUID
  -> Scene Load
  -> Play Mode
```

## 设计原则

- `ChikaGame` 只链接 Engine/Runtime，不链接 Editor。
- Editor UI 集成不进入通用 Render/RHI 公共接口。
- Project Descriptor 保存项目级配置，不保存机器绝对路径。
- 持久化引用使用 GUID 与可选 SubAsset；Runtime Handle 只用于当前进程内缓存。
- Cook 输入是显式 Root Asset，输出是传递依赖闭包。
- Cooked Runtime 不扫描源资产、不生成 meta、不运行 Importer、不执行热重载。
- Package 是可复制的完整运行目录，不依赖仓库工作目录。
- 每一步保持 Editor、测试和现有开发态运行路径可用。

## 实施顺序

| Step | 最小交付 | 解锁能力 |
| --- | --- | --- |
| 0.1 | 冻结 Standalone 边界与失败基线 | 后续行为可验证 |
| 0.2 | 新增不链接 Editor 的 `ChikaGame` | 独立 Runtime 入口 |
| 0.3 | 将 ImGui/Editor UI 从通用 Render/RHI 接口外移 | Runtime 不再携带 Editor UI 依赖 |
| 0.4 | 建立 Game/Editor/Tool 构建配置 | 可单独构建发布态 Game |
| 1.1 | Scene 成为有 GUID 的正式资产 | 启动 Scene 可稳定引用 |
| 1.2 | 建立统一 `AssetReference` 契约 | Scene 依赖可被静态收集 |
| 1.3 | 新增 Project Descriptor | 项目级启动和游戏配置 |
| 1.4 | Project 配置投影到 EngineContext | Runtime 不再依赖写死配置 |
| 1.5 | 按 GUID 加载启动 Scene 并进入 Play | 第一个真正游戏启动闭环 |
| 1.6 | Scene 提供 Runtime Camera/Light | Standalone 画面不借用 Editor 语义 |
| 2.1 | 建立资产直接依赖查询 | 能解释单个资产依赖什么 |
| 2.2 | 生成传递依赖 Cook Manifest | 能解释最终为什么打包某资产 |
| 3.1 | 定义 Cooked Content 与 Registry | 发布态具有稳定输入格式 |
| 3.2 | 建立按资产类型的 Cook Rule | 每类源资产有明确运行产物 |
| 3.3 | Cooker 生成依赖闭包产物 | Manifest 转化为完整 Cooked Content |
| 3.4 | Cooked Runtime Provider | Runtime 脱离源资产和 Importer |
| 4.1 | Package Staging | 收集 EXE、DLL、配置和内容 |
| 4.2 | 一键 Package Target | 构建流程可重复 |
| 4.3 | Source-free Standalone 验收 | 证明交付物真实独立 |

## 完成定义

整个阶段完成时必须满足：

1. `ChikaGame.exe` 不链接或初始化任何 Editor/ImGui Panel 代码。
2. 游戏从 Project Descriptor 指定的启动 Scene 进入 Play。
3. 游戏画面由启动 Scene 中的 Runtime Camera/Light 驱动。
4. Cook Manifest 只包含启动 Scene 的传递依赖。
5. Package 目录不包含 `Assets/` 源文件、`.meta` 或 Shader 源码。
6. 将 Package 复制到仓库外后可以启动、渲染、运行脚本并正常退出。
7. 缺失 Project、Registry、Scene 或依赖时快速失败，并输出可定位日志。

## 非目标

- 本阶段不实现多平台 Cook、资源压缩包、加密或补丁系统。
- 本阶段不实现 Editor 项目创建向导和多项目工作区。
- 本阶段不实现异步 Scene Streaming、World Partition 或 DLC。
- 本阶段不要求完全移除开发态 Path Loading，但发布态禁止依赖它。

## 与成熟引擎的对应关系

- `ChikaGame` 对应独立 Game Target，而不是 Editor 的另一种启动参数。
- Project Descriptor 对应轻量项目描述文件，类似 Unreal `.uproject` 的项目入口职责。
- Cook Manifest 和 Cooked Asset Registry 分别对应“要构建什么”和“运行时可读取什么”的边界。
- Cook、Stage、Package 分开执行，避免把资产转换、文件收集和发布验证混为一个不可测试脚本。

## 每步实现记录要求

每完成一个 Step，应直接更新对应文档的 `Status`，并补充：

- 实际修改文件和公共 API。
- 旧路径如何迁移或保留兼容。
- 自动测试、Smoke Test 和失败路径结果。
- 生成的 Manifest、Registry 或 Package 示例。
- 尚未完成的限制和下一 Step。
