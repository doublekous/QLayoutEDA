/**
 * @file SelectionEngine.h
 * @brief 图形选择引擎
 */

#pragma once

#include "core/Types.h"
#include "QuadTree.h"
#include <QObject>
#include <QSet>
#include <QPointF>
#include <QRectF>

namespace QLayoutEDA {

class GDSData;
struct GDSBoundary;
struct GDSPath;
struct GDSText;

/**
 * @brief 选中对象
 */
struct SelectedObject {
    enum Type {
        Boundary,
        Path,
        Text,
        SREF,
        AREF
    };
    
    Type type;
    quint32 index;          ///< 在数组中的索引
    QString structureName;  ///< 所属结构
    int layer;
    
    bool operator==(const SelectedObject& other) const {
        return type == other.type && index == other.index && structureName == other.structureName;
    }
};

/**
 * @brief 选择结果
 */
struct SelectionResult {
    QSet<SelectedObject> objects;
    bool isEmpty() const { return objects.isEmpty(); }
    int count() const { return objects.size(); }
};

/**
 * @brief 图形选择引擎
 * 
 * 支持：
 * - 点选（Point Selection）
 * - 框选（Rect Selection）
 * - 图层过滤
 * - 容差控制
 */
class SelectionEngine : public QObject {
    Q_OBJECT
    
public:
    explicit SelectionEngine(QObject* parent = nullptr);
    ~SelectionEngine() override = default;
    
    /**
     * @brief 设置数据源
     */
    void setDataSource(GDSDataPtr data);
    
    /**
     * @brief 设置空间索引
     */
    void setSpatialIndex(QuadTree* index);
    
    /**
     * @brief 设置选择容差（像素）
     */
    void setTolerance(double pixels);
    double tolerance() const { return m_tolerance; }
    
    /**
     * @brief 点选
     * @param worldPoint 世界坐标点
     * @param addToSelection 是否添加到现有选择
     * @return 选择结果
     */
    SelectionResult pickPoint(const QPointF& worldPoint, bool addToSelection = false);
    
    /**
     * @brief 框选
     * @param worldRect 世界坐标矩形
     * @param addToSelection 是否添加到现有选择
     * @return 选择结果
     */
    SelectionResult pickRect(const QRectF& worldRect, bool addToSelection = false);
    
    /**
     * @brief 清除选择
     */
    void clearSelection();
    
    /**
     * @brief 获取当前选择
     */
    const QSet<SelectedObject>& currentSelection() const { return m_currentSelection; }
    
    /**
     * @brief 按图层过滤选择
     */
    QSet<SelectedObject> filterByLayer(int layer) const;
    
    /**
     * @brief 按类型过滤选择
     */
    QSet<SelectedObject> filterByType(SelectedObject::Type type) const;
    
    /**
     * @brief 获取选中数量
     */
    int selectedCount() const { return m_currentSelection.size(); }
    
    /**
     * @brief 是否有选择
     */
    bool hasSelection() const { return !m_currentSelection.isEmpty(); }
    
    /**
     * @brief 选择全部
     */
    SelectionResult selectAll();
    
    /**
     * @brief 反选
     */
    SelectionResult invertSelection();
    
signals:
    /**
     * @brief 选择改变信号
     */
    void selectionChanged(const QSet<SelectedObject>& selection);
    
private:
    /**
     * @brief 测试点是否在多边形内
     */
    bool pointInPolygon(const QPointF& point, const QVector<DbPoint>& polygon) const;
    
    /**
     * @brief 测试点是否在路径附近
     */
    bool pointNearPath(const QPointF& point, const GDSPath& path, double tolerance) const;
    
    /**
     * @brief 测试矩形是否与多边形相交
     */
    bool rectIntersectsPolygon(const QRectF& rect, const QVector<DbPoint>& polygon) const;
    
    /**
     * @brief 获取 Boundary 的边界框
     */
    QRectF getBoundaryBounds(const GDSBoundary& boundary) const;
    
    /**
     * @brief 获取 Path 的边界框
     */
    QRectF getPathBounds(const GDSPath& path) const;
    
    GDSDataPtr m_data;
    QuadTree* m_spatialIndex = nullptr;
    
    double m_tolerance = 5.0;  // 默认 5 像素容差
    QString m_currentStructure;
    
    QSet<SelectedObject> m_currentSelection;
};

} // namespace QLayoutEDA

// qHash 实现
inline uint qHash(const QLayoutEDA::SelectedObject& obj, uint seed = 0) {
    return qHash(obj.type, seed) ^ qHash(obj.index, seed) ^ qHash(obj.structureName, seed);
}