# QLayoutEDA GitHub 工作流程

本文档定义了 QLayoutEDA 项目的 GitHub 协作规范，所有智能体必须遵循。

---

## 一、Git Flow 分支策略

### 分支结构

```
main ─────────────────────────────────────────
  │   v0.1.0   v0.2.0   v1.0.0
  ├─────●────────●────────●─────────────────
  │
  │   hotfix/xxx
  │
develop ─────────────────────────────────────
  │
  ├── feature/gds-xxx      (eda_gds_parser)
  ├── feature/graphic-xxx  (eda_graphic_algo)
  ├── feature/ui-xxx       (eda_qt_dev)
  ├── feature/test-xxx     (eda_tester)
  │
  └── release/vX.X.X
```

### 分支命名规范

| 分支类型 | 格式 | 示例 |
|----------|------|------|
| 功能分支 | `feature/<module>-<name>` | `feature/graphic-cell-cache` |
| 修复分支 | `fix/<module>-<name>` | `fix/ui-zoom-position` |
| 热修复分支 | `hotfix/<name>` | `hotfix/crash-on-load` |
| 发布分支 | `release/vX.X.X` | `release/v0.2.0` |
| 文档分支 | `docs/<name>` | `docs/api-reference` |

### 模块前缀

| 模块 | 前缀 | 负责智能体 |
|------|------|------------|
| GDS 解析 | `gds-` | eda_gds_parser |
| 图形算法 | `graphic-` | eda_graphic_algo |
| UI 界面 | `ui-` | eda_qt_dev |
| 测试 | `test-` | eda_tester |
| 文档 | - | eda_pm |

---

## 二、Commit 规范

### 格式

```
<type>(<scope>): <subject>

<body>

<footer>

[<agent-id>]
```

### Type 类型

| Type | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(ui): 添加缩放动画效果` |
| `fix` | Bug 修复 | `fix(graphic): 修复坐标精度问题` |
| `docs` | 文档更新 | `docs: 更新 README` |
| `style` | 代码格式 | `style: 格式化代码` |
| `refactor` | 重构 | `refactor(parser): 重构解析器` |
| `test` | 测试 | `test: 添加单元测试` |
| `chore` | 构建/工具 | `chore: 更新 CI 配置` |
| `perf` | 性能优化 | `perf(graphic): 优化渲染性能` |

### Scope 范围

| Scope | 模块 |
|-------|------|
| `parser` | GDS 解析 |
| `graphic` | 图形算法 |
| `ui` | 用户界面 |
| `test` | 测试 |
| `docs` | 文档 |

### 示例

```
feat(graphic): 实现 Cell 缓存机制

- 添加 CellRenderCache 类
- 实现 LRU 淘汰策略
- 支持缩放容差匹配

性能提升：渲染时间从 21s 降低到 3s

[eda_graphic_algo]

Closes #123
```

---

## 三、Pull Request 流程

### 创建 PR

1. **从 develop 创建功能分支**
   ```bash
   git checkout develop
   git pull
   git checkout -b feature/graphic-cell-cache
   ```

2. **开发和提交**
   ```bash
   git add .
   git commit -m "feat(graphic): 实现 Cell 缓存机制 [eda_graphic_algo]"
   git push origin feature/graphic-cell-cache
   ```

3. **在 GitHub 创建 PR**
   - 标题：`feat(graphic): 实现 Cell 缓存机制`
   - 目标分支：`develop`
   - 关联 Issue：`Closes #123`
   - 描述：功能说明、测试结果

### PR 模板

```markdown
## 描述
简要描述此 PR 的内容

## 关联 Issue
Closes #

## 变更类型
- [ ] 🚀 新功能
- [ ] 🐛 Bug 修复
- [ ] 🔨 重构
- [ ] 📝 文档更新
- [ ] ✅ 测试

## 测试情况
- [ ] 单元测试通过
- [ ] 集成测试通过
- [ ] 手动测试通过

## 智能体签名
**Assigned Agent**: @QLayoutEDA-Bot (eda_xxx)
```

### 代码审查

1. CI 自动运行测试
2. 测试通过后，架构师审查
3. 审查通过后合并

---

## 四、Issue 管理

### Issue 模板

| 模板 | 用途 | 自动标签 |
|------|------|----------|
| Bug Report | 报告 Bug | `type/bug` |
| Feature Request | 功能需求 | `type/feature` |
| Development Task | 开发任务 | `type/task` |

### 标签体系

```
类型标签：
  type/feature    type/bug    type/task    type/enhancement

模块标签：
  module/parser   module/graphic   module/ui   module/test

优先级标签：
  priority/high   priority/medium   priority/low

状态标签：
  status/in-progress   status/blocked   status/review
```

### Issue 分配

创建 Issue 时添加模块标签，系统会自动分配给对应智能体：

| 模块标签 | 分配智能体 |
|----------|------------|
| `module/parser` | eda_gds_parser |
| `module/graphic` | eda_graphic_algo |
| `module/ui` | eda_qt_dev |
| `module/test` | eda_tester |

---

## 五、智能体签名

### 签名格式

每个 Commit 必须包含智能体签名：

```
[eda_xxx]
```

### 签名列表

| 智能体 ID | 签名 |
|-----------|------|
| eda_architect | `[eda_architect]` |
| eda_gds_parser | `[eda_gds_parser]` |
| eda_graphic_algo | `[eda_graphic_algo]` |
| eda_qt_dev | `[eda_qt_dev]` |
| eda_tester | `[eda_tester]` |
| eda_pm | `[eda_pm]` |

---

## 六、Code Owners

每个模块有对应的 Code Owner，负责审查代码：

```
/src/parser/ @QLayoutEDA-Bot(eda_gds_parser)
/src/graphic/ @QLayoutEDA-Bot(eda_graphic_algo)
/src/ui/ @QLayoutEDA-Bot(eda_qt_dev)
/tests/ @QLayoutEDA-Bot(eda_tester)
/docs/ @QLayoutEDA-Bot(eda_pm)
```

---

## 七、版本发布

### 版本号规范

遵循语义化版本：`MAJOR.MINOR.PATCH`

| 版本类型 | 说明 | 示例 |
|----------|------|------|
| MAJOR | 重大变更 | 1.0.0 → 2.0.0 |
| MINOR | 新功能 | 0.1.0 → 0.2.0 |
| PATCH | Bug 修复 | 0.1.0 → 0.1.1 |

### 发布流程

1. **创建发布分支**
   ```bash
   git checkout develop
   git checkout -b release/v0.2.0
   ```

2. **更新版本号和文档**
   ```bash
   # 更新 CHANGELOG.md
   # 更新版本号
   git commit -m "chore: 准备 v0.2.0 发布 [eda_architect]"
   ```

3. **合并到 main**
   ```bash
   git checkout main
   git merge --no-ff release/v0.2.0
   git tag -a v0.2.0 -m "Release v0.2.0"
   git push origin main --tags
   ```

4. **合并回 develop**
   ```bash
   git checkout develop
   git merge --no-ff release/v0.2.0
   git push origin develop
   ```

5. **删除发布分支**
   ```bash
   git branch -d release/v0.2.0
   git push origin --delete release/v0.2.0
   ```

---

## 八、快速参考

### 常用命令

```bash
# 创建功能分支
git checkout develop && git checkout -b feature/xxx

# 提交代码
git add . && git commit -m "feat(xxx): 描述 [eda_xxx]"

# 推送分支
git push origin feature/xxx

# 更新本地分支
git checkout develop && git pull
```

### 相关链接

| 链接 | 地址 |
|------|------|
| 仓库 | https://github.com/doublekous/QLayoutEDA |
| Issues | https://github.com/doublekous/QLayoutEDA/issues |
| Actions | https://github.com/doublekous/QLayoutEDA/actions |
| Projects | https://github.com/doublekous/QLayoutEDA/projects |

---

*文档版本: v1.0*
*最后更新: 2026-03-23*