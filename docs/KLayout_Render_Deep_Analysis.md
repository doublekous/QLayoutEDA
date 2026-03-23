# KLayout 渲染引擎深度分析报告 v2.0

## 一、源码分析范围

### 1.1 已完整分析的文件

| 文件 | 行数 | 核心内容 |
|------|------|----------|
| **layRedrawThread.h** | 140 行 | 多线程渲染调度器定义 |
| **layRedrawThread.cc** | 438 行 | 线程启动、停止、增量重绘、任务调度 |
| **layRedrawThreadWorker.h** | 230 行 | Cell 缓存键/值定义、Worker 定义 |
| **layRedrawThreadWorker.cc** | 2400+ 行 | Cell 缓存实现、视口裁剪、层次渲染 |
| **layBitmap.h** | 200 行 | Bitmap 扫描线渲染接口 |
| **layBitmap.cc** | 700+ 行 | 扫描线填充、Bitmap 合并 |
| **layLayoutCanvas.h** | 280 行 | 图像缓存、画布管理 |
| **layLayoutCanvas.cc** | 1100+ 行 | 图像缓存恢复、增量更新、paint_event |
| **layBitmapRenderer.h** | 300 行 | 渲染器接口 |
| **layRenderer.h** | 350 行 | 渲染器基类 |
| **layViewport.h** | 150 行 | 视口变换管理 |

---

## 二、KLayout 流畅渲染的核心原因

### 2.1 多层缓存架构

```
┌─────────────────────────────────────────────────────────────┐
│                     KLayout 缓存架构                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Level 1: Image Cache (图像缓存)                      │   │
│  │  - 缓存完整视口图像                                   │   │
│  │  - 缓存键：Viewport + Layer 配置                      │   │
│  │  - 支持 "precious" 标记（fitAll 结果优先保留）        │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Level 2: Cell Cache (Cell 缓存)                     │   │
│  │  - 缓存键：层级数 + Cell 索引 + 变换矩阵（不含位移）   │   │
│  │  - 缓存值：4 个 Bitmap (fill/frame/vertex/text)      │   │
│  │  - 相同变换的 Cell 实例复用缓存                       │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Level 3: Cell Variant Cache (Cell 变体缓存)         │   │
│  │  - 避免相同 Cell + 变换重复渲染                       │   │
│  │  - 特别适用于大量 SREF 引用同一 Cell                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Image Cache 实现

**源码位置**: `layLayoutCanvas.cc` 第 420-480 行

```cpp
// 查找可复用的缓存
std::vector <ImageCacheEntry>::iterator c;
for (c = m_image_cache.begin (); c != m_image_cache.end (); ++c) {
    if (! c->opened () && c->equals (m_viewport_l, m_layers)) {
        break;  // 找到匹配的缓存
    }
}

if (c != m_image_cache.end ()) {
    // 缓存命中，直接恢复
    mp_redraw_thread->commit (m_layers, m_viewport_l, resolution (), font_resolution ());
    restore_data (c->data ());  // O(1) 恢复
} else {
    // 缓存未命中，创建新缓存
    m_image_cache.push_back (ImageCacheEntry (m_viewport_l, m_layers, precious));
    mp_redraw_thread->start (...);
}
```

**关键点**：
- fitAll 结果标记为 "precious"，优先保留
- 缓存命中时直接恢复图像数据
- 缓存大小可配置（m_image_cache_size）

### 2.3 Cell Cache 实现

**源码位置**: `layRedrawThreadWorker.cc` 第 2160-2210 行

```cpp
// Cell 缓存键
CellCacheKey key (to_level - level, ci, trans_wo_disp);

// 查找缓存
cell_cache_t::iterator cached_cell = m_cell_cache.find (key);

if (cached_cell == m_cell_cache.end ()) {
    // 缓存未命中：创建新的
    cached_cell = m_cell_cache.insert (std::make_pair (key, CellCacheInfo ())).first;
    
    // 创建 4 个 Bitmap
    cached_cell->second.fill   = new lay::Bitmap (width, height, 1.0, 1.0);
    cached_cell->second.frame  = new lay::Bitmap (width, height, 1.0, 1.0);
    cached_cell->second.vertex = new lay::Bitmap (width, height, 1.0, 1.0);
    cached_cell->second.text   = new lay::Bitmap (width, height, 1.0, 1.0);
    
    // 渲染到缓存
    draw_layer_wo_cache (..., cached_cell->second.fill, ...);
}

// 缓存命中：复制 Bitmap
cached_cell->second.hits++;
copy_bitmap (cached_cell->second.fill,   fill,   t.x (), t.y ());
copy_bitmap (cached_cell->second.frame,  frame,  t.x (), t.y ());
copy_bitmap (cached_cell->second.vertex, vertex, t.x (), t.y ());
copy_bitmap (cached_cell->second.text,   text,   t.x (), t.y ());
```

**关键点**：
- 每个 Cell 渲染到 4 个独立 Bitmap
- 变换矩阵不含位移（位移在复制时应用）
- 相同变换的 SREF 引用只需复制 Bitmap

### 2.4 增量重绘

**源码位置**: `layRedrawThread.cc` 第 120-170 行

```cpp
void RedrawThread::start (...) {
    // 判断是否可以只更新变化部分
    if (! force_redraw && 
        m_valid_region.overlaps (new_region) && 
        m_stored_fp == m_vp_trans.fp_trans ()) {
        
        // 计算需要重绘的区域
        std::vector<db::DBox> parts = subtract_box (new_region, m_valid_region);
        
        // 只重绘变化区域
        m_redraw_regions = parts;
        
        // 计算位移向量
        sv = db::Vector (m_vp_trans * db::DVector (m_last_center - new_region.center ()));
        shift_vector = &sv;
        
    } else {
        // 全屏重绘
        m_redraw_regions.clear ();
        m_redraw_regions.push_back (db::Box (db::Point (0, 0), db::Point (m_width, m_height)));
    }
}
```

**关键点**：
- 平移时只重绘新出现的边缘区域
- 中心区域通过位移复用
- 大幅减少渲染量

### 2.5 多线程渲染

**源码位置**: `layRedrawThread.cc` 第 230-280 行

```cpp
void RedrawThread::do_start (...) {
    // 设置 Worker 数量
    if (nworkers >= 0 && nworkers != num_workers ()) {
        set_num_workers (nworkers);
    }
    
    // 设置绘制任务队列
    // 1. 装饰物 (draw_custom_queue_entry)
    // 2. 可见图层 (0..n)
    // 3. Cell 边界框 (draw_boxes_queue_entry)
    
    if (! m_custom_already_drawn) {
        schedule (new RedrawThreadTask (draw_custom_queue_entry));
    }
    for (int i = 0; i < m_nlayers; ++i) {
        if (m_layers [i].needs_drawing ()) {
            schedule (new RedrawThreadTask (i));
        }
    }
    if (! m_boxes_already_drawn) {
        schedule (new RedrawThreadTask (draw_boxes_queue_entry));
    }
    
    start ();  // 启动多 Worker
}

tl::Worker * RedrawThread::create_worker () {
    return new RedrawThreadWorker (this);
}
```

**关键点**：
- 每个图层独立渲染任务
- 多 Worker 并行处理
- 任务完成标记 enabled=false

### 2.6 Bitmap 扫描线渲染

**源码位置**: `layBitmap.cc` 第 400-500 行

```cpp
void Bitmap::render_fill (std::vector<lay::RenderEdge> &edges) {
    // 扫描线算法
    tl::sort (edges.begin (), edges.end ());
    
    double y = floor (edges.begin ()->y1 ());
    
    while (done != edges.end () && y < height ()) {
        // 更新活动边
        // 按位置排序
        // 填充扫描线
        int c = 0;
        for (e = done; e != todo; ++e) {
            c += e->delta ();
            if (c == 0) {
                fill (yint, x1int, x2int);  // 填充一行
            }
        }
        y += 1.0;
    }
}

void Bitmap::merge (const lay::Bitmap *from, int dx, int dy) {
    // 位运算合并 Bitmap
    for (unsigned int n = n0; n < from_height; ++n) {
        uint32_t *sl_to = scanline (n + dy);
        const uint32_t *sl_from = from->scanline (n);
        for (unsigned int i = 0; i < m; ++i) {
            *sl_to++ |= *sl_from++;  // 位运算合并
        }
    }
}
```

**关键点**：
- 高效扫描线填充算法
- 位运算加速
- Bitmap 合并用于缓存复用

### 2.7 视口裁剪

**源码位置**: `layRedrawThreadWorker.cc` 第 800-900 行

```cpp
void RedrawThreadWorker::draw_layer (...) {
    // 遍历重绘区域
    for (std::vector<db::Box>::const_iterator b = redraw_regions.begin (); 
         b != redraw_regions.end (); ++b) {
        
        // 视口裁剪
        db::Box vp = *b;
        
        // 遍历 Cell 实例
        for (const auto& inst : cell.child_cells ()) {
            // 计算实例边界框
            db::Box inst_bbox = trans * inst.bbox ();
            
            // 视口裁剪判断
            if (! vp.overlaps (inst_bbox)) {
                continue;  // 跳过视口外的实例
            }
            
            // 渲染视口内的实例
            draw_cell (...);
        }
    }
}
```

**关键点**：
- 层次化裁剪（Cell 级别）
- 只渲染视口内的内容
- 大幅减少渲染量

---

## 三、KLayout 与 QLayoutEDA 对比

| 特性 | KLayout | QLayoutEDA (当前) |
|------|---------|-------------------|
| **Image Cache** | ✅ 完整视口缓存 | ❌ 无 |
| **Cell Cache** | ✅ 4 Bitmap + 变换键 | ❌ 无 |
| **Cell Variant Cache** | ✅ 避免重复渲染 | ❌ 无 |
| **增量重绘** | ✅ 边缘区域更新 | ❌ 全屏重绘 |
| **多线程渲染** | ✅ 多 Worker 并行 | ⚠️ 框架存在未启用 |
| **Bitmap 渲染** | ✅ 扫描线 + 位运算 | ❌ QPainter 绑定 |
| **视口裁剪** | ✅ Cell 级别 | ⚠️ 简单实现 |

---

## 四、QLayoutEDA 优化方案

### 4.1 阶段 1：实现 Image Cache（紧急）

**目标**：缓存完整视口图像，fitAll 后再次 fitAll 瞬间完成

**实现**：
```cpp
struct ImageCacheEntry {
    QImage image;
    ViewTransform transform;
    QString structureName;
    qint64 lastAccessTime;
    bool precious;  // fitAll 结果优先保留
};

class ImageCache {
    QHash<QString, ImageCacheEntry> m_cache;
    int m_maxSize = 3;
};
```

### 4.2 阶段 2：实现 Cell Cache

**目标**：相同变换的 SREF 只渲染一次

**实现**：
```cpp
struct CellCacheKey {
    int levels;
    QString cellName;
    double scale;
    double rotation;
    bool mirrorX;
};

struct CellCacheValue {
    QImage fill;
    QImage frame;
    QPointF offset;
    qint64 lastAccessTime;
};

class CellCache {
    QHash<CellCacheKey, CellCacheValue> m_cache;
    int m_maxSize = 100;
};
```

### 4.3 阶段 3：实现增量重绘

**目标**：平移时只更新新区域

**实现**：
```cpp
void CanvasWidget::pan(int dx, int dy) {
    // 计算可复用区域
    QRect reuseRect = calculateReuseRect(dx, dy);
    
    // 位移缓存图像
    m_cachedImage = shiftImage(m_cachedImage, dx, dy);
    
    // 只渲染新区域
    QRect newRegion = calculateNewRegion(dx, dy);
    renderRegion(newRegion);
}
```

### 4.4 阶段 4：启用多线程渲染

**目标**：渲染在后台线程，UI 不阻塞

**实现**：
```cpp
// 启用现有的 RenderCoordinator
m_useAsyncRendering = true;

connect(m_renderCoordinator.get(), &RenderCoordinator::renderComplete,
        this, [this](const RenderResult& result) {
    m_cachedImage = result.image;
    update();
});
```

---

## 五、预期效果

| 指标 | 当前 | 阶段 1 后 | 阶段 2 后 | 最终目标 |
|------|------|-----------|-----------|----------|
| fitAll 首次 | 21s | 21s | 5s | 1-2s |
| fitAll 再次 | 21s | **<100ms** | **<100ms** | **<50ms** |
| 缩放响应 | 11-20s | 10s | **<1s** | **<200ms** |
| 平移响应 | 卡顿 | 卡顿 | 流畅 | **<100ms** |

---

## 六、实施计划

| 阶段 | 任务 | 工期 | 优先级 |
|------|------|------|--------|
| **阶段 0** | 回滚错误修改，恢复基本功能 | 0.5天 | **P0** |
| **阶段 1** | 实现 Image Cache | 1天 | **P0** |
| **阶段 2** | 实现 Cell Cache | 2天 | **P0** |
| **阶段 3** | 实现增量重绘 | 2天 | P1 |
| **阶段 4** | 启用多线程渲染 | 1天 | P1 |

---

**文档版本**: v2.0
**创建日期**: 2026-03-23
**作者**: EDA 架构师
**参考源码**: KLayout v0.29.x (E:\klayout-master)
**分析文件数**: 12 个核心源文件
**总分析代码行数**: 6000+ 行