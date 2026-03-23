/**
 * @file ICoordinateSystem.h
 * @brief 坐标系统接口
 */

#pragma once

#include "core/Types.h"
#include <QPointF>
#include <QObject>

namespace QLayoutEDA {

/**
 * @brief 坐标系统接口
 * 
 * 负责数据库坐标与屏幕坐标之间的转换
 */
class ICoordinateSystem {
public:
    virtual ~ICoordinateSystem() = default;
    
    /**
     * @brief 设置数据库单位（米）
     */
    virtual void setDatabaseUnit(double unit) = 0;
    
    /**
     * @brief 获取数据库单位
     */
    virtual double getDatabaseUnit() const = 0;
    
    /**
     * @brief 设置视图变换
     */
    virtual void setTransform(const ViewTransform& transform) = 0;
    
    /**
     * @brief 获取视图变换
     */
    virtual ViewTransform getTransform() const = 0;
    
    /**
     * @brief 数据库坐标 -> 屏幕坐标
     */
    virtual QPointF dbToScreen(Coord x, Coord y) const = 0;
    
    /**
     * @brief 屏幕坐标 -> 数据库坐标
     */
    virtual DbPoint screenToDb(const QPointF& screenPos) const = 0;
    
    /**
     * @brief 设置缩放
     */
    virtual void setScale(double scale) = 0;
    
    /**
     * @brief 获取缩放
     */
    virtual double getScale() const = 0;
    
    /**
     * @brief 设置偏移
     */
    virtual void setOffset(double x, double y) = 0;
    
    /**
     * @brief 适配视图
     */
    virtual void fitToRect(const DbRect& rect, double margin = 1.1) = 0;
};

} // namespace QLayoutEDA

Q_DECLARE_INTERFACE(QLayoutEDA::ICoordinateSystem, "com.QLayoutEDA.ICoordinateSystem/1.0")