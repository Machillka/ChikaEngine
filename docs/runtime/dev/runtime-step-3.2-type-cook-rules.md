# Runtime Step 3.2：建立按类型 Cook Rule

## Metadata

- Status: Planned
- Depends on: Step 3.1, existing Importer outputs
- Suggested scope: `tools/Cooker`, `Runtime/Asset`, tests

## Goal

为每种 Runtime 必需资产建立明确的 Source/Imported -> Cooked 转换规则，禁止 Cooker 使用“未知文件直接复制”掩盖缺失支持。

## Planned Changes

- 定义 `IAssetCookRule` 或等价类型分发接口。
- 首版覆盖：
  - Scene/Material/Shader Template：输出版本化 Runtime 数据。
  - Shader：只输出 SPIR-V 和运行时需要的 Reflection 数据，不输出 GLSL 源码。
  - Texture/Mesh/Animation：输出当前 Runtime Loader 可消费的 Imported Artifact；后续再升级平台格式。
  - Script：输出可部署的 Python Runtime Artifact，首版优先使用与打包 CPython 匹配的 bytecode。
- 每条 Rule 返回 Cooked 路径、内容 Hash、运行依赖和诊断。
- 未注册 AssetType 必须使 Cook 失败。

## Minimum Deliverable

- Cook Manifest 中每种发布阻塞资产都有对应 Rule。
- 每条 Rule 可以独立处理一个测试资产并生成 Registry Entry。
- Shader 源码、`.meta` 和 Editor-only 文件不会进入 Cooked Content。

## Done When

- 相同输入和配置产生稳定 Cooked Artifact。
- 修改输入会改变内容 Hash。
- 未支持类型、导入失败和输出冲突有明确错误。
- 每种 Rule 至少有一个成功和一个失败测试。

## Verification

- 对 Scene、Material、Shader、Texture、Mesh、Animation 和 Script 分别执行单资产 Cook。
- 扫描输出，确认不存在 `.meta`、Shader 源码和绝对路径。

## Not Included

- 不实现纹理压缩、Mesh LOD 构建或跨平台 Shader 编译矩阵。
- 不实现增量 Cook 缓存。

## Next Step

- Step 3.3：编排 Manifest 的完整 Cook 闭包。

