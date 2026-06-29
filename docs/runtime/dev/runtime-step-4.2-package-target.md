# Runtime Step 4.2：建立一键 Package Target

## Metadata

- Status: Planned
- Depends on: Step 3.3, Step 4.1
- Suggested scope: CMake, packaging tool, docs, tests

## Goal

把构建、Cook、Staging 和基础校验串成单一可重复入口，避免人工复制文件形成不可复现交付物。

## Planned Changes

- 增加 `ChikaPackage` 工具或 CMake Package Target。
- 固定执行顺序：Build Game -> Cook Project -> Stage Package -> Validate Manifest。
- 支持最小参数：Project、Platform、Configuration、Output。
- Package 前清理或替换本次目标目录，但不影响其他 Project/Platform/Configuration。
- 为失败步骤传播非零退出码。

## Minimum Deliverable

- 一条命令生成完整 Package。
- 连续执行不会混入上一次的失效文件。
- 失败日志明确指出 Build、Cook、Stage 或 Validate 中的具体阶段。

## Done When

- Package 命令可用于本地和未来 CI。
- 相同输入生成相同 Package 文件集合。
- 构建或 Cook 失败时不发布伪成功 Package。

## Verification

- 执行 Package 命令并验证输出目录。
- 删除一项必要依赖，确认命令失败。
- 修复依赖后重新执行，确认旧错误文件未残留。

## Not Included

- 不实现安装器、自动上传或版本发布服务。
- 不实现多平台并行构建。

## Next Step

- Step 4.3：执行脱离仓库和源资产的 Standalone 验收。

