# Renderer Step 4.4：Transient 与编译验证

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderGraph.cpp`, `RenderGraph.hpp`, `RenderBaselineTests.cpp`

## Goal

在提交 RHI 命令前拒绝非法 Graph，并确保 transient 资源只在物理兼容时复用。

## Implementation

- Texture Hash 覆盖 extent、format、mips、layers、sample count、usage。
- Buffer Hash 覆盖 size、usage、memory usage。
- Compile 检查无效 Handle、transient read-before-write、重复 transient producer、非 Graphics Attachment 和无 Attachment Graphics Pass；Attachment 合法性仅检查 `colorWrites`/Depth，不把普通 Texture 写入误判为 Attachment。
- 依赖模型按声明顺序建立 RAW、WAR、WAW 边，不再使用“最后 producer 覆盖”。
- Compile 返回成功状态并保留可展示的错误列表。

## Verification

非法 transient read-before-write 测试在 Compile 阶段失败；Texture Upload Copy Pass 编译与状态转换测试通过，现有全部测试通过。
