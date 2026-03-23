/**
 * @file SelectionEngine.cpp
 * @brief 选择引擎实现
 * 
 * Issue #7: Shape Selection and Move Functionality
 * Author: eda_graphic_algo
 */

#include "SelectionEngine.h"
#include "GDSData.h"
#include "Boundary.h"
#include "Path.h"
#include "Structure.h"

#include <QDebug>
#include <QRectF>

namespace QLayoutEDA {

SelectionEngine::SelectionEngine(QObject* parent)
    : QObject(parent)
    , m_data(nullptr)
    , m_tolerance(5.0)
{
}

void SelectionEngine::setDataSource(std::shared_ptr<GDSData> data)
{
    if (m_data != data) {
        clearSelection();
        m_data = data;
    }
}

void SelectionEngine::setTolerance(double tolerance)
{
    m_tolerance = tolerance;
}

// ============================================================
// 选择操作
// ============================================================

QVector<ObjectId> SelectionEngine::pickPoint(const QPointF& worldPos, double tolerance)
{
    QVector<ObjectId> result;
    
    if (!m_data) {
        qWarning() << "SelectionEngine: No data source set";
        return result;
    }
    
    double tol = (tolerance < 0) ? m_tolerance : tolerance;
    
    // 获取当前显示的 Structure
    QString currentStructure = m_data->currentStructure();
    if (currentStructure.isEmpty()) {
        return result;
    }
    
    // 遍历所有层
    auto structure = m_data->getStructure(currentStructure);
    if (!structure) {
        return result;
    }
    
    // 检查 Boundary
    const auto& boundaries = structure->boundaries();
    for (int layer : boundaries.keys()) {
        const auto& layerBoundaries = boundaries[layer];
        for (int i = 0; i < layerBoundaries.size(); ++i) {
            if (isPointInsideBoundary(worldPos, createObjectId(currentStructure, layer, i, ObjectId::Boundary))) {
                ObjectId id = createObjectId(currentStructure, layer, i, ObjectId::Boundary);
                result.append(id);
            }
        }
    }
    
    // TODO: 检查 Path, SREF, AREF
    
    // 如果不是添加模式，先清除现有选择
    if (!m_addToExistingSelection && !result.isEmpty()) {
        clearSelection();
    }
    
    // 添加到选择集
    for (const auto& id : result) {
        SelectedObject obj;
        obj.id = id;
        obj.structureName = id.structureName;
        obj.layer = id.layer;
        obj.index = id.index;
        obj.objectType = SelectedObject::Boundary;
        m_selectedObjects.insert(obj);
    }
    
    if (!result.isEmpty()) {
        emit selectionChanged();
        emit selectionCompleted(result.size());
    }
    
    return result;
}

QVector<ObjectId> SelectionEngine::pickRect(const QRectF& worldRect)
{
    QVector<ObjectId> result;
    
    if (!m_data) {
        qWarning() << "SelectionEngine: No data source set";
        return result;
    }
    
    // 获取当前显示的 Structure
    QString currentStructure = m_data->currentStructure();
    if (currentStructure.isEmpty()) {
        return result;
    }
    
    auto structure = m_data->getStructure(currentStructure);
    if (!structure) {
        return result;
    }
    
    // 检查 Boundary
    const auto& boundaries = structure->boundaries();
    for (int layer : boundaries.keys()) {
        const auto& layerBoundaries = boundaries[layer];
        for (int i = 0; i < layerBoundaries.size(); ++i) {
            if (isRectIntersectBoundary(worldRect, createObjectId(currentStructure, layer, i, ObjectId::Boundary))) {
                result.append(createObjectId(currentStructure, layer, i, ObjectId::Boundary));
            }
        }
    }
    
    // 如果不是添加模式，先清除现有选择
    if (!m_addToExistingSelection && !result.isEmpty()) {
        clearSelection();
    }
    
    // 添加到选择集
    for (const auto& id : result) {
        SelectedObject obj;
        obj.id = id;
        obj.structureName = id.structureName;
        obj.layer = id.layer;
        obj.index = id.index;
        obj.objectType = SelectedObject::Boundary;
        m_selectedObjects.insert(obj);
    }
    
    if (!result.isEmpty()) {
        emit selectionChanged();
        emit selectionCompleted(result.size());
    }
    
    return result;
}

SelectionResult SelectionEngine::selectAll()
{
    SelectionResult result;
    
    if (!m_data) {
        qWarning() << "SelectionEngine: No data source set";
        return result;
    }
    
    QString currentStructure = m_data->currentStructure();
    if (currentStructure.isEmpty()) {
        return result;
    }
    
    auto structure = m_data->getStructure(currentStructure);
    if (!structure) {
        return result;
    }
    
    // 清除现有选择
    clearSelection();
    
    // 选择所有 Boundary
    const auto& boundaries = structure->boundaries();
    for (int layer : boundaries.keys()) {
        const auto& layerBoundaries = boundaries[layer];
        for (int i = 0; i < layerBoundaries.size(); ++i) {
            ObjectId id = createObjectId(currentStructure, layer, i, ObjectId::Boundary);
            result.selectedIds.append(id);
            
            SelectedObject obj;
            obj.id = id;
            obj.structureName = currentStructure;
            obj.layer = layer;
            obj.index = i;
            obj.objectType = SelectedObject::Boundary;
            m_selectedObjects.insert(obj);
        }
    }
    
    if (!result.isEmpty()) {
        emit selectionChanged();
        emit selectionCompleted(result.count());
    }
    
    return result;
}

void SelectionEngine::addToSelection(const QVector<ObjectId>& ids)
{
    for (const auto& id : ids) {
        SelectedObject obj;
        obj.id = id;
        obj.structureName = id.structureName;
        obj.layer = id.layer;
        obj.index = id.index;
        obj.objectType = static_cast<SelectedObject::ObjectType>(id.type);
        m_selectedObjects.insert(obj);
    }
    
    if (!ids.isEmpty()) {
        emit selectionChanged();
    }
}

void SelectionEngine::removeFromSelection(const QVector<ObjectId>& ids)
{
    for (const auto& id : ids) {
        for (auto it = m_selectedObjects.begin(); it != m_selectedObjects.end(); ) {
            if (it->id == id) {
                it = m_selectedObjects.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    if (!ids.isEmpty()) {
        emit selectionChanged();
    }
}

// ============================================================
// 选择管理
// ============================================================

void SelectionEngine::clearSelection()
{
    if (m_selectedObjects.isEmpty()) {
        return;
    }
    
    m_selectedObjects.clear();
    emit selectionChanged();
}

SelectionResult SelectionEngine::invertSelection()
{
    SelectionResult result;
    
    if (!m_data) {
        return result;
    }
    
    QString currentStructure = m_data->currentStructure();
    if (currentStructure.isEmpty()) {
        return result;
    }
    
    auto structure = m_data->getStructure(currentStructure);
    if (!structure) {
        return result;
    }
    
    // 收集当前选中的 ID
    QSet<ObjectId> currentSelected;
    for (const auto& obj : m_selectedObjects) {
        currentSelected.insert(obj.id);
    }
    
    // 清除选择
    m_selectedObjects.clear();
    
    // 选择所有未选中的对象
    const auto& boundaries = structure->boundaries();
    for (int layer : boundaries.keys()) {
        const auto& layerBoundaries = boundaries[layer];
        for (int i = 0; i < layerBoundaries.size(); ++i) {
            ObjectId id = createObjectId(currentStructure, layer, i, ObjectId::Boundary);
            
            if (!currentSelected.contains(id)) {
                result.selectedIds.append(id);
                
                SelectedObject obj;
                obj.id = id;
                obj.structureName = currentStructure;
                obj.layer = layer;
                obj.index = i;
                obj.objectType = SelectedObject::Boundary;
                m_selectedObjects.insert(obj);
            }
        }
    }
    
    emit selectionChanged();
    emit selectionCompleted(result.count());
    
    return result;
}

QSet<SelectedObject> SelectionEngine::filterByLayer(int layer)
{
    QSet<SelectedObject> result;
    for (const auto& obj : m_selectedObjects) {
        if (obj.layer == layer) {
            result.insert(obj);
        }
    }
    return result;
}

QSet<SelectedObject> SelectionEngine::filterByType(SelectedObject::ObjectType type)
{
    QSet<SelectedObject> result;
    for (const auto& obj : m_selectedObjects) {
        if (obj.objectType == type) {
            result.insert(obj);
        }
    }
    return result;
}

// ============================================================
// 内部辅助方法
// ============================================================

bool SelectionEngine::isPointInsideBoundary(const QPointF& point, const ObjectId& id)
{
    // TODO: 实现真正的点在多边形内部检测
    // 目前使用简单的边界框检测
    
    if (!m_data) {
        return false;
    }
    
    auto structure = m_data->getStructure(id.structureName);
    if (!structure) {
        return false;
    }
    
    const auto& boundaries = structure->boundaries();
    if (!boundaries.contains(id.layer)) {
        return false;
    }
    
    const auto& layerBoundaries = boundaries[id.layer];
    if (id.index < 0 || id.index >= layerBoundaries.size()) {
        return false;
    }
    
    // 获取边界框并检测
    const auto& boundary = layerBoundaries[id.index];
    QRectF bbox = boundary.boundingBox();
    
    // 扩展边界框以包含容差
    bbox.adjust(-m_tolerance, -m_tolerance, m_tolerance, m_tolerance);
    
    return bbox.contains(point);
}

bool SelectionEngine::isRectIntersectBoundary(const QRectF& rect, const ObjectId& id)
{
    if (!m_data) {
        return false;
    }
    
    auto structure = m_data->getStructure(id.structureName);
    if (!structure) {
        return false;
    }
    
    const auto& boundaries = structure->boundaries();
    if (!boundaries.contains(id.layer)) {
        return false;
    }
    
    const auto& layerBoundaries = boundaries[id.layer];
    if (id.index < 0 || id.index >= layerBoundaries.size()) {
        return false;
    }
    
    const auto& boundary = layerBoundaries[id.index];
    QRectF bbox = boundary.boundingBox();
    
    return rect.intersects(bbox);
}

ObjectId SelectionEngine::createObjectId(const QString& structureName, int layer, int index, ObjectId::Type type)
{
    ObjectId id;
    id.structureName = structureName;
    id.layer = layer;
    id.index = index;
    id.type = type;
    return id;
}

} // namespace QLayoutEDA