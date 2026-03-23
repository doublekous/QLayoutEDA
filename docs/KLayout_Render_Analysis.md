# KLayout 渲染引擎深度分析与优化方案

## 一、KLayout 渲染核心架构

### 1.1 层次化渲染缓存 (Hierarchical Caching)

```
┌─────────────────────────────────────────────────────────┐
│                    TopCell (顶层视图)                     │
│  ┌─────────────────────────────────────────────────┐    │
│  │  Cell A Cache (QImage)                          │    │
│  │  ┌─────────────────────────────────────────┐    │    │
│  │  │  Boundary + Path + Text                  │    │    │
│  │  └─────────────────────────────────────────┘    │    │
│  └─────────────────────────────────────────────────┘    │
│                        ↓ SREF 引用                       │
│  ┌─────────────────────────────────────────────────┐    │
│  │  Cell B Cache (QImage) ← 变换后绘制              │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

**关键点**：
- 每个 Cell 独立渲染到 QImage 缓存
- SREF/AREF 引用时直接绘制缓存的图像（应用变换）
- 缓存失效条件：Cell 内容变化、缩放级别变化超过阈值

### 1.2 渲染流程

```
用户操作 → 视口变化
    ↓
计算可见区域 (Viewport Culling)
    ↓
确定需要渲染的 Cell 列表
    ↓
检查缓存是否存在
    ├─ 存在 → 直接绘制缓存图像
    └─ 不存在 → 渲染 Cell → 存入缓存 → 绘制
    ↓
应用 SREF/AREF 变换
    ↓
绘制到屏幕
```

### 1.3 关键数据结构

```cpp
// Cell 渲染缓存
struct CellRenderCache {
    QImage image;           // 渲染结果
    QRectF boundingBox;     // 边界框（世界坐标）
    double cachedScale;     // 缓存时的缩放级别
    bool dirty;             // 是否需要重新渲染
};

// 渲染配置
struct RenderConfig {
    int maxCacheSize;       // 最大缓存数量
    double scaleThreshold;  // 缩放变化阈值（超过则重新渲染）
    bool enableLOD;         // 是否启用 LOD
};
```

## 二、当前实现差距分析

| 特性 | KLayout | QLayoutEDA | 状态 |
|------|---------|------------|------|
| Cell 缓存 | ✅ | ❌ | 缺失 |
| SREF 变换绘制 | ✅ 图像变换 | ❌ 递归渲染 | 性能差 |
| 视口裁剪 | ✅ 层次裁剪 | ⚠️ 简单裁剪 | 需优化 |
| 异步渲染 | ✅ | ⚠️ 框架存在但未启用 | 需启用 |
| LOD | ✅ | ⚠️ 简单实现 | 需优化 |
| 增量更新 | ✅ | ❌ | 缺失 |

### 2.1 性能瓶颈定位

**当前代码问题** (CanvasWidget.cpp):

```cpp
// 问题 1：每次都递归展开所有 SREF
void renderCell(QPainter& painter, const QString& cellName, ...) {
    // 每次调用都遍历所有 SREF，无缓存
    for (const auto& sref : structure->references) {
        renderCell(painter, sref.structureName, ...);  // 递归调用
    }
}
```

**性能影响**：
- topcell000111 有 736,800 个 SREF
- 每次渲染都要递归调用 100 万次 renderCell()
- 耗时 21 秒

## 三、优化方案

### 阶段 1：紧急修复 (P0) - 1天

#### 1.1 修复展开条件
```cpp
// CanvasWidget.cpp 第 612 行
// 修改前
bool expandSREF = (m_transform.scale > SREF_EXPAND_THRESHOLD);
// 修改后
bool expandSREF = (m_transform.scale >= SREF_EXPAND_THRESHOLD);
```

#### 1.2 移除 SREF 展开数量限制
```cpp
// 第 50 行
constexpr int MAX_SREF_EXPAND_COUNT = 10000000;  // 提高到 1000 万
```

### 阶段 2：Cell 缓存机制 (P1) - 3天

#### 2.1 新增 CellRenderCache 类

```cpp
// CellRenderCache.h
class CellRenderCache {
public:
    struct CacheEntry {
        QImage image;
        QRectF boundingBox;
        double cachedScale;
        qint64 lastAccessTime;
    };
    
    QImage getCellImage(const QString& cellName, double scale);
    void setCellImage(const QString& cellName, const QImage& image, double scale);
    void invalidateCell(const QString& cellName);
    void clear();
    
private:
    QHash<QString, CacheEntry> m_cache;
    int m_maxCacheSize = 100;  // 最多缓存 100 个 Cell
};
```

#### 2.2 修改渲染逻辑

```cpp
// CanvasWidget.cpp - 新渲染流程
void CanvasWidget::drawObjects(QPainter& painter) {
    // 1. 计算视口范围
    QRectF viewport = getViewportRect();
    
    // 2. 获取当前 Cell
    auto structure = m_gdsData->getStructure(m_currentStructure);
    
    // 3. 检查缓存
    QImage cachedImage = m_cellCache.getCellImage(m_currentStructure, m_transform.scale);
    if (!cachedImage.isNull()) {
        painter.drawImage(0, 0, cachedImage);
        return;
    }
    
    // 4. 无缓存，渲染并存入缓存
    QImage cellImage(width(), height(), QImage::Format_ARGB32);
    QPainter cellPainter(&cellImage);
    renderCellHierarchical(cellPainter, structure, viewport);
    
    m_cellCache.setCellImage(m_currentStructure, cellImage, m_transform.scale);
    painter.drawImage(0, 0, cellImage);
}
```

### 阶段 3：层次化渲染 (P1) - 3天

#### 3.1 SREF 使用图像变换绘制

```cpp
// 不再递归渲染，直接绘制缓存的 Cell 图像
void renderSREF(QPainter& painter, const GDSSRef& sref, const QImage& childImage) {
    QTransform transform;
    transform.translate(sref.position.x, sref.position.y);
    transform.rotate(sref.angle);
    transform.scale(sref.magnification, sref.magnification);
    if (sref.reflected) {
        transform.scale(-1, 1);
    }
    
    painter.setTransform(transform);
    painter.drawImage(0, 0, childImage);
    painter.resetTransform();
}
```

#### 3.2 渲染优先级队列

```cpp
// 根据视口和层级确定渲染优先级
struct RenderPriority {
    int depth;          // Cell 层级深度
    int visibleCount;   // 可见 SREF 数量
    double area;        // 占用面积
};

// 优先渲染顶层和大面积 Cell
QPriorityQueue<RenderTask> renderQueue;
```

### 阶段 4：异步渲染 (P2) - 2天

#### 4.1 启用 RenderCoordinator

```cpp
// CanvasWidget 构造函数
m_useAsyncRendering = true;  // 启用异步渲染

connect(m_renderCoordinator.get(), &RenderCoordinator::renderComplete,
        this, [this](const RenderResult& result) {
    m_cachedImage = result.image;
    update();  // 触发重绘
});
```

#### 4.2 渲染任务分块

```cpp
// 将大任务分解为小块
struct RenderChunk {
    QString cellName;
    QRectF region;
    int priority;
};

void RenderCoordinator::renderAsync(const QVector<RenderChunk>& chunks) {
    for (const auto& chunk : chunks) {
        QtConcurrent::run([this, chunk]() {
            QImage image = renderChunk(chunk);
            emit chunkComplete(chunk.cellName, image);
        });
    }
}
```

### 阶段 5：深度优化 (P2) - 5天

#### 5.1 增量更新

```cpp
// 只更新变化的区域
void CanvasWidget::updateRegion(const QRectF& dirtyRegion) {
    // 1. 标记受影响的 Cell 缓存为 dirty
    // 2. 只重新渲染 dirty 区域
    // 3. 合并到现有缓存
}
```

#### 5.2 GPU 加速 (可选)

```cpp
// 使用 QOpenGLWidget 替代 QWidget
class CanvasWidget : public QOpenGLWidget {
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
};
```

## 四、预期性能指标

| 指标 | 当前 | 优化后 | 目标 |
|------|------|--------|------|
| fitAll 渲染 | 21s | <1s | KLayout 水平 |
| 缩放响应 | 21s | <100ms | 流畅 |
| 内存占用 | 高 | 中 | 缓存复用 |
| 大文件支持 | 卡顿 | 流畅 | 100MB+ 无感知 |

## 五、实施计划

| 阶段 | 任务 | 负责人 | 工期 | 优先级 |
|------|------|--------|------|--------|
| 阶段 1 | 紧急修复展开条件 | eda_qt_dev | 0.5天 | P0 |
| 阶段 2 | Cell 缓存机制 | eda_graphic_algo | 3天 | P1 |
| 阶段 3 | 层次化渲染 | eda_graphic_algo | 3天 | P1 |
| 阶段 4 | 异步渲染 | eda_graphic_algo | 2天 | P2 |
| 阶段 5 | 深度优化 | eda_graphic_algo | 5天 | P2 |

## 六、KLayout 关键源码参考

### 6.1 layCellView.cc - Cell 渲染
```cpp
// KLayout 的 Cell 渲染入口
void CellView::paint(QPainter* painter, const lay::Viewport& vp) {
    // 层次化渲染，使用缓存
    m_cell_renderer->render(painter, vp, m_cell_cache);
}
```

### 6.2 layCellCache.cc - Cell 缓存
```cpp
// KLayout 的 Cell 缓存实现
QImage CellCache::get_image(const db::Cell* cell, double scale) {
    CacheKey key(cell, scale);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it->second.image;  // 缓存命中
    }
    return QImage();  // 未命中，需要渲染
}
```

### 6.3 layLayoutView.cc - 视口渲染
```cpp
// KLayout 的视口渲染逻辑
void LayoutView::paintEvent(QPaintEvent* event) {
    // 1. 计算可见区域
    // 2. 获取可见 Cell 列表
    // 3. 检查缓存，渲染未缓存的 Cell
    // 4. 合成最终图像
}
```

---

**文档版本**: v1.0
**创建日期**: 2026-03-23
**作者**: EDA 架构师