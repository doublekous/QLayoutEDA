/**
 * @file CellRenderCache.h
 * @brief Cell 渲染缓存 - 参考 KLayout 实现
 */

#ifndef CELLRENDERCACHE_H
#define CELLRENDERCACHE_H

#include <QImage>
#include <QHash>
#include <QString>
#include <QPointF>
#include <QRectF>
#include <QMutex>
#include <QElapsedTimer>

namespace QLayoutEDA {

/**
 * @brief Cell 缓存键
 * 
 * 参考 KLayout: 不含位移的变换作为键
 * 位移在绘制时应用，这样相同变换的 Cell 可以复用缓存
 */
struct CellCacheKey {
    QString cellName;       ///< Cell 名称
    double scale;           ///< 缩放比例
    double rotation;        ///< 旋转角度（度）
    bool mirrorX;           ///< X 镜像
    int depth;              ///< 递归深度（用于区分不同层次）
    
    CellCacheKey()
        : scale(1.0), rotation(0), mirrorX(false), depth(0) {}
    
    CellCacheKey(const QString& name, double s, double r, bool m, int d)
        : cellName(name), scale(s), rotation(r), mirrorX(m), depth(d) {}
    
    bool operator==(const CellCacheKey& other) const {
        return cellName == other.cellName &&
               qFuzzyCompare(scale, other.scale) &&
               qFuzzyCompare(rotation, other.rotation) &&
               mirrorX == other.mirrorX &&
               depth == other.depth;
    }
    
    bool operator<(const CellCacheKey& other) const {
        if (cellName != other.cellName) return cellName < other.cellName;
        if (!qFuzzyCompare(scale, other.scale)) return scale < other.scale;
        if (!qFuzzyCompare(rotation, other.rotation)) return rotation < other.rotation;
        if (mirrorX != other.mirrorX) return mirrorX < other.mirrorX;
        return depth < other.depth;
    }
};

/**
 * @brief Cell 缓存值
 * 
 * 参考 KLayout: 存储渲染后的图像和偏移量
 */
struct CellCacheValue {
    QImage fillImage;       ///< 填充图像
    QImage frameImage;      ///< 轮廓图像（可选）
    QPointF renderOffset;   ///< 渲染时的偏移（用于绘制时调整位置）
    QRectF boundingBox;     ///< Cell 边界框
    qint64 lastAccessTime;  ///< 最后访问时间（用于 LRU）
    size_t hits;            ///< 命中次数
    
    CellCacheValue()
        : lastAccessTime(0), hits(0) {}
};

/**
 * @brief Cell 渲染缓存
 * 
 * 参考 KLayout layRedrawThreadWorker 的 m_cell_cache 实现
 * 
 * 核心优化：
 * 1. 相同变换的 Cell 只渲染一次
 * 2. SREF 引用时直接复制缓存图像
 * 3. LRU 淘汰策略控制内存
 */
class CellRenderCache {
public:
    explicit CellRenderCache(int maxCells = 100, qint64 maxMemory = 200 * 1024 * 1024);
    ~CellRenderCache() = default;
    
    /**
     * @brief 获取缓存的 Cell 图像
     * @param key 缓存键
     * @param fill 输出填充图像
     * @param offset 输出偏移量
     * @return 是否命中
     */
    bool get(const CellCacheKey& key, QImage& fill, QPointF& offset);
    
    /**
     * @brief 存入缓存
     */
    void put(const CellCacheKey& key, const QImage& fill, 
             const QPointF& offset, const QRectF& boundingBox);
    
    /**
     * @brief 检查是否有缓存
     */
    bool has(const CellCacheKey& key) const;
    
    /**
     * @brief 清空缓存
     */
    void clear();
    
    /**
     * @brief 使指定 Cell 缓存失效
     */
    void invalidate(const QString& cellName);
    
    /**
     * @brief 统计信息
     */
    struct Stats {
        int totalCells;       ///< 缓存的 Cell 总数
        qint64 totalMemory;   ///< 总内存占用（字节）
        size_t totalHits;     ///< 总命中次数
        size_t totalMisses;   ///< 总未命中次数
        double hitRate;       ///< 命中率
    };
    Stats getStats() const;
    
    /**
     * @brief 打印统计信息
     */
    void printStats() const;
    
    /**
     * @brief 设置最大缓存数量
     */
    void setMaxCells(int maxCells);
    
    /**
     * @brief 设置最大内存占用
     */
    void setMaxMemory(qint64 bytes);
    
private:
    /**
     * @brief 淘汰最久未使用的缓存
     */
    void evictOldest();
    
    /**
     * @brief 计算图像内存大小
     */
    qint64 calculateImageSize(const QImage& image) const;
    
    QHash<CellCacheKey, CellCacheValue> m_cache;
    mutable QMutex m_mutex;
    
    int m_maxCells;
    qint64 m_maxMemory;
    qint64 m_currentMemory;
    
    mutable size_t m_hits;
    mutable size_t m_misses;
    
    QElapsedTimer m_timer;
};

/**
 * @brief CellCacheKey 的 qHash 实现
 */
inline uint qHash(const CellCacheKey& key, uint seed = 0) {
    QtPrivate::QHashCombine hash;
    seed = hash(seed, key.cellName);
    seed = hash(seed, key.scale);
    seed = hash(seed, key.rotation);
    seed = hash(seed, key.mirrorX);
    seed = hash(seed, key.depth);
    return seed;
}

} // namespace QLayoutEDA

#endif // CELLRENDERCACHE_H