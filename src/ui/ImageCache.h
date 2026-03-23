/**
 * @file ImageCache.h
 * @brief 图像缓存 - 参考 KLayout 实现
 * @author QLayoutEDA Team
 */

#pragma once

#include <QImage>
#include <QHash>
#include <QString>
#include <QElapsedTimer>

namespace QLayoutEDA {

/**
 * @brief 图像缓存键
 */
struct ImageCacheKey {
    QString structureName;      ///< Cell 名称
    double scale;               ///< 缩放级别
    double offsetX;             ///< X 偏移
    double offsetY;             ///< Y 偏移
    int width;                  ///< 图像宽度
    int height;                 ///< 图像高度
    
    ImageCacheKey()
        : scale(0), offsetX(0), offsetY(0), width(0), height(0) {}
    
    ImageCacheKey(const QString& name, double s, double ox, double oy, int w, int h)
        : structureName(name), scale(s), offsetX(ox), offsetY(oy), width(w), height(h) {}
    
    bool operator==(const ImageCacheKey& other) const {
        return structureName == other.structureName
            && qFuzzyCompare(scale, other.scale)
            && qFuzzyCompare(offsetX, other.offsetX)
            && qFuzzyCompare(offsetY, other.offsetY)
            && width == other.width
            && height == other.height;
    }
};

/**
 * @brief ImageCacheKey 哈希函数
 */
inline uint qHash(const ImageCacheKey& key, uint seed = 0)
{
    QtPrivate::QHashCombine hash;
    seed = hash(seed, key.structureName);
    seed = hash(seed, static_cast<qint64>(key.scale * 1000));
    seed = hash(seed, static_cast<qint64>(key.offsetX * 100));
    seed = hash(seed, static_cast<qint64>(key.offsetY * 100));
    seed = hash(seed, key.width);
    seed = hash(seed, key.height);
    return seed;
}

/**
 * @brief 图像缓存条目
 */
struct ImageCacheEntry {
    QImage image;               ///< 缓存的图像
    qint64 lastAccessTime;      ///< 最后访问时间（毫秒）
    bool precious;              ///< fitAll 结果优先保留
    
    ImageCacheEntry()
        : lastAccessTime(0), precious(false) {}
    
    ImageCacheEntry(const QImage& img, bool prec = false)
        : image(img), precious(prec)
    {
        static QElapsedTimer timer;
        if (!timer.isValid()) timer.start();
        lastAccessTime = timer.elapsed();
    }
};

/**
 * @brief 图像缓存管理器
 * 
 * 参考 KLayout 实现：
 * - 缓存最近渲染的图像
 * - fitAll 结果标记为 precious，优先保留
 * - LRU 淘汰策略
 */
class ImageCache {
public:
    explicit ImageCache(int maxSize = 3)
        : m_maxSize(maxSize)
    {
        m_timer.start();
    }
    
    /**
     * @brief 获取缓存的图像
     * @return 缓存图像，未命中返回空图像
     */
    QImage get(const ImageCacheKey& key)
    {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            it->lastAccessTime = m_timer.elapsed();
            return it->image;
        }
        return QImage();
    }
    
    /**
     * @brief 存入缓存
     * @param precious fitAll 结果，优先保留
     */
    void put(const ImageCacheKey& key, const QImage& image, bool precious = false)
    {
        // 如果已存在，更新
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            it->image = image;
            it->precious = precious;
            it->lastAccessTime = m_timer.elapsed();
            return;
        }
        
        // 清理旧的 precious 条目（如果新的是 precious）
        if (precious) {
            for (auto i = m_cache.begin(); i != m_cache.end(); ) {
                if (i->precious && i.key().structureName == key.structureName) {
                    i->precious = false;
                }
                ++i;
            }
        }
        
        // 淘汰旧条目
        while (m_cache.size() >= m_maxSize) {
            evictOldest();
        }
        
        // 添加新条目
        m_cache.insert(key, ImageCacheEntry(image, precious));
    }
    
    /**
     * @brief 清空缓存
     */
    void clear()
    {
        m_cache.clear();
    }
    
    /**
     * @brief 设置最大缓存数量
     */
    void setMaxSize(int size)
    {
        m_maxSize = size;
        while (m_cache.size() > m_maxSize) {
            evictOldest();
        }
    }
    
    /**
     * @brief 获取缓存统计
     */
    struct Stats {
        int count;
        int preciousCount;
        qint64 totalBytes;
    };
    
    Stats getStats() const
    {
        Stats s{0, 0, 0};
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            s.count++;
            if (it->precious) s.preciousCount++;
            s.totalBytes += it->image.sizeInBytes();
        }
        return s;
    }
    
private:
    /**
     * @brief 淘汰最旧的条目
     */
    void evictOldest()
    {
        if (m_cache.isEmpty()) return;
        
        // 先淘汰非 precious 的最旧条目
        qint64 oldestTime = LLONG_MAX;
        ImageCacheKey oldestKey;
        bool found = false;
        
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (!it->precious && it->lastAccessTime < oldestTime) {
                oldestTime = it->lastAccessTime;
                oldestKey = it.key();
                found = true;
            }
        }
        
        // 如果没有非 precious 条目，淘汰最旧的 precious 条目
        if (!found) {
            for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
                if (it->lastAccessTime < oldestTime) {
                    oldestTime = it->lastAccessTime;
                    oldestKey = it.key();
                    found = true;
                }
            }
        }
        
        if (found) {
            m_cache.remove(oldestKey);
        }
    }
    
    QHash<ImageCacheKey, ImageCacheEntry> m_cache;
    int m_maxSize;
    QElapsedTimer m_timer;
};

} // namespace QLayoutEDA