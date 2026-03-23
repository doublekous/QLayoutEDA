/**
 * @file SelectionEngine.cpp
 * @brief 图形选择引擎实现
 */

#include "SelectionEngine.h"
#include "GeometryAlgorithms.h"
#include "../parser/GDSParser.h"
#include <QDebug>
#include <QLineF>
#include <QtMath>
#include <cmath>

namespace QLayoutEDA {

//=============================================================================
// SelectionEngine
//=============================================================================

SelectionEngine::SelectionEngine(QObject* parent)
    : QObject(parent)
{
}

void SelectionEngine::setDataSource(GDSDataPtr data)
{
    m_data = data;
    if (data && !data->topStructure.isEmpty()) {
        m_currentStructure = data->topStructure;
    }
}

void SelectionEngine::setSpatialIndex(QuadTree* index)
{
    m_spatialIndex = index;
}

void SelectionEngine::setTolerance(double pixels)
{
    m_tolerance = qMax(1.0, pixels);
}

//=============================================================================
// 简化接口实现（使用 QuadTree）
//=============================================================================

QVector<ObjectId> SelectionEngine::pickPoint(const QPointF& pos, double tolerance)
{
    QVector<ObjectId> result;
    
    if (!m_data) {
        return result;
    }
    
    QString targetStructure = m_currentStructure.isEmpty() ? m_data->topStructure : m_currentStructure;
    auto structure = m_data->getStructure(targetStructure);
    if (!structure) {
        return result;
    }
    
    // 优先使用 QuadTree 空间索引
    if (m_spatialIndex) {
        // 构建查询区域
        QRectF queryRect(pos.x() - tolerance, pos.y() - tolerance, 
                         tolerance * 2, tolerance * 2);
        
        // 从 QuadTree 查询候选图形
        QVector<ShapeProxy> candidates = m_spatialIndex->query(queryRect);
        
        // 精确过滤
        double minDist = std::numeric_limits<double>::max();
        ObjectId nearestId;
        bool found = false;
        
        for (const auto& proxy : candidates) {
            // 计算点到图形边界的距离
            double dist = 0;
            double cx = proxy.bounds.center().x();
            double cy = proxy.bounds.center().y();
            dist = std::sqrt((pos.x() - cx) * (pos.x() - cx) + 
                            (pos.y() - cy) * (pos.y() - cy));
            
            // 检查是否在容差范围内且更近
            if (dist < tolerance && dist < minDist) {
                minDist = dist;
                nearestId = ObjectId(static_cast<ObjectId::Type>(proxy.type), 
                                     proxy.index, targetStructure);
                found = true;
            }
        }
        
        if (found) {
            result.append(nearestId);
        }
        
        return result;
    }
    
    // 回退：无 QuadTree 时使用遍历方式
    qint64 dbX = static_cast<qint64>(pos.x() * 1000);  // μm → nm
    qint64 dbY = static_cast<qint64>(pos.y() * 1000);
    qint64 tol = static_cast<qint64>(tolerance * 1000);
    
    double minDist = std::numeric_limits<double>::max();
    ObjectId nearestId;
    bool found = false;
    
    // 检查 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        const auto& boundary = structure->boundaries[i];
        if (boundary.points.size() < 3) continue;
        
        // 快速边界框测试
        qint64 bMinX = LLONG_MAX, bMaxX = LLONG_MIN;
        qint64 bMinY = LLONG_MAX, bMaxY = LLONG_MIN;
        for (const auto& pt : boundary.points) {
            bMinX = qMin(bMinX, pt.x);
            bMaxX = qMax(bMaxX, pt.x);
            bMinY = qMin(bMinY, pt.y);
            bMaxY = qMax(bMaxY, pt.y);
        }
        
        if (dbX < bMinX - tol || dbX > bMaxX + tol ||
            dbY < bMinY - tol || dbY > bMaxY + tol) {
            continue;
        }
        
        // 精确测试
        QPointF testPoint(dbX, dbY);
        if (pointInPolygon(testPoint, boundary.points)) {
            double cx = (bMinX + bMaxX) / 2.0 / 1000.0;
            double cy = (bMinY + bMaxY) / 2.0 / 1000.0;
            double dist = std::sqrt((pos.x() - cx) * (pos.x() - cx) + 
                                   (pos.y() - cy) * (pos.y() - cy));
            if (dist < minDist) {
                minDist = dist;
                nearestId = ObjectId(ObjectId::Boundary, i, targetStructure);
                found = true;
            }
        }
    }
    
    // 检查 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        const auto& path = structure->paths[i];
        if (path.points.size() < 2) continue;
        
        if (pointNearPath(pos, path, tolerance)) {
            // 计算中心距离
            double cx = 0, cy = 0;
            for (const auto& pt : path.points) {
                cx += pt.x / 1000.0;
                cy += pt.y / 1000.0;
            }
            cx /= path.points.size();
            cy /= path.points.size();
            double dist = std::sqrt((pos.x() - cx) * (pos.x() - cx) + 
                                   (pos.y() - cy) * (pos.y() - cy));
            if (dist < minDist) {
                minDist = dist;
                nearestId = ObjectId(ObjectId::Path, i, targetStructure);
                found = true;
            }
        }
    }
    
    // 检查 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        const auto& text = structure->texts[i];
        double tx = text.position.x / 1000.0;
        double ty = text.position.y / 1000.0;
        double dist = std::sqrt((pos.x() - tx) * (pos.x() - tx) + 
                               (pos.y() - ty) * (pos.y() - ty));
        if (dist < tolerance && dist < minDist) {
            minDist = dist;
            nearestId = ObjectId(ObjectId::Text, i, targetStructure);
            found = true;
        }
    }
    
    if (found) {
        result.append(nearestId);
    }
    
    return result;
}

QVector<ObjectId> SelectionEngine::pickRect(const QRectF& rect)
{
    QVector<ObjectId> result;
    
    if (!m_data) {
        return result;
    }
    
    QString targetStructure = m_currentStructure.isEmpty() ? m_data->topStructure : m_currentStructure;
    auto structure = m_data->getStructure(targetStructure);
    if (!structure) {
        return result;
    }
    
    // 优先使用 QuadTree 空间索引
    if (m_spatialIndex) {
        // 从 QuadTree 查询范围内的图形
        QVector<ShapeProxy> candidates = m_spatialIndex->query(rect);
        
        for (const auto& proxy : candidates) {
            result.append(ObjectId(static_cast<ObjectId::Type>(proxy.type), 
                                   proxy.index, targetStructure));
        }
        
        return result;
    }
    
    // 回退：无 QuadTree 时使用遍历方式
    qint64 rectMinX = static_cast<qint64>(rect.left() * 1000);
    qint64 rectMaxX = static_cast<qint64>(rect.right() * 1000);
    qint64 rectMinY = static_cast<qint64>(rect.top() * 1000);
    qint64 rectMaxY = static_cast<qint64>(rect.bottom() * 1000);
    
    // 检查 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        const auto& boundary = structure->boundaries[i];
        if (boundary.points.size() < 3) continue;
        
        qint64 bMinX = LLONG_MAX, bMaxX = LLONG_MIN;
        qint64 bMinY = LLONG_MAX, bMaxY = LLONG_MIN;
        for (const auto& pt : boundary.points) {
            bMinX = qMin(bMinX, pt.x);
            bMaxX = qMax(bMaxX, pt.x);
            bMinY = qMin(bMinY, pt.y);
            bMaxY = qMax(bMaxY, pt.y);
        }
        
        // 边界框相交测试
        if (!(bMaxX < rectMinX || bMinX > rectMaxX ||
              bMaxY < rectMinY || bMinY > rectMaxY)) {
            result.append(ObjectId(ObjectId::Boundary, i, targetStructure));
        }
    }
    
    // 检查 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        const auto& path = structure->paths[i];
        if (path.points.size() < 2) continue;
        
        qint64 pMinX = LLONG_MAX, pMaxX = LLONG_MIN;
        qint64 pMinY = LLONG_MAX, pMaxY = LLONG_MIN;
        for (const auto& pt : path.points) {
            pMinX = qMin(pMinX, pt.x);
            pMaxX = qMax(pMaxX, pt.x);
            pMinY = qMin(pMinY, pt.y);
            pMaxY = qMax(pMaxY, pt.y);
        }
        
        if (!(pMaxX < rectMinX || pMinX > rectMaxX ||
              pMaxY < rectMinY || pMinY > rectMaxY)) {
            result.append(ObjectId(ObjectId::Path, i, targetStructure));
        }
    }
    
    // 检查 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        const auto& text = structure->texts[i];
        qint64 tx = text.position.x;
        qint64 ty = text.position.y;
        
        if (tx >= rectMinX && tx <= rectMaxX && ty >= rectMinY && ty <= rectMaxY) {
            result.append(ObjectId(ObjectId::Text, i, targetStructure));
        }
    }
    
    return result;
}

QVector<ObjectId> SelectionEngine::selectAll(const QString& structureName)
{
    QVector<ObjectId> result;
    
    if (!m_data) {
        return result;
    }
    
    QString targetStructure = structureName.isEmpty() ? m_data->topStructure : structureName;
    auto structure = m_data->getStructure(targetStructure);
    if (!structure) {
        return result;
    }
    
    // 选择所有 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        result.append(ObjectId(ObjectId::Boundary, i, targetStructure));
    }
    
    // 选择所有 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        result.append(ObjectId(ObjectId::Path, i, targetStructure));
    }
    
    // 选择所有 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        result.append(ObjectId(ObjectId::Text, i, targetStructure));
    }
    
    return result;
}

//=============================================================================
// 原有接口实现
//=============================================================================

SelectionResult SelectionEngine::pickPointWithSelection(const QPointF& worldPoint, bool addToSelection)
{
    SelectionResult result;
    
    if (!m_data) return result;
    
    auto structure = m_data->getStructure(m_currentStructure);
    if (!structure) return result;
    
    // 转换世界坐标到数据库单位（假设世界坐标是微米）
    qint64 dbX = static_cast<qint64>(worldPoint.x() * 1000);  // μm → nm
    qint64 dbY = static_cast<qint64>(worldPoint.y() * 1000);
    qint64 tol = static_cast<qint64>(m_tolerance * 1000);  // 容差也转 nm
    
    // 1. 检查 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        const auto& boundary = structure->boundaries[i];
        if (boundary.points.size() < 3) continue;
        
        // 计算边界框
        qint64 bMinX = LLONG_MAX, bMaxX = LLONG_MIN;
        qint64 bMinY = LLONG_MAX, bMaxY = LLONG_MIN;
        for (const auto& pt : boundary.points) {
            bMinX = qMin(bMinX, pt.x);
            bMaxX = qMax(bMaxX, pt.x);
            bMinY = qMin(bMinY, pt.y);
            bMaxY = qMax(bMaxY, pt.y);
        }
        
        // 快速排除
        if (dbX < bMinX - tol || dbX > bMaxX + tol ||
            dbY < bMinY - tol || dbY > bMaxY + tol) {
            continue;
        }
        
        // 精确测试：点是否在多边形内
        if (pointInPolygon(QPointF(dbX, dbY), boundary.points)) {
            SelectedObject obj;
            obj.type = SelectedObject::Boundary;
            obj.index = i;
            obj.structureName = m_currentStructure;
            obj.layer = boundary.layer;
            result.objects.insert(obj);
        }
    }
    
    // 2. 检查 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        const auto& path = structure->paths[i];
        if (path.points.size() < 2) continue;
        
        if (pointNearPath(worldPoint, path, m_tolerance)) {
            SelectedObject obj;
            obj.type = SelectedObject::Path;
            obj.index = i;
            obj.structureName = m_currentStructure;
            obj.layer = path.layer;
            result.objects.insert(obj);
        }
    }
    
    // 3. 检查 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        const auto& text = structure->texts[i];
        // 简化：只检查位置点
        double dx = worldPoint.x() - text.position.x / 1000.0;
        double dy = worldPoint.y() - text.position.y / 1000.0;
        if (dx * dx + dy * dy < m_tolerance * m_tolerance) {
            SelectedObject obj;
            obj.type = SelectedObject::Text;
            obj.index = i;
            obj.structureName = m_currentStructure;
            obj.layer = text.layer;
            result.objects.insert(obj);
        }
    }
    
    // 更新当前选择
    if (!addToSelection) {
        m_currentSelection = result.objects;
    } else {
        m_currentSelection.unite(result.objects);
    }
    
    if (!result.isEmpty()) {
        emit selectionChanged(m_currentSelection);
    }
    
    return result;
}

SelectionResult SelectionEngine::pickRectWithSelection(const QRectF& worldRect, bool addToSelection)
{
    SelectionResult result;
    
    if (!m_data) return result;
    
    auto structure = m_data->getStructure(m_currentStructure);
    if (!structure) return result;
    
    // 转换到数据库单位
    qint64 rectMinX = static_cast<qint64>(worldRect.left() * 1000);
    qint64 rectMaxX = static_cast<qint64>(worldRect.right() * 1000);
    qint64 rectMinY = static_cast<qint64>(worldRect.top() * 1000);
    qint64 rectMaxY = static_cast<qint64>(worldRect.bottom() * 1000);
    
    // 1. 检查 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        const auto& boundary = structure->boundaries[i];
        if (boundary.points.size() < 3) continue;
        
        // 计算边界框
        qint64 bMinX = LLONG_MAX, bMaxX = LLONG_MIN;
        qint64 bMinY = LLONG_MAX, bMaxY = LLONG_MIN;
        for (const auto& pt : boundary.points) {
            bMinX = qMin(bMinX, pt.x);
            bMaxX = qMax(bMaxX, pt.x);
            bMinY = qMin(bMinY, pt.y);
            bMaxY = qMax(bMaxY, pt.y);
        }
        
        // 边界框相交测试
        if (bMaxX < rectMinX || bMinX > rectMaxX ||
            bMaxY < rectMinY || bMinY > rectMaxY) {
            continue;
        }
        
        SelectedObject obj;
        obj.type = SelectedObject::Boundary;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = boundary.layer;
        result.objects.insert(obj);
    }
    
    // 2. 检查 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        const auto& path = structure->paths[i];
        if (path.points.size() < 2) continue;
        
        // 计算边界框
        qint64 pMinX = LLONG_MAX, pMaxX = LLONG_MIN;
        qint64 pMinY = LLONG_MAX, pMaxY = LLONG_MIN;
        for (const auto& pt : path.points) {
            pMinX = qMin(pMinX, pt.x);
            pMaxX = qMax(pMaxX, pt.x);
            pMinY = qMin(pMinY, pt.y);
            pMaxY = qMax(pMaxY, pt.y);
        }
        
        if (pMaxX < rectMinX || pMinX > rectMaxX ||
            pMaxY < rectMinY || pMinY > rectMaxY) {
            continue;
        }
        
        SelectedObject obj;
        obj.type = SelectedObject::Path;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = path.layer;
        result.objects.insert(obj);
    }
    
    // 3. 检查 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        const auto& text = structure->texts[i];
        qint64 tx = text.position.x;
        qint64 ty = text.position.y;
        
        if (tx >= rectMinX && tx <= rectMaxX && ty >= rectMinY && ty <= rectMaxY) {
            SelectedObject obj;
            obj.type = SelectedObject::Text;
            obj.index = i;
            obj.structureName = m_currentStructure;
            obj.layer = text.layer;
            result.objects.insert(obj);
        }
    }
    
    // 更新当前选择
    if (!addToSelection) {
        m_currentSelection = result.objects;
    } else {
        m_currentSelection.unite(result.objects);
    }
    
    if (!result.isEmpty()) {
        emit selectionChanged(m_currentSelection);
    }
    
    return result;
}

void SelectionEngine::clearSelection()
{
    if (!m_currentSelection.isEmpty()) {
        m_currentSelection.clear();
        emit selectionChanged(m_currentSelection);
    }
}

QSet<SelectedObject> SelectionEngine::filterByLayer(int layer) const
{
    QSet<SelectedObject> result;
    for (const auto& obj : m_currentSelection) {
        if (obj.layer == layer) {
            result.insert(obj);
        }
    }
    return result;
}

QSet<SelectedObject> SelectionEngine::filterByType(SelectedObject::Type type) const
{
    QSet<SelectedObject> result;
    for (const auto& obj : m_currentSelection) {
        if (obj.type == type) {
            result.insert(obj);
        }
    }
    return result;
}

SelectionResult SelectionEngine::selectAll()
{
    SelectionResult result;
    
    if (!m_data) return result;
    
    auto structure = m_data->getStructure(m_currentStructure);
    if (!structure) return result;
    
    // 选择所有 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Boundary;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->boundaries[i].layer;
        result.objects.insert(obj);
    }
    
    // 选择所有 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Path;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->paths[i].layer;
        result.objects.insert(obj);
    }
    
    // 选择所有 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Text;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->texts[i].layer;
        result.objects.insert(obj);
    }
    
    m_currentSelection = result.objects;
    emit selectionChanged(m_currentSelection);
    
    return result;
}

SelectionResult SelectionEngine::invertSelection()
{
    SelectionResult result;
    
    if (!m_data) return result;
    
    auto structure = m_data->getStructure(m_currentStructure);
    if (!structure) return result;
    
    // 反选所有 Boundary
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Boundary;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->boundaries[i].layer;
        
        if (!m_currentSelection.contains(obj)) {
            result.objects.insert(obj);
        }
    }
    
    // 反选所有 Path
    for (int i = 0; i < structure->paths.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Path;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->paths[i].layer;
        
        if (!m_currentSelection.contains(obj)) {
            result.objects.insert(obj);
        }
    }
    
    // 反选所有 Text
    for (int i = 0; i < structure->texts.size(); ++i) {
        SelectedObject obj;
        obj.type = SelectedObject::Text;
        obj.index = i;
        obj.structureName = m_currentStructure;
        obj.layer = structure->texts[i].layer;
        
        if (!m_currentSelection.contains(obj)) {
            result.objects.insert(obj);
        }
    }
    
    m_currentSelection = result.objects;
    emit selectionChanged(m_currentSelection);
    
    return result;
}

bool SelectionEngine::pointInPolygon(const QPointF& point, const QVector<DbPoint>& polygon) const
{
    // 射线法判断点是否在多边形内
    int n = polygon.size();
    bool inside = false;
    
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = polygon[i].x;
        double yi = polygon[i].y;
        double xj = polygon[j].x;
        double yj = polygon[j].y;
        
        if (((yi > point.y()) != (yj > point.y())) &&
            (point.x() < (xj - xi) * (point.y() - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    
    return inside;
}

bool SelectionEngine::pointNearPath(const QPointF& point, const GDSPath& path, double tolerance) const
{
    // 检查点是否在路径附近
    for (int i = 0; i < path.points.size() - 1; ++i) {
        QPointF p1(path.points[i].x / 1000.0, path.points[i].y / 1000.0);
        QPointF p2(path.points[i+1].x / 1000.0, path.points[i+1].y / 1000.0);
        
        // 计算点到线段的距离
        double dx = p2.x() - p1.x();
        double dy = p2.y() - p1.y();
        double lengthSq = dx * dx + dy * dy;
        
        double dist;
        if (lengthSq < 1e-12) {
            // 线段退化为点
            dist = QLineF(point, p1).length();
        } else {
            // 计算投影参数
            double t = ((point.x() - p1.x()) * dx + (point.y() - p1.y()) * dy) / lengthSq;
            t = qBound(0.0, t, 1.0);
            
            // 计算最近点
            double nearestX = p1.x() + t * dx;
            double nearestY = p1.y() + t * dy;
            
            dist = std::sqrt((point.x() - nearestX) * (point.x() - nearestX) + 
                            (point.y() - nearestY) * (point.y() - nearestY));
        }
        
        if (dist < tolerance) {
            return true;
        }
    }
    return false;
}

bool SelectionEngine::rectIntersectsPolygon(const QRectF& rect, const QVector<DbPoint>& polygon) const
{
    // 简化：只检查边界框相交
    if (polygon.isEmpty()) return false;
    
    qint64 minX = LLONG_MAX, maxX = LLONG_MIN;
    qint64 minY = LLONG_MAX, maxY = LLONG_MIN;
    
    for (const auto& pt : polygon) {
        minX = qMin(minX, pt.x);
        maxX = qMax(maxX, pt.x);
        minY = qMin(minY, pt.y);
        maxY = qMax(maxY, pt.y);
    }
    
    return !(maxX < rect.left() * 1000 || minX > rect.right() * 1000 ||
             maxY < rect.top() * 1000 || minY > rect.bottom() * 1000);
}

QRectF SelectionEngine::getBoundaryBounds(const GDSBoundary& boundary) const
{
    if (boundary.points.isEmpty()) return QRectF();
    
    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    
    for (const auto& pt : boundary.points) {
        double x = pt.x / 1000.0;
        double y = pt.y / 1000.0;
        minX = qMin(minX, x);
        maxX = qMax(maxX, x);
        minY = qMin(minY, y);
        maxY = qMax(maxY, y);
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QRectF SelectionEngine::getPathBounds(const GDSPath& path) const
{
    if (path.points.isEmpty()) return QRectF();
    
    double halfWidth = path.width / 2000.0;  // nm → μm
    
    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    
    for (const auto& pt : path.points) {
        double x = pt.x / 1000.0;
        double y = pt.y / 1000.0;
        minX = qMin(minX, x - halfWidth);
        maxX = qMax(maxX, x + halfWidth);
        minY = qMin(minY, y - halfWidth);
        maxY = qMax(maxY, y + halfWidth);
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

} // namespace QLayoutEDA