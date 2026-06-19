# Runtime Step 2.2：生成 Cook Manifest

## Metadata

- Status: Planned
- Depends on: Step 2.1, Project Descriptor
- Suggested scope: `Runtime/Asset` or `tools/Cooker`, Project integration, tests

## Goal

从 Project 的启动 Scene 和 `alwaysCook` 资产生成确定性的传递依赖清单，明确最终发布内容以及每个资产被包含的原因。

## Planned Changes

- 定义 Cook Root：`startupScene`、`alwaysCook` 和未来显式命令行 Root。
- 对 AssetDependencyGraph 执行传递闭包遍历。
- Manifest 为每个资产记录 GUID、可选 SubAsset、类型、源/导入标识、依赖 Asset ID 和 Cook 原因。
- 输出稳定排序和格式版本，禁止写入绝对路径。
- 失败时聚合报告所有缺失依赖，而不是遇到第一个错误立即丢失上下文。

## Minimum Deliverable

以下结构表示 Manifest 的逻辑字段；实际实现可以使用 JSON 作为首版可调试格式。

```json
{
  "version": 1,
  "project": "ChikaSampleGame",
  "roots": ["startup-scene-guid"],
  "assets": [
    {
      "guid": "asset-guid",
      "subAsset": "",
      "type": "material",
      "dependencies": ["shader-template-guid", "texture-guid"],
      "reason": "startup-scene-guid -> mesh-renderer -> asset-guid"
    }
  ]
}
```

## Done When

- 相同 Project 和资产输入生成字节稳定或语义稳定的 Manifest。
- 未被启动 Scene 或 `alwaysCook` 引用的测试资产不会进入 Manifest。
- Manifest 能解释每个资产为何进入 Package。
- 缺失依赖使 Cook 预检失败。

## Verification

- 对固定测试图生成 Manifest 并比较稳定结果。
- 添加未引用资产，确认 Manifest 不变化。
- 删除依赖资产，确认错误包含完整引用链。

## Not Included

- 不生成最终 Cooked Content。
- 不进行压缩、打包或平台转换。

## Next Step

- Step 3.1：定义 Cooked Content 目录和 Cooked Asset Registry。
