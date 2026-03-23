# KLayout 缩放优化核心逻辑分析

## 一、KLayout Cell Cache 工作流程

### 1.1 缓存条件判断（layRedrawThreadWorker.cc:2140-2160）

```cpp
// 判断是否可以缓存
bool can_cache = (m_bitmap_caching && dynamic_cast<lay::Bitmap *>(fill) != 0);

// Cell 必须完全在搜索区域内
if (vv.size() > 1 || !cell_bbox.inside(vv.front())) {
    can_cache = false;
}

// 只有被多次引用的 Cell 才缓存
if (can_cache && level > 0) {
    // 检查是否有多个父实例
    db::Cell::parent_inst_iterator p = cell.begin_parent_insts();
    size_t n;
    for (n = 0; !p.at_end() && n < 2; ++p, ++n);
    if (n <= 1) {
        can_cache = false;  // 只被引用一次，不需要缓存
    }
}
```

### 1.2 缓存键（不含位移）

```cpp
// 关键：位移不作为缓存键的一部分
db::CplxTrans trans_wo_disp = trans;
trans_wo_disp.disp(db::DVector());  // 清除位移

CellCacheKey key(to_level - level, ci, trans_wo_disp);
```

### 1.3 缓存未命中：渲染到 Bitmap

```cpp
if (cached_cell == m_cell_cache.end()) {
    // 创建 Bitmap
    int width = int(cell_box_trans.width() + 3);
    int height = int(cell_box_trans.height() + 3);
    
    cached_cell->second.fill   = new lay::Bitmap(width, height, 1.0, 1.0);
    cached_cell->second.frame  = new lay::Bitmap(width, height, 1.0, 1.0);
    cached_cell->second.vertex = new lay::Bitmap(width, height, 1.0, 1.0);
    cached_cell->second.text   = new lay::Bitmap(width, height, 1.0, 1.0);
    
    // 渲染到 Bitmap（不是屏幕！）
    draw_layer_wo_cache(from_level, to_level, ci, drawing_trans, vv, level,
                        cached_cell->second.fill, cached_cell->second.frame,
                        cached_cell->second.vertex, cached_cell->second.text, ...);
}
```

### 1.4 缓存命中：直接复制 Bitmap

```cpp
cached_cell->second.hits++;  // 记录命中次数

// 计算绘制位置 = 缓存偏移 + 当前位移
db::Point t = db::Point(cached_cell->second.offset + trans.disp());

// 直接复制 Bitmap 到目标画布
copy_bitmap(cached_cell->second.fill,   fill,   t.x(), t.y());
copy_bitmap(cached_cell->second.frame,  frame,  t.x(), t.y());
copy_bitmap(cached_cell->second.vertex, vertex, t.x(), t.y());
copy_bitmap(cached_cell->second.text,   text,   t.x(), t.y());
```

## 二、QLayoutEDA 正确实现方案

### 2.1 问题分析

当前问题：
1. ❌ Cell Cache 只有 `get`，没有 `put`
2. ❌ 没有渲染到离屏图像
3. ❌ 缓存永远不会命中

### 2.2 正确实现

```cpp
void CanvasWidget::renderCell(...) {
    // ========== 1. 缓存键（不含位移）==========
    CellCacheKey cacheKey(cellName, scale, rotation, mirrorX, toLevel - depth);
    
    // ========== 2. 检查缓存 ==========
    if (m_cellRenderCache && depth > 0) {
        QImage cachedImage;
        QPointF cacheOffset;
        
        if (m_cellRenderCache->get(cacheKey, cachedImage, cacheOffset)) {
            // ✅ 缓存命中！直接绘制
            qDebug() << "[CellCache] HIT:" << cellName 
                     << "scale=" << scale 
                     << "offset=" << cacheOffset;
            
            double drawX = offsetX + cacheOffset.x();
            double drawY = offsetY + cacheOffset.y();
            painter.drawImage(QPointF(drawX, drawY), cachedImage);
            stats.cacheHits++;
            return;
        }
    }
    
    // ========== 3. 缓存未命中：渲染到离屏图像 ==========
    qDebug() << "[CellCache] MISS:" << cellName 
             << "scale=" << scale 
             << "bbox=" << structure->boundingBox;
    
    // 计算需要的图像大小
    QRectF cellBBox = getCellBoundingBox(structure);
    int imageWidth = int(cellBBox.width() * scale) + 4;  // +4 边距
    int imageHeight = int(cellBBox.height() * scale) + 4;
    
    // 限制图像大小（避免内存爆炸）
    imageWidth = qMin(imageWidth, 4096);
    imageHeight = qMin(imageHeight, 4096);
    
    // 创建离屏图像
    QImage offscreenImage(imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
    offscreenImage.fill(Qt::transparent);
    
    // 离屏绘制
    QPainter offscreenPainter(&offscreenImage);
    offscreenPainter.setRenderHint(QPainter::Antialiasing, true);
    
    // 渲染到离屏图像（offset = -cellBBox.topLeft * scale）
    QPointF renderOffset(-cellBBox.left() * scale, -cellBBox.top() * scale);
    renderCellToPainter(offscreenPainter, structure, scale, renderOffset);
    
    // ========== 4. 存入缓存 ==========
    if (m_cellRenderCache && depth > 0) {
        QPointF cacheOffset(cellBBox.left() * scale, cellBBox.top() * scale);
        m_cellRenderCache->put(cacheKey, offscreenImage, cacheOffset, cellBBox);
        
        qDebug() << "[CellCache] STORE:" << cellName 
                 << "size=" << imageWidth << "x" << imageHeight
                 << "memory=" << offscreenImage.sizeInBytes() / 1024 << "KB";
    }
    
    // ========== 5. 绘制到屏幕 ==========
    double drawX = offsetX - cellBBox.left() * scale;
    double drawY = offsetY - cellBBox.top() * scale;
    painter.drawImage(QPointF(drawX, drawY), offscreenImage);
}
```

## 三、关键优化点

| 优化项 | 说明 |
|--------|------|
| **离屏渲染** | 渲染到 QImage，而不是直接到屏幕 |
| **缓存键不含位移** | 相同变换的 SREF 复用缓存 |
| **位移在绘制时应用** | `drawX = offsetX + cacheOffset.x()` |
| **图像大小限制** | 避免 100 万像素以上的图像 |
| **详细日志** | 打印 HIT/MISS/STORE 便于调试 |

## 四、预期效果

| 场景 | 优化前 | 优化后 |
|------|--------|--------|
| 首次 fitAll | 21s | 21s（缓存存储）|
| 再次 fitAll | 21s | **<100ms**（缓存命中）|
| 缩放 | 11-20s | **<1s**（Cell Cache 复用）|

---

**文档版本**: v1.0
**创建日期**: 2026-03-23
**参考源码**: KLayout layRedrawThreadWorker.cc:2140-2220