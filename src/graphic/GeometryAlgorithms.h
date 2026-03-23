/**
 * @file GeometryAlgorithms.h
 * @brief 几何算法工具类
 */

#pragma once

#include "core/Types.h"
#include <QVector>
#include <QPointF>
#include <QRectF>

namespace QLayoutEDA {

/**
 * @brief 几何算法工具类
 */
class GeometryAlgorithms {
public:
    /**
     * @brief Douglas-Peucker 顶点简化算法
     * @param points 原始顶点
     * @param epsilon 容差阈值
     * @return 简化后的顶点
     */
    static QVector<DbPoint> simplifyDouglasPeucker(const QVector<DbPoint>& points, double epsilon);
    
    /**
     * @brief 简化 QPointF 序列
     */
    static QVector<QPointF> simplifyDouglasPeuckerF(const QVector<QPointF>& points, double epsilon);
    
    /**
     * @brief 计算边界框
     */
    static DbRect computeBoundingBox(const QVector<DbPoint>& points);
    
    /**
     * @brief 计算边界框（QPointF）
     */
    static QRectF computeBoundingBoxF(const QVector<QPointF>& points);
    
    /**
     * @brief 边界框相交测试
     */
    static bool boundsIntersect(const DbRect& a, const DbRect& b);
    
    /**
     * @brief 点在边界框内测试
     */
    static bool pointInBounds(const DbPoint& point, const DbRect& bounds);
    
    /**
     * @brief 根据缩放级别计算合适的简化容差
     * @param scale 当前缩放比例
     * @param originalVertexCount 原始顶点数
     * @return 简化容差（数据库单位）
     */
    static double computeSimplifyEpsilon(double scale, int originalVertexCount);
    
    /**
     * @brief 判断是否需要简化
     */
    static bool shouldSimplify(double scale, int vertexCount);
    
    /**
     * @brief 计算多边形面积
     */
    static double computePolygonArea(const QVector<DbPoint>& points);
    
    /**
     * @brief 计算点到线段的距离
     */
    static double pointToSegmentDistance(const QPointF& point, 
                                         const QPointF& segStart, 
                                         const QPointF& segEnd);

private:
    static void simplifyDP(const QVector<QPointF>& points, int first, int last, 
                          QVector<bool>& flags, double epsilon);
};

} // namespace QLayoutEDA