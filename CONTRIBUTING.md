# 贡献指南

感谢您考虑为 QLayoutEDA 做出贡献！

## 开发流程

### 1. Fork 仓库
点击右上角 Fork 按钮，将仓库 Fork 到您的账户。

### 2. 克隆仓库
```bash
git clone https://github.com/YOUR_USERNAME/QLayoutEDA.git
cd QLayoutEDA
```

### 3. 创建功能分支
```bash
git checkout develop
git checkout -b feature/your-feature-name
```

### 4. 开发与提交
```bash
# 编码...
git add .
git commit -m "feat: 添加新功能"
```

### 5. 推送并创建 PR
```bash
git push origin feature/your-feature-name
```
然后在 GitHub 上创建 Pull Request。

## 提交规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型
- `feat` - 新功能
- `fix` - Bug 修复
- `docs` - 文档更新
- `style` - 代码格式（不影响功能）
- `refactor` - 重构
- `test` - 测试
- `chore` - 构建/工具变更

### Scope 范围
- `parser` - GDS 解析
- `graphic` - 图形算法
- `ui` - 用户界面
- `test` - 测试
- `docs` - 文档

### 示例
```
feat(graphic): 实现 Cell 缓存机制

- 添加 CellRenderCache 类
- 实现 LRU 淘汰策略
- 支持缩放容差匹配

Closes #123
```

## 智能体团队

本项目由智能体团队协作开发：

| 智能体 | 职责 |
|--------|------|
| `eda_architect` | 架构设计、代码审查 |
| `eda_gds_parser` | GDS 解析模块 |
| `eda_graphic_algo` | 图形算法、渲染引擎 |
| `eda_qt_dev` | UI 界面开发 |
| `eda_tester` | 测试、质量保证 |
| `eda_pm` | 文档、需求管理 |

## 代码风格

- 使用 clang-format 格式化代码
- 遵循 Qt 编码规范
- 添加必要的注释

## 测试

提交前请确保：
```bash
# 构建测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# 运行测试
cd build && ctest
```

## 许可证

本项目采用 MIT 许可证。贡献的代码将以相同许可证发布。