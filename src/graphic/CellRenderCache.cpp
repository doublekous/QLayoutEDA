/**
 * @file CellRenderCache.cpp
 * @brief Cell 渲染缓存实现 - 参考 KLayout
 */

#include "CellRenderCache.h"
#include <QDebug>
#include <QMutexLocker>

namespace QLayoutEDA {

//=============================================================================
// CellRenderCache
//=============================================================================

CellRenderCache::CellRenderCache(int maxCells, qint64 maxMemory)
    : m_maxCells(maxCells)
    , m_maxMemory(maxMemory)
    , m_currentMemory(0)
    , m_hits(0)
    , m_misses(0)
{
    m_timer.start();
}

bool CellRenderCache::get(const CellCacheKey& key, QImage& fill, QPointF& offset)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_misses++;
        return false;
    }
    
    // 更新访问信息
    it->lastAccessTime = m_timer.elapsed();
    it->hits++;
    m_hits++;
    
    // 复制图像和偏移
    fill = it->fillImage;
    offset = it->renderOffset;
    
    return true;
}

void CellRenderCache::put(const CellCacheKey& key, const QImage& fill,
                           const QPointF& offset, const QRectF& boundingBox)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否需要淘汰
    qint64 imageSize = calculateImageSize(fill);
    
    while ((m_cache.size() >= m_maxCells || m_currentMemory + imageSize > m_maxMemory) 
           && !m_cache.isEmpty()) {
        evictOldest();
    }
    
    // 创建缓存值
    CellCacheValue value;
    value.fillImage = fill;
    value.renderOffset = offset;
    value.boundingBox = boundingBox;
    value.lastAccessTime = m_timer.elapsed();
    value.hits = 0;
    
    // 如果已存在，先减去旧内存
    if (m_cache.contains(key)) {
        m_currentMemory -= calculateImageSize(m_cache[key].fillImage);
    }
    
    // 存入缓存
    m_cache[key] = value;
    m_currentMemory += imageSize;
    
    qDebug() << "CellCache: 存储" << key.cellName 
             << "缩放:" << key.scale
             << "大小:" << (imageSize / 1024.0) << "KB"
             << "总数:" << m_cache.size()
             << "内存:" << (m_currentMemory / 1024.0 / 1024.0) << "MB";
}

bool CellRenderCache::has(const CellCacheKey& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

void CellRenderCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
    m_currentMemory = 0;
    m_hits = 0;
    m_misses = 0;
}

void CellRenderCache::invalidate(const QString& cellName)
{
    QMutexLocker locker(&m_mutex);
    
    QList<CellCacheKey> keysToRemove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it.key().cellName == cellName) {
            m_currentMemory -= calculateImageSize(it->fillImage);
            keysToRemove.append(it.key());
        }
    }
    
    for (const auto& key : keysToRemove) {
        m_cache.remove(key);
    }
}

CellRenderCache::Stats CellRenderCache::getStats() const
{
    QMutexLocker locker(&m_mutex);
    
    Stats stats;
    stats.totalCells = m_cache.size();
    stats.totalMemory = m_currentMemory;
    stats.totalHits = m_hits;
    stats.totalMisses = m_misses;
    
    size_t total = m_hits + m_misses;
    stats.hitRate = total > 0 ? static_cast<double>(m_hits) / total : 0.0;
    
    return stats;
}

void CellRenderCache::printStats() const
{
    Stats stats = getStats();
    
    qDebug() << "========== Cell Cache 统计 ==========";
    qDebug() << "缓存 Cell 数:" << stats.totalCells;
    qDebug() << "内存占用:" << (stats.totalMemory / 1024.0 / 1024.0) << "MB";
    qDebug() << "命中次数:" << stats.totalHits;
    qDebug() << "未命中次数:" << stats.totalMisses;
    qDebug() << "命中率:" << QString::number(stats.hitRate * 100, 'f', 1) << "%";
    qDebug() << "=====================================";
}

void CellRenderCache::setMaxCells(int maxCells)
{
    QMutexLocker locker(&m_mutex);
    m_maxCells = maxCells;
    
    while (m_cache.size() > m_maxCells) {
        evictOldest();
    }
}

void CellRenderCache::setMaxMemory(qint64 bytes)
{
    QMutexLocker locker(&m_mutex);
    m_maxMemory = bytes;
    
    while (m_currentMemory > m_maxMemory && !m_cache.isEmpty()) {
        evictOldest();
    }
}

void CellRenderCache::evictOldest()
{
    if (m_cache.isEmpty()) return;
    
    // 找到最久未使用的
    CellCacheKey oldestKey;
    qint64 oldestTime = LLONG_MAX;
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->lastAccessTime < oldestTime) {
            oldestTime = it->lastAccessTime;
            oldestKey = it.key();
        }
    }
    
    if (m_cache.contains(oldestKey)) {
        m_currentMemory -= calculateImageSize(m_cache[oldestKey].fillImage);
        m_cache.remove(oldestKey);
        qDebug() << "CellCache: 淘汰" << oldestKey.cellName;
    }
}

qint64 CellRenderCache::calculateImageSize(const QImage& image) const
{
    if (image.isNull()) return 0;
    return static_cast<qint64>(image.sizeInBytes());
}

} // namespace QLayoutEDA