# KLayout 缓存策略深度分析

## 一、KLayout 的两层缓存

### 1. Image Cache（视图缓存）
- 缓存完整视图的渲染结果
- 精确匹配（缩放+偏移+尺寸）
- 用于相同视图快速恢复

### 2. Cell Cache（Cell 缓存）- **关键优化**
- 缓存每个 Cell 的渲染结果
- 缓存键：Cell索引 + 变换矩阵（不含位移）
- **缩放变化后仍然可以复用**

## 二、Cell Cache 如何处理缩放

```cpp
// KLayout 源码
CellCacheKey key(to_level - level, ci, trans_wo_disp);
// trans_wo_disp = 变换矩阵，但位移设为0

// 缓存的是 Cell 在特定缩放下的渲染结果
// 缩放变化 = 新的缓存键 = 重新渲染
```

**关键点**：KLayout 的 Cell Cache 在缩放变化后需要重新渲染，但：
1. 只渲染当前缩放级别
2. 异步渲染，不阻塞UI
3. 渲染完成后更新显示

## 三、为什么 KLayout 缩放流畅

1. **异步渲染** - 后台线程渲染，UI不阻塞
2. **增量更新** - 500ms快照，渐进显示
3. **层次化裁剪** - 只渲染可见区域
4. **缓存复用** - 相同变换的Cell不重复渲染

## 四、当前 QLayoutEDA 问题

| 问题 | 说明 |
|------|------|
| ❌ 同步渲染 | 渲染阻塞UI，导致卡顿 |
| ❌ Cell Cache 无 put | 每次都重新渲染所有SREF |
| ❌ 无增量更新 | 每次全屏重绘 |

## 五、正确优化方案

### 方案1：异步渲染（最重要）
```cpp
// 后台线程渲染
QFuture<QImage> future = QtConcurrent::run([this]() {
    QImage image = renderToImage();
    return image;
});

// 渲染完成后更新
watcher->setFuture(future);
connect(watcher, &QFutureWatcher::finished, [this]() {
    m_cachedImage = future.result();
    update();
});
```

### 方案2：正确实现 Cell Cache
```cpp
// 渲染到离屏图像
QImage cellImage(width, height);
QPainter cellPainter(&cellImage);
renderCellToImage(cellPainter, cell);

// 存入缓存
m_cellCache->put(key, cellImage, offset);

// 绘制到屏幕
painter.drawImage(pos, cellImage);
```

## 六、实施优先级

| 优先级 | 任务 | 效果 |
|--------|------|------|
| P0 | 异步渲染 | UI不阻塞 |
| P0 | Cell Cache put | SREF复用 |
| P1 | 增量更新 | 平滑过渡 |

---

**文档版本**: v1.0
**日期**: 2026-03-23