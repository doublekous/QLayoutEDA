/**
 * @file CoordinateSystem.h
 * @brief 坐标系统
 */

#pragma once

#include "interfaces/ICoordinateSystem.h"

namespace QLayoutEDA {

class CoordinateSystem : public ICoordinateSystem {
public:
    explicit CoordinateSystem();
    ~CoordinateSystem() override;
    
    void setDatabaseUnit(double unit) override;
    double getDatabaseUnit() const override;
    void setTransform(const ViewTransform& transform) override;
    ViewTransform getTransform() const override;
    QPointF dbToScreen(Coord x, Coord y) const override;
    DbPoint screenToDb(const QPointF& screenPos) const override;
    void setScale(double scale) override;
    double getScale() const override;
    void setOffset(double x, double y) override;
    void fitToRect(const DbRect& rect, double margin) override;

private:
    double m_dbUnit = 1e-9;
    ViewTransform m_transform;
};

} // namespace QLayoutEDA