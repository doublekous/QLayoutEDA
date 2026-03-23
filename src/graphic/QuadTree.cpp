/**
 * @file QuadTree.cpp
 * @brief 四叉树空间索引实现
 */

#include "QuadTree.h"
#include "../core/Types.h"
#include <QDebug>
#include <QtAlgorithms>

namespace QLayoutEDA {

//=============================================================================
// QuadTreeNode 实现
//=============================================================================

QuadTreeNode::QuadTreeNode(const QRectF& bounds, int depth)
    : m_bounds(bounds)
    , m_depth(depth)
{
    m_children[0] = m_children[1] = m_children[2] = m_children[3] = nullptr;
}

QuadTreeNode::~QuadTreeNode()
{
    for (int i = 0; i < 4; ++i) {
        delete m_children[i];
    }
}

void QuadTreeNode::insert(const ShapeProxy& shape)
{
    // 如果不是叶子节点，递归插入到子节点
    if (!isLeaf()) {
        int idx = getChildIndex(shape.bounds);
        if (idx >= 0) {
            m_children[idx]->insert(shape);
        } else {
            // 跨越多个子节点，存储在当前节点
            m_shapes.append(shape);
        }
        return;
    }
    
    // 叶子节点：直接存储
    m_shapes.append(shape);
    
    // 检查是否需要分裂
    if (m_shapes.size() > MAX_SHAPES && m_depth < MAX_DEPTH) {
        subdivide();
    }
}

void QuadTreeNode::subdivide()
{
    double halfW = m_bounds.width() / 2.0;
    double halfH = m_bounds.height() / 2.0;
    double x = m_bounds.left();
    double y = m_bounds.top();
    
    // 创建四个子节点
    // 0=左上, 1=右上, 2=左下, 3=右下
    m_children[0] = new QuadTreeNode(QRectF(x, y, halfW, halfH), m_depth + 1);
    m_children[1] = new QuadTreeNode(QRectF(x + halfW, y, halfW, halfH), m_depth + 1);
    m_children[2] = new QuadTreeNode(QRectF(x, y + halfH, halfW, halfH), m_depth + 1);
    m_children[3] = new QuadTreeNode(QRectF(x + halfW, y + halfH, halfW, halfH), m_depth + 1);
    
    // 重新分配图形到子节点
    QVector<ShapeProxy> remaining;
    for (const auto& shape : m_shapes) {
        int idx = getChildIndex(shape.bounds);
        if (idx >= 0) {
            m_children[idx]->insert(shape);
        } else {
            remaining.append(shape);
        }
    }
    m_shapes = remaining;
}

int QuadTreeNode::getChildIndex(const QRectF& shapeBounds) const
{
    if (isLeaf()) return -1;
    
    double midX = m_bounds.left() + m_bounds.width() / 2.0;
    double midY = m_bounds.top() + m_bounds.height() / 2.0;
    
    bool left = shapeBounds.right() < midX;
    bool right = shapeBounds.left() >= midX;
    bool top = shapeBounds.bottom() < midY;
    bool bottom = shapeBounds.top() >= midY;
    
    if (left && top) return 0;      // 左上
    if (right && top) return 1;     // 右上
    if (left && bottom) return 2;   // 左下
    if (right && bottom) return 3;  // 右下
    
    return -1; // 跨越多个象限
}

QVector<ShapeProxy> QuadTreeNode::query(const QRectF& viewport) const
{
    QVector<ShapeProxy> result;
    query(viewport, result, -1);
    return result;
}

void QuadTreeNode::query(const QRectF& viewport, QVector<ShapeProxy>& result, int maxCount) const
{
    // 检查视口是否与节点相交
    if (!m_bounds.intersects(viewport)) {
        return;
    }
    
    // 如果达到最大数量，停止查询
    if (maxCount > 0 && result.size() >= maxCount) {
        return;
    }
    
    // 添加当前节点的图形
    for (const auto& shape : m_shapes) {
        if (shape.bounds.intersects(viewport)) {
            result.append(shape);
            if (maxCount > 0 && result.size() >= maxCount) {
                return;
            }
        }
    }
    
    // 递归查询子节点
    if (!isLeaf()) {
        for (int i = 0; i < 4; ++i) {
            if (m_children[i] && m_children[i]->m_bounds.intersects(viewport)) {
                m_children[i]->query(viewport, result, maxCount);
                if (maxCount > 0 && result.size() >= maxCount) {
                    return;
                }
            }
        }
    }
}

int QuadTreeNode::getShapeCount() const
{
    int count = m_shapes.size();
    if (!isLeaf()) {
        for (int i = 0; i < 4; ++i) {
            if (m_children[i]) {
                count += m_children[i]->getShapeCount();
            }
        }
    }
    return count;
}

void QuadTreeNode::clear()
{
    m_shapes.clear();
    for (int i = 0; i < 4; ++i) {
        delete m_children[i];
        m_children[i] = nullptr;
    }
}

//=============================================================================
// QuadTree 实现
//=============================================================================

QuadTree::QuadTree()
    : m_root(nullptr)
{
}

QuadTree::~QuadTree()
{
    clear();
}

void QuadTree::build(const QVector<GDSBoundary>& boundaries,
                     const QVector<GDSPath>& paths,
                     const QVector<GDSText>& texts)
{
    clear();
    
    if (boundaries.isEmpty() && paths.isEmpty() && texts.isEmpty()) {
        return;
    }
    
    // 计算总边界
    m_totalBounds = calculateBounds(boundaries, paths);
    
    // 扩展边界为正方形（简化分裂逻辑）
    double size = qMax(m_totalBounds.width(), m_totalBounds.height());
    QPointF center = m_totalBounds.center();
    m_totalBounds = QRectF(center.x() - size/2, center.y() - size/2, size, size);
    
    // 创建根节点
    m_root = new QuadTreeNode(m_totalBounds);
    
    // 插入所有图形
    int totalShapes = 0;
    
    // 插入 Boundary
    for (int i = 0; i < boundaries.size(); ++i) {
        const auto& b = boundaries[i];
        if (b.points.isEmpty()) continue;
        
        // 计算边界框（转换为微米）
        double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x / 1000.0);
            maxX = qMax(maxX, p.x / 1000.0);
            minY = qMin(minY, p.y / 1000.0);
            maxY = qMax(maxY, p.y / 1000.0);
        }
        
        ShapeProxy proxy(ShapeProxy::Boundary, i, b.layer, QRectF(minX, minY, maxX-minX, maxY-minY));
        m_root->insert(proxy);
        totalShapes++;
        
        if (totalShapes % 10000 == 0) {
            qDebug() << "四叉树构建进度:" << totalShapes;
        }
    }
    
    // 插入 Path
    for (int i = 0; i < paths.size(); ++i) {
        const auto& p = paths[i];
        if (p.points.isEmpty()) continue;
        
        double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
        for (const auto& pt : p.points) {
            minX = qMin(minX, pt.x / 1000.0);
            maxX = qMax(maxX, pt.x / 1000.0);
            minY = qMin(minY, pt.y / 1000.0);
            maxY = qMax(maxY, pt.y / 1000.0);
        }
        
        ShapeProxy proxy(ShapeProxy::Path, i, p.layer, QRectF(minX, minY, maxX-minX, maxY-minY));
        m_root->insert(proxy);
        totalShapes++;
    }
    
    // 插入 Text
    for (int i = 0; i < texts.size(); ++i) {
        const auto& t = texts[i];
        double x = t.position.x / 1000.0;
        double y = t.position.y / 1000.0;
        
        ShapeProxy proxy(ShapeProxy::Text, i, t.layer, QRectF(x-5, y-5, 10, 10));
        m_root->insert(proxy);
        totalShapes++;
    }
    
    qDebug() << "四叉树构建完成: 总图形数=" << totalShapes;
}

QVector<ShapeProxy> QuadTree::query(const QRectF& viewport) const
{
    if (!m_root) return QVector<ShapeProxy>();
    return m_root->query(viewport);
}

void QuadTree::query(const QRectF& viewport, QVector<ShapeProxy>& result, int maxCount) const
{
    if (!m_root) return;
    m_root->query(viewport, result, maxCount);
}

void QuadTree::clear()
{
    delete m_root;
    m_root = nullptr;
}

QRectF QuadTree::calculateBounds(const QVector<GDSBoundary>& boundaries,
                                  const QVector<GDSPath>& paths) const
{
    double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
    
    for (const auto& b : boundaries) {
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x / 1000.0);
            maxX = qMax(maxX, p.x / 1000.0);
            minY = qMin(minY, p.y / 1000.0);
            maxY = qMax(maxY, p.y / 1000.0);
        }
    }
    
    for (const auto& p : paths) {
        for (const auto& pt : p.points) {
            minX = qMin(minX, pt.x / 1000.0);
            maxX = qMax(maxX, pt.x / 1000.0);
            minY = qMin(minY, pt.y / 1000.0);
            maxY = qMax(maxY, pt.y / 1000.0);
        }
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QuadTree::Stats QuadTree::getStats() const
{
    Stats stats = {0, 0, 0, 0};
    if (!m_root) return stats;
    
    stats.totalNodes = 1;
    stats.totalShapes = m_root->getShapeCount();
    stats.maxDepth = QuadTreeNode::MAX_DEPTH;
    stats.leafNodes = 1;
    
    return stats;
}

} // namespace QLayoutEDA