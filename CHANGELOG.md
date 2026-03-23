# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Cell 缓存机制（进行中）
- 视口变换管理（规划中）

### Changed
- 渲染性能优化（进行中）

### Fixed
- SREF 展开数量限制（待修复）

## [0.1.0] - 2026-03-20

### Added
- GDS 文件解析器
  - 支持 BOUNDARY、PATH、SREF、AREF、TEXT 等记录类型
  - 支持大文件解析（测试通过 130MB）
  - Cell 层级引用分析

- 图形渲染引擎
  - 四叉树空间索引
  - 多线程异步渲染框架
  - SREF/AREF 递归展开渲染

- UI 界面
  - 主窗口框架
  - 画布控件（缩放、平移、fitAll）
  - 图层面板
  - 属性面板
  - Cell 列表面板

- 测试
  - 单元测试框架
  - 91 个测试用例全部通过

### Dependencies
- Qt 5.15.2
- CMake 3.16+
- MSVC 2019+

---

## 版本说明

- **主版本号**: 重大架构变更或不兼容更新
- **次版本号**: 新功能添加
- **修订号**: Bug 修复和小改进

[Unreleased]: https://github.com/QLayoutEDA/QLayoutEDA/compare/v0.1.0...develop
[0.1.0]: https://github.com/QLayoutEDA/QLayoutEDA/releases/tag/v0.1.0