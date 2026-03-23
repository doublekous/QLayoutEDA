/**
 * @file QuadTree.h
 * @brief 四叉树空间索引 - 参考 KLayout 实现
 */

#ifndef QUADTREE_H
#define QUADTREE_H

#include <QRectF>
#include <QVector>
#include <QSharedPointer>
#include <array>

namespace QLayoutEDA {

// 前向声明
struct GDSBoundary;
struct GDSPath;
struct GDSText;

/**
 * @brief 轻量级图形代理 - 不存储顶点，只存储引用
 */
struct ShapeProxy {
    enum Type : quint8 {
        Boundary = 0,
        Path = 1,
        Text = 2
    };
    
    Type type;
    quint32 index;      // 在原始数组中的索引
    qint32 layer;       // 图层
    QRectF bounds;      // 边界框（世界坐标，微米）
    
    ShapeProxy() : type(Boundary), index(0), layer(0) {}
    ShapeProxy(Type t, quint32 idx, qint32 lyr, const QRectF& b)
        : type(t), index(idx), layer(lyr), bounds(b) {}
};

/**
 * @brief 四叉树节点
 */
class QuadTreeNode {
public:
    static constexpr int MAX_DEPTH = 10;      // 最大深度
    static constexpr int MAX_SHAPES = 64;     // 节点最大图形数
    
    QuadTreeNode(const QRectF& bounds, int depth = 0);
    ~QuadTreeNode();
    
    /**
     * @brief 插入图形代理
     */
    void insert(const ShapeProxy& shape);
    
    /**
     * @brief 查询视口内的图形
     * @param viewport 视口范围（世界坐标）
     * @return 图形代理列表
     */
    QVector<ShapeProxy> query(const QRectF& viewport) const;
    
    /**
     * @brief 查询视口内的图形（带数量限制）
     */
    void query(const QRectF& viewport, QVector<ShapeProxy>& result, int maxCount = -1) const;
    
    /**
     * @brief 获取节点边界
     */
    QRectF getBounds() const { return m_bounds; }
    
    /**
     * @brief 获取图形数量
     */
    int getShapeCount() const;
    
    /**
     * @brief 清空节点
     */
    void clear();
    
private:
    void subdivide();
    int getChildIndex(const QRectF& shapeBounds) const;
    bool isLeaf() const { return m_children[0] == nullptr; }
    
    QRectF m_bounds;                        // 节点边界
    int m_depth;                            // 当前深度
    QVector<ShapeProxy> m_shapes;           // 存储的图形
    std::array<QuadTreeNode*, 4> m_children; // 四个子节点
    
    // 子节点索引：0=左上, 1=右上, 2=左下, 3=右下
};

/**
 * @brief 四叉树管理器
 */
class QuadTree {
public:
    QuadTree();
    ~QuadTree();
    
    /**
     * @brief 从 GDS 数据构建四叉树
     */
    void build(const QVector<GDSBoundary>& boundaries,
               const QVector<GDSPath>& paths,
               const QVector<GDSText>& texts);
    
    /**
     * @brief 查询视口内的图形
     */
    QVector<ShapeProxy> query(const QRectF& viewport) const;
    
    /**
     * @brief 查询视口内的图形（带数量限制）
     */
    void query(const QRectF& viewport, QVector<ShapeProxy>& result, int maxCount) const;
    
    /**
     * @brief 获取根节点
     */
    QuadTreeNode* getRoot() const { return m_root; }
    
    /**
     * @brief 清空四叉树
     */
    void clear();
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        int totalNodes;
        int totalShapes;
        int maxDepth;
        int leafNodes;
    };
    Stats getStats() const;
    
private:
    QRectF calculateBounds(const QVector<GDSBoundary>& boundaries,
                           const QVector<GDSPath>& paths) const;
    
    QuadTreeNode* m_root;
    QRectF m_totalBounds;
};

} // namespace QLayoutEDA

#endif // QUADTREE_H