# 渲染引擎优化方案

## 1. KLayout 渲染架构分析

### 1.1 核心架构

```
┌─────────────────────────────────────────────────────────┐
│                    lay::LayoutView                       │
│  ┌─────────────────────────────────────────────────────┐│
│  │               Canvas Widget (UI)                     ││
│  │  - 鼠标交互                                          ││
│  │  - 缩放/平移                                         ││
│  │  - 显示缓存图像                                      ││
│  └─────────────────────────────────────────────────────┘│
│                          │                               │
│                          ▼                               │
│  ┌─────────────────────────────────────────────────────┐│
│  │           lay::LayoutViewWidget                      ││
│  │  - 视口管理                                          ││
│  │  - 图层可见性                                        ││
│  │  - 渲染任务调度                                      ││
│  └─────────────────────────────────────────────────────┘│
│                          │                               │
│                          ▼                               │
│  ┌─────────────────────────────────────────────────────┐│
│  │           lay::LayoutCanvas                          ││
│  │  - 渲染协调                                          ││
│  │  - 缓存管理                                          ││
│  │  - 异步渲染                                          ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```

### 1.2 关键技术

#### 1.2.1 Cell 渲染缓存

```cpp
// KLayout 的核心思路
class CellCache {
    struct CacheEntry {
        QImage image;           // 渲染结果缓存
        double scale;           // 缓存时的缩放
        QRectF viewport;        // 缓存时的视口
        qint64 timestamp;       // 缓存时间
        bool valid;             // 是否有效
    };
    
    QHash<QString, CacheEntry> m_cellCache;
    
public:
    QImage getCellImage(const QString& cellName, double scale);
    void invalidateCell(const QString& cellName);
    void invalidateAll();
};
```

**优势**：
- Cell 渲染一次，多次复用
- SREF 引用同一 Cell 时，共享缓存
- 内存可控（LRU 淘汰策略）

#### 1.2.2 层次渲染

```
TopCell (顶层)
    │
    ├─ 渲染到缓存 QImage
    │
    └─ SREF → ChildCell
              │
              ├─ 渲染到缓存 QImage（独立）
              │
              └─ 合成：在 TopCell 位置绘制 ChildCell 缓存
```

**关键**：每个 Cell 独立渲染缓存，SREF 只需变换+合成

#### 1.2.3 增量更新

```cpp
void LayoutCanvas::updateView(const QRectF& dirtyRect) {
    // 1. 只重绘脏区域
    // 2. 复用未变化的缓存
    // 3. 异步渲染
    
    if (m_cacheValid && m_cachedViewport.contains(dirtyRect)) {
        // 直接使用缓存
        return;
    }
    
    // 请求异步渲染
    m_renderer->requestRender(dirtyRect);
}
```

#### 1.2.4 LOD 细节层次

| 缩放级别 | 渲染策略 | 精度 |
|----------|----------|------|
| < 0.001x | BoundingBox Only | 最低 |
| 0.001x - 0.01x | 简化多边形 | 低 |
| 0.01x - 0.1x | 正常渲染 | 中 |
| > 0.1x | 完整细节 | 高 |

#### 1.2.5 异步渲染架构

```
UI 线程                     工作线程
    │                           │
    │  requestRender()          │
    ├──────────────────────────►│
    │                           │
    │  返回，不阻塞              │
    │                           │
    │                           ├─ 查询空间索引
    │                           │
    │                           ├─ 渲染到 QImage
    │                           │
    │  renderComplete(image)    │
    │◄──────────────────────────┤
    │                           │
    ├─ 更新显示                 │
    │                           │
```

### 1.3 KLayout 性能数据

| 数据规模 | 渲染时间 | 内存占用 |
|----------|----------|----------|
| 10 万图形 | < 50ms | ~50MB |
| 100 万图形 | < 200ms | ~200MB |
| 1000 万图形 | < 1s | ~500MB |

---

## 2. 当前实现差距对比

### 2.1 架构对比

| 特性 | KLayout | 当前实现 | 差距 |
|------|---------|----------|------|
| Cell 缓存 | ✅ 每Cell独立缓存 | ❌ 无缓存 | 严重 |
| SREF 合成 | ✅ 变换+合成缓存 | ❌ 递归展开渲染 | 严重 |
| 增量更新 | ✅ 脏区域重绘 | ❌ 全量重绘 | 中等 |
| 异步渲染 | ✅ 后台线程 | ✅ 已实现 | 无 |
| LOD | ✅ 多级细节 | ✅ 已实现 | 无 |
| 空间索引 | ✅ R-Tree | ✅ QuadTree | 小 |

### 2.2 性能对比

| 测试场景 | KLayout | 当前实现 | 差距 |
|----------|---------|----------|------|
| fitAll (100万SREF) | < 200ms | 21s | **100x** |
| 缩放响应 | < 50ms | 卡顿 | 严重 |
| 平移响应 | < 16ms | 卡顿 | 严重 |

### 2.3 问题根因分析

```
当前渲染流程（问题）：

fitAll()
    │
    └─ renderCell(TopCell)
           │
           ├─ 渲染直接图形 ✓
           │
           └─ 遍历 736,800 个 SREF
                  │
                  └─ 每个都查询 + 绘制边界框 ✗（慢！）

KLayout 渲染流程（优化）：

fitAll()
    │
    ├─ 检查 TopCell 缓存 → 无效
    │
    ├─ 异步渲染 TopCell
    │      │
    │      ├─ 渲染直接图形 ✓
    │      │
    │      └─ SREF 合成
    │             │
    │             ├─ ChildCell 已缓存？
    │             │      ├─ 是 → 直接合成 ✓
    │             │      └─ 否 → 异步渲染 ChildCell
    │             │
    │             └─ 缓存 TopCell 结果
    │
    └─ 显示缓存图像
```

---

## 3. 分阶段优化方案

### P0 - 紧急（本周完成）

#### 3.1.1 Cell 渲染缓存

**目标**：实现 Cell 级别的渲染缓存

**文件**：
- `src/graphic/CellCache.h` - 新增
- `src/graphic/CellCache.cpp` - 新增

**实现**：

```cpp
class CellCache {
public:
    struct CacheKey {
        QString cellName;
        double scale;
        int lodLevel;
        
        bool operator==(const CacheKey& other) const;
    };
    
    struct CacheEntry {
        QImage image;
        QRectF bounds;
        qint64 timestamp;
        int accessCount;
    };
    
    QImage get(const CacheKey& key);
    void put(const CacheKey& key, const QImage& image);
    void invalidate(const QString& cellName);
    void clear();
    
    // LRU 淘汰
    void setMaxMemorySize(qint64 bytes);
    
private:
    QHash<CacheKey, CacheEntry> m_cache;
    qint64 m_maxMemory = 100 * 1024 * 1024;  // 100MB
    qint64 m_currentMemory = 0;
};
```

**预期效果**：
- fitAll 时间：21s → < 500ms
- 内存增加：~100MB

#### 3.1.2 SREF 边界框批量绘制

**目标**：批量绘制 SREF 边界框，减少绘制调用

**修改**：
- `src/ui/CanvasWidget.cpp` - `renderCell()`

**实现**：

```cpp
// 收集所有边界框，一次性绘制
QVector<QRectF> srefBounds;
srefBounds.reserve(structure->references.size());

for (const auto& sref : structure->references) {
    // 计算边界框
    srefBounds.append(rect);
}

// 批量绘制
painter.setPen(QPen(QColor(120, 120, 120), 1));
painter.setBrush(Qt::NoBrush);
for (const auto& rect : srefBounds) {
    painter.drawRect(rect);
}
```

---

### P1 - 重要（下周完成）

#### 3.2.1 层次渲染合成

**目标**：实现 Cell 层次渲染和合成

**文件**：
- `src/graphic/HierarchicalRenderer.h` - 新增
- `src/graphic/HierarchicalRenderer.cpp` - 新增

**实现**：

```cpp
class HierarchicalRenderer {
public:
    QImage renderCell(const QString& cellName, double scale, int lod);
    
private:
    QImage renderCellInternal(const QString& cellName, double scale, int lod);
    void compositeSREF(QPainter& painter, const GDSReference& sref, 
                       const QImage& childImage);
    
    CellCache m_cache;
    GDSDataPtr m_data;
};
```

#### 3.2.2 脏区域增量更新

**目标**：只重绘变化区域

**修改**：
- `src/ui/CanvasWidget.cpp` - `paintEvent()`

**实现**：

```cpp
void CanvasWidget::paintEvent(QPaintEvent* event) {
    QRectF dirtyRect = event->rect();
    
    if (m_cacheValid && m_cachedImage.rect().contains(dirtyRect.toRect())) {
        // 增量更新：只绘制脏区域
        QPainter painter(this);
        painter.drawImage(dirtyRect.topLeft(), m_cachedImage, dirtyRect);
    } else {
        // 全量渲染
        renderFull();
    }
}
```

---

### P2 - 优化（后续迭代）

#### 3.3.1 GPU 加速渲染

**目标**：使用 OpenGL/Vulkan 加速渲染

**方案**：
- 使用 QOpenGLWidget 替代 QWidget
- 顶点缓冲对象 (VBO) 存储图形数据
- 着色器渲染

**预期效果**：
- 渲染速度：提升 5-10x

#### 3.3.2 延迟加载

**目标**：大文件按需加载 Cell

**实现**：
- 流式解析 GDS
- Cell 按需加载到内存
- LRU 淘汰策略

---

## 4. 预期性能指标

### 4.1 目标

| 场景 | 当前 | P0 优化后 | P1 优化后 | KLayout |
|------|------|-----------|-----------|---------|
| fitAll (100万SREF) | 21s | < 500ms | < 200ms | < 200ms |
| 缩放响应 | 卡顿 | < 100ms | < 50ms | < 50ms |
| 平移响应 | 卡顿 | < 50ms | < 16ms | < 16ms |
| 内存占用 | 低 | +100MB | +200MB | ~200MB |

### 4.2 验收标准

- [ ] fitAll 时间 < 500ms
- [ ] 缩放/平移流畅（60fps）
- [ ] 内存占用 < 500MB
- [ ] 无画面撕裂

---

## 5. 实施计划

### Week 1 (P0)

| 日期 | 任务 | 负责人 |
|------|------|--------|
| Day 1-2 | 实现 CellCache | eda_graphic_algo |
| Day 3-4 | 集成 CellCache 到 CanvasWidget | eda_graphic_algo |
| Day 5 | 测试验证 | eda_tester |

### Week 2 (P1)

| 日期 | 任务 | 负责人 |
|------|------|--------|
| Day 1-3 | 实现 HierarchicalRenderer | eda_graphic_algo |
| Day 4-5 | 增量更新 + 测试 | eda_graphic_algo |

---

## 6. 风险与应对

| 风险 | 影响 | 应对措施 |
|------|------|----------|
| 内存溢出 | 高 | 实现 LRU 淘汰，限制缓存大小 |
| 缓存失效频繁 | 中 | 优化缓存粒度，支持多分辨率缓存 |
| 多线程竞争 | 中 | 使用 QMutex 保护缓存访问 |

---

## 7. 参考资料

- KLayout 源码：https://github.com/KLayout/klayout
- KLayout 渲染模块：`src/lay/layView.cc`
- Qt 渲染优化：https://doc.qt.io/qt-5/qpainter.html