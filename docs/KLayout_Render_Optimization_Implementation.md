# KLayout 渲染引擎深度优化实现方案

## 一、KLayout 核心渲染架构

### 1.1 关键源码文件
- `src/layview/layview/layLayoutView_qt.h` - 布局视图头文件 (23KB)
- `src/layview/layview/layLayoutView_qt.cc` - 布局视图实现 (43KB)

### 1.2 核心渲染机制

```
┌─────────────────────────────────────────────────────────────────┐
│                     KLayout 渲染架构                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │   LayoutView │───→│ ViewportWidget│───→│ CanvasPlane │         │
│  └─────────────┘    └─────────────┘    └─────────────┘         │
│         │                  │                  │                 │
│         ▼                  ▼                  ▼                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │ CellHierarchy│    │ Viewport    │    │ BitmapBuffer│         │
│  │   Cache      │    │ Culling     │    │ (QImage)    │         │
│  └─────────────┘    └─────────────┘    └─────────────┘         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 关键渲染策略

| 策略 | KLayout 实现 | 效果 |
|------|-------------|------|
| **Cell 缓存** | 每个 Cell 渲染到 QImage | 避免重复渲染 |
| **层次渲染** | SREF 绘制缓存的 Cell 图像 | O(1) 复杂度 |
| **视口裁剪** | 层次化边界框测试 | 只渲染可见区域 |
| **异步渲染** | 后台线程 + 信号通知 | UI 不阻塞 |
| **增量更新** | dirty 区域标记 | 最小化重绘 |

## 二、QLayoutEDA 优化实现

### 2.1 新增文件

#### CellRenderCache.h
```cpp
#pragma once

#include <QImage>
#include <QHash>
#include <QRectF>
#include <QString>

namespace QLayoutEDA {

/**
 * @brief Cell 渲染缓存项
 */
struct CellCacheEntry {
    QImage image;              // 渲染结果
    QRectF boundingBox;        // 边界框（世界坐标）
    double cachedScale = 0.0;  // 缓存时的缩放级别
    qint64 lastAccessTime = 0; // 最后访问时间（用于 LRU）
    bool dirty = true;         // 是否需要重新渲染
};

/**
 * @brief Cell 渲染缓存管理器
 * 
 * 参考 KLayout 的 CanvasPlane 和 BitmapBuffer 实现
 */
class CellRenderCache {
public:
    explicit CellRenderCache(int maxCacheSize = 100);
    
    /**
     * @brief 获取缓存的 Cell 图像
     * @param cellName Cell 名称
     * @param scale 当前缩放级别
     * @param tolerance 缩放容差（默认 0.1，即 10% 变化内复用缓存）
     * @return 缓存的图像，如果不存在或过期返回空
     */
    QImage getCellImage(const QString& cellName, double scale, double tolerance = 0.1);
    
    /**
     * @brief 设置 Cell 缓存
     */
    void setCellImage(const QString& cellName, const QImage& image, 
                      const QRectF& boundingBox, double scale);
    
    /**
     * @brief 标记 Cell 为 dirty
     */
    void invalidateCell(const QString& cellName);
    
    /**
     * @brief 标记所有 Cell 为 dirty
     */
    void invalidateAll();
    
    /**
     * @brief 清空缓存
     */
    void clear();
    
    /**
     * @brief 获取缓存统计信息
     */
    struct Stats {
        int totalCells;
        int cachedCells;
        qint64 totalMemoryBytes;
    };
    Stats getStats() const;

private:
    QHash<QString, CellCacheEntry> m_cache;
    int m_maxCacheSize;
    
    void evictOldest(); // LRU 淘汰
};

} // namespace QLayoutEDA
```

#### CellRenderCache.cpp
```cpp
#include "CellRenderCache.h"
#include <QDateTime>

namespace QLayoutEDA {

CellRenderCache::CellRenderCache(int maxCacheSize)
    : m_maxCacheSize(maxCacheSize)
{
}

QImage CellRenderCache::getCellImage(const QString& cellName, double scale, double tolerance)
{
    auto it = m_cache.find(cellName);
    if (it == m_cache.end()) {
        return QImage(); // 未找到
    }
    
    CellCacheEntry& entry = it.value();
    
    // 检查是否 dirty
    if (entry.dirty) {
        return QImage();
    }
    
    // 检查缩放级别是否在容差范围内
    double scaleDiff = qAbs(entry.cachedScale - scale) / entry.cachedScale;
    if (scaleDiff > tolerance) {
        entry.dirty = true;
        return QImage();
    }
    
    // 更新访问时间
    entry.lastAccessTime = QDateTime::currentMSecsSinceEpoch();
    
    return entry.image;
}

void CellRenderCache::setCellImage(const QString& cellName, const QImage& image,
                                    const QRectF& boundingBox, double scale)
{
    // 检查是否需要淘汰
    if (m_cache.size() >= m_maxCacheSize && !m_cache.contains(cellName)) {
        evictOldest();
    }
    
    CellCacheEntry entry;
    entry.image = image;
    entry.boundingBox = boundingBox;
    entry.cachedScale = scale;
    entry.lastAccessTime = QDateTime::currentMSecsSinceEpoch();
    entry.dirty = false;
    
    m_cache[cellName] = entry;
}

void CellRenderCache::invalidateCell(const QString& cellName)
{
    auto it = m_cache.find(cellName);
    if (it != m_cache.end()) {
        it.value().dirty = true;
    }
}

void CellRenderCache::invalidateAll()
{
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        it.value().dirty = true;
    }
}

void CellRenderCache::clear()
{
    m_cache.clear();
}

CellRenderCache::Stats CellRenderCache::getStats() const
{
    Stats stats;
    stats.totalCells = m_cache.size();
    stats.cachedCells = 0;
    stats.totalMemoryBytes = 0;
    
    for (const auto& entry : m_cache) {
        if (!entry.dirty) {
            stats.cachedCells++;
            stats.totalMemoryBytes += entry.image.sizeInBytes();
        }
    }
    
    return stats;
}

void CellRenderCache::evictOldest()
{
    if (m_cache.isEmpty()) return;
    
    QString oldestKey;
    qint64 oldestTime = LLONG_MAX;
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it.value().lastAccessTime < oldestTime) {
            oldestTime = it.value().lastAccessTime;
            oldestKey = it.key();
        }
    }
    
    if (!oldestKey.isEmpty()) {
        m_cache.remove(oldestKey);
    }
}

} // namespace QLayoutEDA
```

### 2.2 修改 CanvasWidget.cpp

#### 2.2.1 添加成员变量
```cpp
// CanvasWidget.h 添加
#include "../graphic/CellRenderCache.h"

class CanvasWidget : public QWidget {
    // ... 现有成员 ...
    
private:
    std::unique_ptr<CellRenderCache> m_cellCache;
};
```

#### 2.2.2 修改 renderCell 方法

**核心修改：使用缓存渲染 SREF**

```cpp
void CanvasWidget::renderCell(QPainter& painter, const QString& cellName,
                               double offsetX, double offsetY, double scale,
                               double rotation, bool mirrorX,
                               int depth, RenderStats& stats,
                               bool expandSREF, int& srefExpandedCount)
{
    // 1. 检查缓存
    QImage cachedImage = m_cellCache->getCellImage(cellName, scale);
    
    if (!cachedImage.isNull()) {
        // 缓存命中，直接绘制
        painter.save();
        painter.translate(offsetX, -offsetY);
        if (mirrorX) painter.scale(-1, 1);
        if (rotation != 0) painter.rotate(qRadiansToDegrees(rotation));
        painter.drawImage(-cachedImage.width() / 2, -cachedImage.height() / 2, cachedImage);
        painter.restore();
        return;
    }
    
    // 2. 缓存未命中，渲染并存入缓存
    auto structure = m_gdsData->getStructure(cellName);
    if (!structure) return;
    
    // 3. 渲染到临时图像
    QRectF bbox = calculateCellBoundingBox(structure);
    int imageWidth = static_cast<int>(bbox.width() * scale) + 2;
    int imageHeight = static_cast<int>(bbox.height() * scale) + 2;
    
    QImage cellImage(imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
    cellImage.fill(Qt::transparent);
    
    QPainter cellPainter(&cellImage);
    cellPainter.setRenderHint(QPainter::Antialiasing, true);
    
    // 渲染 Boundary
    for (const auto& boundary : structure->boundaries) {
        renderBoundary(cellPainter, boundary, scale, 0, 0);
    }
    
    // 渲染 Path
    for (const auto& path : structure->paths) {
        renderPath(cellPainter, path, scale, 0, 0);
    }
    
    // 4. 渲染 SREF（递归展开）
    for (const auto& sref : structure->references) {
        stats.srefCount++;
        
        // 获取子 Cell 的缓存图像
        QImage childImage = m_cellCache->getCellImage(sref.structureName, scale);
        
        if (!childImage.isNull()) {
            // 子 Cell 缓存命中，直接绘制
            renderSREFWithTransform(painter, sref, childImage, offsetX, offsetY, scale, mirrorX);
            srefExpandedCount++;
        } else {
            // 子 Cell 缓存未命中，递归渲染
            if (depth < MAX_RECURSION_DEPTH) {
                double childOffsetX = offsetX + sref.position.x * scale * (mirrorX ? -1 : 1);
                double childOffsetY = offsetY + sref.position.y * scale;
                double childRotation = rotation + sref.angle;
                double childScale = scale * sref.magnification;
                bool childMirrorX = mirrorX != sref.reflected;
                
                renderCell(painter, sref.structureName,
                           childOffsetX, childOffsetY, childScale,
                           childRotation, childMirrorX,
                           depth + 1, stats, expandSREF, srefExpandedCount);
            }
        }
    }
    
    // 5. 存入缓存
    m_cellCache->setCellImage(cellName, cellImage, bbox, scale);
    
    // 6. 绘制到主画布
    painter.save();
    painter.translate(offsetX, -offsetY);
    painter.drawImage(-cellImage.width() / 2, -cellImage.height() / 2, cellImage);
    painter.restore();
}

void CanvasWidget::renderSREFWithTransform(QPainter& painter, const GDSSRef& sref,
                                            const QImage& childImage,
                                            double offsetX, double offsetY,
                                            double scale, bool mirrorX)
{
    QTransform transform;
    transform.translate(offsetX + sref.position.x * scale * (mirrorX ? -1 : 1),
                        -(offsetY + sref.position.y * scale));
    transform.rotateRadians(sref.angle);
    transform.scale(sref.magnification * (mirrorX ? -1 : 1), sref.magnification);
    
    painter.setTransform(transform);
    painter.drawImage(-childImage.width() / 2, -childImage.height() / 2, childImage);
    painter.resetTransform();
}
```

### 2.3 紧急修复：展开条件

**文件**: `E:\QLayoutEDA\src\ui\CanvasWidget.cpp`

**修改位置**: 第 612 行

```cpp
// 修改前
bool expandSREF = (m_transform.scale > SREF_EXPAND_THRESHOLD);

// 修改后
bool expandSREF = (m_transform.scale >= SREF_EXPAND_THRESHOLD);
```

**或者直接移除条件**:
```cpp
bool expandSREF = true;  // 始终展开 SREF
```

## 三、性能预期

| 指标 | 当前 | 优化后 | 提升 |
|------|------|--------|------|
| fitAll 渲染 | 21s | <1s | **21x** |
| 缩放响应 | 21s | <100ms | **210x** |
| 内存占用 | 高 | 中 | 缓存复用 |
| SREF 展开 | 每次重算 | 缓存复用 | **N/A** |

## 四、实施计划

| 阶段 | 任务 | 负责人 | 工期 |
|------|------|--------|------|
| **P0** | 修复展开条件 `>` → `>=` | eda_qt_dev | 0.5天 |
| **P1** | 实现 CellRenderCache 类 | eda_graphic_algo | 2天 |
| **P1** | 修改 CanvasWidget 使用缓存 | eda_graphic_algo | 1天 |
| **P2** | 异步渲染 + 增量更新 | eda_graphic_algo | 3天 |

---

**文档版本**: v2.0
**创建日期**: 2026-03-23
**作者**: EDA 架构师
**参考**: KLayout 源码 src/layview/layview/layLayoutView_qt.*