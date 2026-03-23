/**
 * @file SelectionEngine.h
 * @brief 选择引擎 - 处理图形的选择操作
 * 
 * Issue #7: Shape Selection and Move Functionality
 * Author: eda_graphic_algo
 */

#ifndef SELECTION_ENGINE_H
#define SELECTION_ENGINE_H

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QSet>
#include <memory>

// 前向声明
namespace QLayoutEDA {
    class GDSData;
}

namespace QLayoutEDA {

/**
 * @brief 对象唯一标识符
 */
struct ObjectId {
    QString structureName;  // 所属 Cell 名称
    int layer;              // 层号
    int index;              // 在该层中的索引
    enum Type { Boundary, Path, SREF, AREF } type;
    
    bool operator==(const ObjectId& other) const {
        return structureName == other.structureName &&
               layer == other.layer &&
               index == other.index &&
               type == other.type;
    }
    
    bool operator<(const ObjectId& other) const {
        if (structureName != other.structureName) return structureName < other.structureName;
        if (layer != other.layer) return layer < other.layer;
        if (index != other.index) return index < other.index;
        return type < other.type;
    }
};

/**
 * @brief 选中的对象
 */
struct SelectedObject {
    ObjectId id;
    QString structureName;
    int layer;
    int index;
    enum ObjectType { Boundary, Path, SREF, AREF } objectType;
    
    bool operator==(const SelectedObject& other) const {
        return id == other.id;
    }
    
    uint qHash(const SelectedObject& obj, uint seed = 0) {
        return qHash(obj.id.structureName, seed) ^ 
               qHash(obj.id.layer, seed) ^ 
               qHash(obj.id.index, seed);
    }
};

/**
 * @brief 选择结果
 */
struct SelectionResult {
    QVector<ObjectId> selectedIds;
    int count() const { return selectedIds.size(); }
    bool isEmpty() const { return selectedIds.isEmpty(); }
    void clear() { selectedIds.clear(); }
};

/**
 * @brief 选择引擎类
 * 
 * 负责处理图形的选择操作，包括点选、框选、全选等功能。
 */
class SelectionEngine : public QObject {
    Q_OBJECT

public:
    explicit SelectionEngine(QObject* parent = nullptr);
    ~SelectionEngine() override = default;

    // 数据源设置
    void setDataSource(std::shared_ptr<GDSData> data);
    std::shared_ptr<GDSData> dataSource() const { return m_data; }

    // 容差设置
    void setTolerance(double tolerance);
    double tolerance() const { return m_tolerance; }

    // ============================================================
    // 选择操作
    // ============================================================
    
    /**
     * @brief 点选操作
     * @param worldPos 世界坐标位置
     * @param tolerance 选择容差（屏幕像素）
     * @return 选中的对象ID列表
     */
    QVector<ObjectId> pickPoint(const QPointF& worldPos, double tolerance = -1);
    
    /**
     * @brief 框选操作
     * @param worldRect 世界坐标矩形
     * @return 选中的对象ID列表
     */
    QVector<ObjectId> pickRect(const QRectF& worldRect);
    
    /**
     * @brief 全选操作
     * @return 选择结果
     */
    SelectionResult selectAll();
    
    /**
     * @brief 添加到当前选择
     * @param ids 要添加的对象ID
     */
    void addToSelection(const QVector<ObjectId>& ids);
    
    /**
     * @brief 从当前选择移除
     * @param ids 要移除的对象ID
     */
    void removeFromSelection(const QVector<ObjectId>& ids);

    // ============================================================
    // 选择管理
    // ============================================================
    
    /**
     * @brief 清除选择
     */
    void clearSelection();
    
    /**
     * @brief 反选
     * @return 新的选择结果
     */
    SelectionResult invertSelection();
    
    /**
     * @brief 按层过滤选择
     * @param layer 层号
     * @return 过滤后的对象集合
     */
    QSet<SelectedObject> filterByLayer(int layer);
    
    /**
     * @brief 按类型过滤选择
     * @param type 对象类型
     * @return 过滤后的对象集合
     */
    QSet<SelectedObject> filterByType(SelectedObject::ObjectType type);
    
    /**
     * @brief 获取选中数量
     */
    int selectedCount() const { return m_selectedObjects.size(); }
    
    /**
     * @brief 是否有选中对象
     */
    bool hasSelection() const { return !m_selectedObjects.isEmpty(); }
    
    /**
     * @brief 获取所有选中对象
     */
    QSet<SelectedObject> selectedObjects() const { return m_selectedObjects; }

signals:
    /**
     * @brief 选择变化信号
     */
    void selectionChanged();
    
    /**
     * @brief 选择完成信号
     * @param count 选中数量
     */
    void selectionCompleted(int count);

private:
    // 内部辅助方法
    bool isPointInsideBoundary(const QPointF& point, const ObjectId& id);
    bool isRectIntersectBoundary(const QRectF& rect, const ObjectId& id);
    ObjectId createObjectId(const QString& structureName, int layer, int index, ObjectId::Type type);

private:
    std::shared_ptr<GDSData> m_data;
    double m_tolerance = 5.0;
    QSet<SelectedObject> m_selectedObjects;
    bool m_addToExistingSelection = false;
};

} // namespace QLayoutEDA

// 使 SelectedObject 可用于 QSet
inline uint qHash(const QLayoutEDA::SelectedObject& obj, uint seed = 0) {
    return qHash(obj.id.structureName, seed) ^ 
           qHash(obj.id.layer, seed) ^ 
           qHash(obj.id.index, seed);
}

#endif // SELECTION_ENGINE_H