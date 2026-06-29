# Runtime Step 3.1：定义 Cooked Content 与 Registry

## Metadata

- Status: Planned
- Depends on: Step 2.2
- Suggested scope: `Runtime/Asset`, `tools/Cooker`, docs, tests

## Goal

定义发布态资产目录和 Cooked Asset Registry，使 Runtime 可以仅通过 GUID 找到运行产物，不再依赖 `.meta`、源目录结构或 AssetDatabase 扫描。

## Planned Changes

- 定义 Cook 输出目录，例如 `Saved/Cooked/<platform>/<project>/Content/`。
- 定义逻辑 Asset ID 到 Cooked 相对路径、AssetType、依赖和内容 Hash 的 Registry。
- Registry 和所有 Cooked 路径均相对 Content Root。
- 使用格式版本和目标平台字段拒绝不兼容 Registry。
- 区分 Cook Manifest 和 Cooked Registry：
  - Manifest 描述“计划 Cook 什么以及原因”。
  - Registry 描述“已经 Cook 出什么以及 Runtime 如何读取”。

## Minimum Deliverable

以下结构表示首版 Registry 的最小逻辑字段；Runtime 只需要该数据和 Content 文件。

```json
{
  "version": 1,
  "platform": "windows-x64",
  "assets": [
    {
      "guid": "asset-guid",
      "subAsset": "",
      "type": "texture",
      "path": "Assets/12/2515072b7d31c965a64a1eba21da4e.texture",
      "dependencies": [],
      "contentHash": "hash"
    }
  ]
}
```

## Done When

- Registry 可按 GUID 查询 Cooked 相对路径和类型。
- Registry 不包含源文件绝对路径、meta 路径或 Importer 实例信息。
- 不兼容版本、平台和重复 GUID 会被拒绝。
- Registry Loader 有独立测试。

## Verification

- 写入并重新加载固定 Registry。
- 验证路径逃逸、重复 GUID、未知版本和错误平台。
- 静态检查 Registry 中不存在绝对路径。

## Not Included

- 不执行资产转换。
- 不实现单文件归档、压缩或加密。

## Next Step

- Step 3.2：为每种发布阻塞资产定义 Cook Rule。
