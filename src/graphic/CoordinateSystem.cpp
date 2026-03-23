/**
 * @file CoordinateSystem.cpp
 * @brief Coordinate system implementation
 */

#include "CoordinateSystem.h"
#include <QtMath>
#include <QDebug>

namespace QLayoutEDA {

CoordinateSystem::CoordinateSystem() = default;
CoordinateSystem::~CoordinateSystem() = default;

void CoordinateSystem::setDatabaseUnit(double unit) 
{ 
    m_dbUnit = unit; 
}

double CoordinateSystem::getDatabaseUnit() const 
{ 
    return m_dbUnit; 
}

void CoordinateSystem::setTransform(const ViewTransform& t) 
{ 
    m_transform = t; 
}

ViewTransform CoordinateSystem::getTransform() const 
{ 
    return m_transform; 
}

QPointF CoordinateSystem::dbToScreen(Coord x, Coord y) const
{
    // Database coords (integer nm) -> Screen coords (pixels)
    // Note: Y axis is inverted (screen Y increases downward)
    double dbX = static_cast<double>(x) * m_dbUnit;
    double dbY = static_cast<double>(y) * m_dbUnit;
    
    return QPointF(dbX * m_transform.scale + m_transform.offsetX,
                   -dbY * m_transform.scale + m_transform.offsetY);
}

DbPoint CoordinateSystem::screenToDb(const QPointF& screenPos) const
{
    // Screen coords -> Database coords
    double dbX = (screenPos.x() - m_transform.offsetX) / m_transform.scale;
    double dbY = (m_transform.offsetY - screenPos.y()) / m_transform.scale;
    
    // Convert from meters to database units
    return DbPoint(static_cast<Coord>(dbX / m_dbUnit),
                   static_cast<Coord>(dbY / m_dbUnit));
}

void CoordinateSystem::setScale(double scale) 
{ 
    // Clamp scale to valid range
    m_transform.scale = qBound(Constants::kMinScale, scale, Constants::kMaxScale);
}

double CoordinateSystem::getScale() const 
{ 
    return m_transform.scale; 
}

void CoordinateSystem::setOffset(double x, double y) 
{ 
    m_transform.offsetX = x; 
    m_transform.offsetY = y; 
}

void CoordinateSystem::fitToRect(const DbRect& rect, double margin)
{
    if (!rect.isValid()) {
        qWarning() << "Cannot fit to invalid rect";
        return;
    }
    
    // Calculate required scale to fit the rect
    double width = static_cast<double>(rect.width()) * m_dbUnit;
    double height = static_cast<double>(rect.height()) * m_dbUnit;
    
    // Assume viewport size (will be overridden by actual viewport)
    double viewportWidth = 800.0;
    double viewportHeight = 600.0;
    
    double scaleX = viewportWidth / (width * margin);
    double scaleY = viewportHeight / (height * margin);
    
    setScale(qMin(scaleX, scaleY));
    
    // Center the rect in viewport
    double centerX = static_cast<double>(rect.left + rect.right) / 2.0 * m_dbUnit;
    double centerY = static_cast<double>(rect.top + rect.bottom) / 2.0 * m_dbUnit;
    
    setOffset(viewportWidth / 2.0 - centerX * m_transform.scale,
              viewportHeight / 2.0 + centerY * m_transform.scale);
    
    qDebug() << "Fit to rect: scale=" << m_transform.scale 
             << "offset=" << m_transform.offsetX << "," << m_transform.offsetY;
}

} // namespace QLayoutEDA