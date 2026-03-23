/**
 * @file GeometryAlgorithms.cpp
 * @brief 几何算法工具类实现
 */

#include "GeometryAlgorithms.h"
#include <cmath>
#include <algorithm>

namespace QLayoutEDA {

//=============================================================================
// Douglas-Peucker 算法
//=============================================================================

void GeometryAlgorithms::simplifyDP(const QVector<QPointF>& points, int first, int last,
                                    QVector<bool>& flags, double epsilon)
{
    if (last <= first + 1) return;
    
    // 找到距离最远的点
    double maxDist = 0;
    int maxIndex = first;
    
    QPointF segStart = points[first];
    QPointF segEnd = points[last];
    
    for (int i = first + 1; i < last; ++i) {
        double dist = pointToSegmentDistance(points[i], segStart, segEnd);
        if (dist > maxDist) {
            maxDist = dist;
            maxIndex = i;
        }
    }
    
    // 如果最大距离大于阈值，递归简化
    if (maxDist > epsilon) {
        flags[maxIndex] = true;
        simplifyDP(points, first, maxIndex, flags, epsilon);
        simplifyDP(points, maxIndex, last, flags, epsilon);
    }
}

QVector<QPointF> GeometryAlgorithms::simplifyDouglasPeuckerF(const QVector<QPointF>& points, double epsilon)
{
    if (points.size() < 3) return points;
    if (epsilon <= 0) return points;
    
    QVector<bool> flags(points.size(), false);
    flags[0] = true;
    flags[points.size() - 1] = true;
    
    simplifyDP(points, 0, points.size() - 1, flags, epsilon);
    
    QVector<QPointF> result;
    for (int i = 0; i < points.size(); ++i) {
        if (flags[i]) {
            result.append(points[i]);
        }
    }
    
    return result;
}

QVector<DbPoint> GeometryAlgorithms::simplifyDouglasPeucker(const QVector<DbPoint>& points, double epsilon)
{
    if (points.size() < 3) return points;
    if (epsilon <= 0) return points;
    
    // 转换为 QPointF
    QVector<QPointF> floatPoints;
    floatPoints.reserve(points.size());
    for (const auto& pt : points) {
        floatPoints.append(QPointF(static_cast<double>(pt.x), static_cast<double>(pt.y)));
    }
    
    // 简化
    QVector<QPointF> simplified = simplifyDouglasPeuckerF(floatPoints, epsilon);
    
    // 转换回 DbPoint
    QVector<DbPoint> result;
    result.reserve(simplified.size());
    for (const auto& pt : simplified) {
        result.append(DbPoint(static_cast<Coord>(std::round(pt.x())), 
                              static_cast<Coord>(std::round(pt.y()))));
    }
    
    return result;
}

//=============================================================================
// 边界框计算
//=============================================================================

DbRect GeometryAlgorithms::computeBoundingBox(const QVector<DbPoint>& points)
{
    if (points.isEmpty()) return DbRect();
    
    Coord minX = LLONG_MAX, maxX = LLONG_MIN;
    Coord minY = LLONG_MAX, maxY = LLONG_MIN;
    
    for (const auto& pt : points) {
        minX = qMin(minX, pt.x);
        maxX = qMax(maxX, pt.x);
        minY = qMin(minY, pt.y);
        maxY = qMax(maxY, pt.y);
    }
    
    return DbRect(minX, minY, maxX, maxY);
}

QRectF GeometryAlgorithms::computeBoundingBoxF(const QVector<QPointF>& points)
{
    if (points.isEmpty()) return QRectF();
    
    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    
    for (const auto& pt : points) {
        minX = qMin(minX, pt.x());
        maxX = qMax(maxX, pt.x());
        minY = qMin(minY, pt.y());
        maxY = qMax(maxY, pt.y());
    }
    
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

//=============================================================================
// 相交测试
//=============================================================================

bool GeometryAlgorithms::boundsIntersect(const DbRect& a, const DbRect& b)
{
    return a.left <= b.right && a.right >= b.left &&
           a.top <= b.bottom && a.bottom >= b.top;
}

bool GeometryAlgorithms::pointInBounds(const DbPoint& point, const DbRect& bounds)
{
    return point.x >= bounds.left && point.x <= bounds.right &&
           point.y >= bounds.top && point.y <= bounds.bottom;
}

//=============================================================================
// 简化参数计算
//=============================================================================

double GeometryAlgorithms::computeSimplifyEpsilon(double scale, int originalVertexCount)
{
    // 根据缩放和顶点数动态调整容差
    // 缩放越小（缩小视图），容差越大（更多简化）
    // 顶点数越多，容差越大
    
    double baseEpsilon = 10.0; // 基础容差（数据库单位，纳米）
    
    // 缩放因子：缩小视图时增大容差
    double scaleF = qMax(1.0, 1.0 / (scale * 1e9)); // 假设 scale 单位是米/像素
    
    // 顶点数因子：顶点越多越激进地简化
    double vertexF = 1.0;
    if (originalVertexCount > 100) {
        vertexF = std::sqrt(static_cast<double>(originalVertexCount) / 100.0);
    }
    
    return baseEpsilon * scaleF * vertexF;
}

bool GeometryAlgorithms::shouldSimplify(double scale, int vertexCount)
{
    // 顶点数少于阈值不需要简化
    if (vertexCount < 50) return false;
    
    // 根据缩放级别判断
    // scale 小于某个阈值时需要简化
    return scale < 1e-6 || vertexCount > 100;
}

//=============================================================================
// 多边形面积
//=============================================================================

double GeometryAlgorithms::computePolygonArea(const QVector<DbPoint>& points)
{
    if (points.size() < 3) return 0.0;
    
    double area = 0.0;
    int n = points.size();
    
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += static_cast<double>(points[i].x) * points[j].y;
        area -= static_cast<double>(points[j].x) * points[i].y;
    }
    
    return std::abs(area) / 2.0;
}

//=============================================================================
// 点到线段距离
//=============================================================================

double GeometryAlgorithms::pointToSegmentDistance(const QPointF& point,
                                                   const QPointF& segStart,
                                                   const QPointF& segEnd)
{
    double dx = segEnd.x() - segStart.x();
    double dy = segEnd.y() - segStart.y();
    
    double lengthSq = dx * dx + dy * dy;
    
    if (lengthSq < 1e-12) {
        // 线段退化为点
        double px = point.x() - segStart.x();
        double py = point.y() - segStart.y();
        return std::sqrt(px * px + py * py);
    }
    
    // 计算投影参数 t
    double t = ((point.x() - segStart.x()) * dx + (point.y() - segStart.y()) * dy) / lengthSq;
    t = qBound(0.0, t, 1.0);
    
    // 计算最近点
    double nearestX = segStart.x() + t * dx;
    double nearestY = segStart.y() + t * dy;
    
    double distX = point.x() - nearestX;
    double distY = point.y() - nearestY;
    
    return std::sqrt(distX * distX + distY * distY);
}

} // namespace QLayoutEDA