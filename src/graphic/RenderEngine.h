/**
 * @file RenderEngine.h
 * @brief Render engine with hierarchical rendering support
 */

#pragma once

#include "interfaces/IRenderEngine.h"
#include "core/Types.h"  // 包含 LODLevel
#include <QScopedPointer>
#include <QTransform>
#include <QSet>
#include <QtMath>

namespace QLayoutEDA {

/**
 * @brief Instance transform for hierarchical rendering
 */
struct InstanceTransform {
    double offsetX = 0;
    double offsetY = 0;
    double scale = 1.0;
    double angle = 0;       // rotation in degrees
    double magnification = 1.0;
    bool reflected = false;
    
    InstanceTransform() = default;
    
    // Transform a point from database to screen
    QPointF transformPoint(Coord dbX, Coord dbY, const ViewTransform& viewTransform) const {
        double x = static_cast<double>(dbX) * magnification;
        double y = static_cast<double>(dbY) * magnification;
        
        if (reflected) x = -x;
        
        // Apply rotation
        double rad = qDegreesToRadians(angle);
        double cosA = qCos(rad);
        double sinA = qSin(rad);
        
        double rx = x * cosA - y * sinA;
        double ry = x * sinA + y * cosA;
        
        // Apply instance offset
        rx += offsetX;
        ry += offsetY;
        
        // Apply view transform
        double screenX = rx * viewTransform.scale + viewTransform.offsetX;
        double screenY = -ry * viewTransform.scale + viewTransform.offsetY;
        
        return QPointF(screenX, screenY);
    }
};

class RenderEngine : public IRenderEngine {
public:
    explicit RenderEngine();
    ~RenderEngine() override;
    
    bool initialize(const RenderConfig& config) override;
    void setData(GDSDataPtr data) override;
    void setCurrentStructure(const QString& name) override;
    void render(QPainter* painter, const QRectF& viewport, 
                const ViewTransform& transform) override;
    void setLayerStyles(const QVector<LayerStyle>& styles) override;
    LayerStyle getLayerStyle(int layer) const override;
    void setLayerVisible(int layer, bool visible) override;
    QVector<int> getVisibleLayers() const override;
    QVector<quint64> pickObjects(const QPointF& screenPos) const override;
    void highlightObject(quint64 id, const QColor& color) override;
    void clearHighlights() override;
    bool exportImage(const QString& filePath, int width, int height) override;
    QString getStats() const override;
    
    void invalidateCache();
    void setSpatialIndex(class SpatialIndex* index);
    LODLevel calculateLOD(double scale) const;
    
    void setMaxRenderDepth(int depth) { m_maxRenderDepth = depth; }
    int getMaxRenderDepth() const { return m_maxRenderDepth; }
    
private:
    void renderStructure(QPainter* painter, const GDSStructurePtr& structure,
                         const ViewTransform& transform, LODLevel lod);
    
    void renderStructureRecursive(QPainter* painter, 
                                  const GDSStructurePtr& structure,
                                  const ViewTransform& viewTransform,
                                  const InstanceTransform& instanceTransform,
                                  LODLevel lod,
                                  int depth);
    
    void renderBoundary(QPainter* painter, const GDSBoundary& boundary,
                        const ViewTransform& viewTransform,
                        const InstanceTransform& instanceTransform,
                        LODLevel lod);
    void renderPath(QPainter* painter, const GDSPath& path,
                    const ViewTransform& viewTransform,
                    const InstanceTransform& instanceTransform,
                    LODLevel lod);
    
    void renderBoundaryFull(QPainter* painter, const GDSBoundary& boundary,
                            const ViewTransform& viewTransform,
                            const InstanceTransform& instanceTransform);
    void renderBoundaryOutline(QPainter* painter, const GDSBoundary& boundary,
                               const ViewTransform& viewTransform,
                               const InstanceTransform& instanceTransform);
    void renderBoundaryBox(QPainter* painter, const GDSBoundary& boundary,
                           const ViewTransform& viewTransform,
                           const InstanceTransform& instanceTransform);
    void renderPathFull(QPainter* painter, const GDSPath& path,
                        const ViewTransform& viewTransform,
                        const InstanceTransform& instanceTransform);
    void renderPathOutline(QPainter* painter, const GDSPath& path,
                           const ViewTransform& viewTransform,
                           const InstanceTransform& instanceTransform);
    
    void drawGrid(QPainter* painter, const QRectF& viewport,
                  const ViewTransform& transform);
    
    QRectF screenToDbRect(const QRectF& screenRect, const ViewTransform& transform) const;
    bool isObjectVisible(const DbRect& objBounds, const QRectF& viewRectDb) const;
    
    struct Impl;
    QScopedPointer<Impl> d;
    
    int m_maxRenderDepth = 32;
    QSet<QString> m_renderingStack;  // For cycle detection
};

} // namespace QLayoutEDA