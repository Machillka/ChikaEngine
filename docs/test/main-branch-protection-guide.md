# Main 分支 CI 与保护规则操作指南

## 目标

- 所有合入 `main` 的变更必须先通过 Pull Request。
- Linux、Windows、macOS CI 任一失败时禁止合并。
- `main` 上的每个新提交都再次触发 CI，包括 GitHub merge commit 和被允许的直接 push。
- 禁止强推和删除 `main`。

GitHub 无法阻止开发者在本地执行 `git commit`；这里保护的是远端 `main`，即禁止不符合规则的 push 或 merge。

## CI 触发逻辑

`.github/workflows/ci.yml` 包含三种触发：

1. `pull_request` targeting `main`：合并前验证。
2. `push` to `main`：合并或提交进入 `main` 后再次验证。
3. `workflow_dispatch`：人工复现。

三平台 matrix 全部成功后，汇总 job 输出固定检查名 `CI Required`。分支保护只依赖这个名称，避免 matrix 显示名变化导致规则失效。

## 首次启用顺序

1. 将 workflow 变更通过 PR 提交到 GitHub，并以 `main` 为目标运行一次。
2. 确认 PR 的 Checks 中出现并通过 `CI Required`。
3. 再创建保护规则。GitHub 要求 required check 在该仓库最近七天内至少完成过一次。

## 推荐：创建 Ruleset

在仓库 <https://github.com/Machillka/ChikaEngine> 中：

1. 打开 **Settings → Rules → Rulesets**。
2. 选择 **New ruleset → New branch ruleset**。
3. Ruleset name 填写 `Protect main`。
4. Enforcement status 选择 **Active**。
5. Bypass list 保持为空；否则名单内用户仍可绕过 CI。
6. Target branches 选择 **Include default branch**，或使用精确模式 `main`。
7. 启用 **Require a pull request before merging**。
8. 启用 **Require status checks to pass**，添加检查 `CI Required`，来源选择 GitHub Actions。
9. 启用 **Require branches to be up to date before merging**。
10. 保持 **Block force pushes** 开启，并启用 **Restrict deletions**。
11. 保存规则。

如果仓库界面没有 Rulesets，可在 **Settings → Branches → Add branch protection rule** 对 `main` 设置同样规则，并启用 **Do not allow bypassing the above settings**。

## 验收

创建规则后验证：

- 从功能分支创建目标为 `main` 的 PR：应自动出现三平台检查和 `CI Required`。
- 任一平台失败：`CI Required` 失败，Merge 按钮不可用。
- PR 分支落后于 `main`：必须先更新分支并重新通过 CI。
- 尝试直接 push `main`：应因必须通过 PR 而被拒绝。
- PR 成功合并：`push` 事件应在 `main` 的 merge commit 上再启动一次 Main CI。
- 强推或删除 `main`：应被拒绝。

## 注意事项

- 不要在其他 workflow 中复用 `CI Required` 作为 job 名，否则 required check 可能产生歧义。
- 不要给管理员、机器人或团队添加 bypass，除非已经定义紧急变更流程。
- `main` push 后的 CI 是合入后的审计；真正阻止错误进入 `main` 的是 PR 上的 required check。
- 本配置不包含定时/nightly 任务。

参考：

- GitHub Rulesets：<https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets>
- Required status checks：<https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches>
