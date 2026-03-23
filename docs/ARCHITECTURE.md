# QLayout EDA 架构设计文档

**版本**: v1.0
**日期**: 2026-03-19
**架构师**: EDA Architect

---

## 1. 项目概述

QLayout EDA 是一款面向模拟/混合信号设计的专业 EDA 版图设计工具。

### 1.1 第一阶段目标

| 指标 | 要求 |
|------|------|
| 开发周期 | 9 周 |
| 文件支持 | 100MB GDS |
| 功能范围 | 完整基础功能 |

### 1.2 核心模块

| 模块 | 职责 |
|------|------|
| Core | 核心数据结构、工具类 |
| Parser | GDSII 文件解析 |
| Graphic | 坐标系统、空间索引、渲染引擎 |
| UI | 主窗口、画布、交互 |

---

## 2. 目录结构

```
E:\QLayoutEDA\
├── src/
│   ├── core/           # 核心模块
│   │   ├── Types.h     # 基础类型定义
│   │   ├── Utils.h     # 工具函数
│   │   └── MemoryPool.h # 内存池
│   ├── parser/         # GDS 解析模块
│   │   ├── GDSParser.h
│   │   ├── GDSParser.cpp
│   │   └── GDSFormat.h
│   ├── graphic/        # 图形模块
│   │   ├── CoordinateSystem.h
│   │   ├── SpatialIndex.h
│   │   ├── RenderEngine.h
│   │   └── LayerManager.h
│   └── ui/             # UI 模块
│       ├── MainWindow.h
│       ├── CanvasWidget.h
│       └── LayerPanel.h
├── include/
│   ├── interfaces/     # 接口定义
│   │   ├── IGDSParser.h
│   │   ├── IRenderEngine.h
│   │   ├── ICoordinateSystem.h
│   │   └── ISpatialIndex.h
│   └── core/           # 公共头文件
│       └── Types.h
├── tests/              # 测试
├── docs/               # 文档
└── resources/          # 资源文件
```

---

## 3. 技术架构

### 3.1 分层架构

```
┌─────────────────────────────────────┐
│           UI Layer (Qt)              │
│  MainWindow, Canvas, Panels          │
├─────────────────────────────────────┤
│         Graphic Layer                │
│  Coordinate, SpatialIndex, Render    │
├─────────────────────────────────────┤
│         Parser Layer                 │
│  GDS Parser, Data Model              │
├─────────────────────────────────────┤
│         Core Layer                   │
│  Types, Utils, Memory Management     │
└─────────────────────────────────────┘
```

### 3.2 命名空间

```cpp
namespace QLayoutEDA {
    // 所有代码使用统一的命名空间
}
```

### 3.3 接口设计原则

1. **依赖倒置**：上层依赖接口，不依赖实现
2. **单一职责**：每个接口只负责一个功能
3. **开闭原则**：对扩展开放，对修改关闭

---

## 4. 核心接口

### 4.1 IGDSParser - GDS 解析接口

```cpp
class IGDSParser {
public:
    virtual ~IGDSParser() = default;
    virtual bool parse(const QString& filePath) = 0;
    virtual GDSDataPtr getData() const = 0;
    virtual int getProgress() const = 0;
    virtual QString getLastError() const = 0;
};
```

### 4.2 ICoordinateSystem - 坐标系统接口

```cpp
class ICoordinateSystem {
public:
    virtual ~ICoordinateSystem() = default;
    virtual QPointF dbToScreen(qint64 x, qint64 y) const = 0;
    virtual QPoint screenToDb(const QPointF& point) const = 0;
    virtual void setScale(double scale) = 0;
    virtual double getScale() const = 0;
};
```

### 4.3 ISpatialIndex - 空间索引接口

```cpp
class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;
    virtual void insert(const QRect& bounds, quint64 id) = 0;
    virtual QVector<quint64> query(const QRect& bounds) const = 0;
    virtual void clear() = 0;
    virtual void rebuild() = 0;
};
```

### 4.4 IRenderEngine - 渲染引擎接口

```cpp
class IRenderEngine {
public:
    virtual ~IRenderEngine() = default;
    virtual void render(QPainter* painter, const QRectF& viewport) = 0;
    virtual void setData(GDSDataPtr data) = 0;
    virtual void setLayerVisible(int layer, bool visible) = 0;
};
```

---

## 5. 数据模型

### 5.1 GDS 数据结构

```cpp
// 使用 64 位整数存储坐标（纳米级精度）
using Coord = qint64;

struct GDSBoundary {
    int layer;
    int dataType;
    QVector<QPoint> points;  // 数据库坐标
};

struct GDSPath {
    int layer;
    int pathType;
    int width;
    QVector<QPoint> points;
};

struct GDSStructure {
    QString name;
    QVector<GDSBoundary> boundaries;
    QVector<GDSPath> paths;
    // ...
};

struct GDSData {
    QString libraryName;
    double units;  // 数据库单位（米）
    QHash<QString, GDSStructure> structures;
};
```

### 5.2 性能优化策略

| 策略 | 说明 |
|------|------|
| 坐标存储 | 64 位整数，避免浮点误差 |
| 空间索引 | R-Tree 四叉树，O(log n) 查询 |
| 渲染优化 | 视口裁剪 + LOD + 缓存 |
| 内存管理 | 对象池 + 延迟加载 |

---

## 6. 开发计划

### 6.1 阶段划分（9 周）

| 周次 | 任务 | 负责人 |
|------|------|--------|
| W1 | 项目框架、核心数据结构 | 架构师 + Core |
| W2-3 | GDS 解析器 | GDS Parser |
| W2-4 | 坐标系统、空间索引 | Graphic Algo |
| W3-5 | 渲染引擎 | Graphic Algo |
| W4-6 | UI 框架、主窗口 | Qt Dev |
| W7-8 | 集成测试 | Tester |
| W9 | 修复、优化 | All |

### 6.2 里程碑

| 里程碑 | 时间 | 交付物 |
|--------|------|--------|
| M1 | W1 结束 | 项目框架、编译通过 |
| M2 | W3 结束 | GDS 解析完成 |
| M3 | W5 结束 | 渲染引擎可用 |
| M4 | W7 结束 | UI 基本功能 |
| M5 | W9 结束 | 第一阶段完成 |

---

## 7. 编码规范

### 7.1 命名约定

| 类型 | 规范 | 示例 |
|------|------|------|
| 类名 | PascalCase | `CoordinateSystem` |
| 函数 | camelCase | `getDbUnits()` |
| 变量 | camelCase | `layerIndex` |
| 成员变量 | m_ 前缀 | `m_scale` |
| 常量 | k 前缀 | `kMaxLayers` |
| 宏 | UPPER_CASE | `GDS_RECORD_HEADER` |

### 7.2 文件编码

- **统一使用 UTF-8 with BOM**
- 头文件使用 `#pragma once`
- 包含顺序：系统头文件 → Qt 头文件 → 项目头文件

### 7.3 注释规范

```cpp
/**
 * @brief 简要描述
 * @param name 参数说明
 * @return 返回值说明
 */
```

---

## 8. 测试策略

### 8.1 单元测试

- 每个模块独立测试
- 覆盖率目标 > 80%
- 使用 Qt Test 框架

### 8.2 集成测试

- 模块间接口测试
- 端到端功能测试
- 性能基准测试

---

## 9. 风险管理

| 风险 | 等级 | 缓解措施 |
|------|------|----------|
| 大文件性能 | 高 | 增量加载 + 流式解析 |
| 渲染性能 | 高 | 视口裁剪 + LOD |
| 坐标精度 | 中 | 64 位整数存储 |
| 内存占用 | 中 | 对象池 + 延迟加载 |

---

**文档结束**