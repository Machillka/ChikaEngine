# Runtime Step 3.3：实现 Cooker 编排

## Metadata

- Status: Planned
- Depends on: Step 2.2, Step 3.1, Step 3.2
- Suggested scope: `engine/tools/cooker` or `tools/Cooker`, CMake, tests

## Goal

实现一个可重复执行的 Cooker，将 Project 的 Cook Manifest 转化为完整 Cooked Content 和 Cooked Asset Registry。

## Planned Changes

- 增加 `ChikaCook` 命令行工具或等价构建入口。
- Cooker 流程固定为：加载 Project -> 扫描/导入 -> 构建依赖图 -> 生成 Manifest -> 执行 Cook Rules -> 写 Registry。
- 使用临时输出目录，全部成功后再发布到最终 Cooked 目录，避免留下半成品。
- 输出汇总：资产数量、各类型数量、失败资产、耗时和最终目录。
- 首版执行可保持单线程，优先保证确定性和错误可读性。

## Minimum Deliverable

- 一条命令为指定 Project 和平台生成完整 Cooked Content。
- 输出只包含 Manifest 闭包中的资产。
- 任一资产失败时整体 Cook 失败，旧的有效 Cook 输出不被破坏。

## Done When

- 连续两次 Cook 相同输入得到语义一致 Registry 和 Artifact Hash。
- 添加未引用资产不会改变 Cook 输出。
- 删除必要依赖会在写最终输出前失败。
- Cooker 有集成测试。

## Verification

- `ChikaCook --project <project> --platform windows-x64 --output <dir>`
- 对相同输入执行两次并比较 Registry/Hash。
- 注入缺失依赖，确认最终输出保持上一次有效状态。

## Not Included

- 不实现分布式 Cook、并行 Cook 或 Derived Data Cache。
- 不进行 Package Staging。

## Next Step

- Step 3.4：让 Packaged Runtime 只读取 Cooked Registry 和 Artifact。

