/**
 * @file RenderEngine.cpp
 * @brief Render engine with hierarchical rendering implementation
 */

#include "RenderEngine.h"
#include "SpatialIndex.h"
#include <QImage>
#include <QPainterPath>
#include <QElapsedTimer>
#include <QDebug>
#include <QtMath>

namespace QLayoutEDA {

// LOD thresholds
static constexpr double LOD_THRESHOLD_MEDIUM = 1e-9;
static constexpr double LOD_THRESHOLD_LOW = 1e-12;
static constexpr double LOD_THRESHOLD_BOX = 1e-15;

struct RenderEngine::Impl {
    RenderConfig config;
    GDSDataPtr data;
    QString currentStructure;
    QVector<LayerStyle> layerStyles;
    QHash<quint64, QColor> highlights;
    
    SpatialIndex* spatialIndex = nullptr;
    
    int renderedObjects = 0;
    int culledObjects = 0;
    int renderedInstances = 0;
    double lastRenderTime = 0;
    
    LayerStyle getOrCreateStyle(int layer) {
        for (const auto& style : layerStyles) {
            if (style.layer == layer) return style;
        }
        
        LayerStyle style;
        style.layer = layer;
        int hue = (layer * 137) % 360;
        style.fillColor = QColor::fromHsl(hue, 180, 160, 180);
        style.borderColor = QColor::fromHsl(hue, 200, 100);
        style.borderWidth = 1;
        style.visible = true;
        style.selectable = true;
        return style;
    }
    
    bool isLayerVisible(int layer) const {
        for (const auto& style : layerStyles) {
            if (style.layer == layer) return style.visible;
        }
        return true;
    }
};

RenderEngine::RenderEngine() : d(new Impl) {}
RenderEngine::~RenderEngine() = default;

bool RenderEngine::initialize(const RenderConfig& config)
{
    d->config = config;
    d->renderedObjects = 0;
    d->culledObjects = 0;
    d->renderedInstances = 0;
    return true;
}

void RenderEngine::setData(GDSDataPtr data) { d->data = data; }
void RenderEngine::setCurrentStructure(const QString& name) { d->currentStructure = name; }
void RenderEngine::invalidateCache() {}
void RenderEngine::setSpatialIndex(SpatialIndex* index) { d->spatialIndex = index; }

LODLevel RenderEngine::calculateLOD(double scale) const
{
    if (scale >= LOD_THRESHOLD_MEDIUM) return LODLevel::Full;
    if (scale >= LOD_THRESHOLD_LOW) return LODLevel::Medium;
    if (scale >= LOD_THRESHOLD_BOX) return LODLevel::Low;
    return LODLevel::BoundingBox;
}

void RenderEngine::render(QPainter* painter, const QRectF& viewport, 
                          const ViewTransform& transform)
{
    QElapsedTimer timer;
    timer.start();
    
    if (!painter) return;
    
    LODLevel lod = calculateLOD(transform.scale);
    
    if (d->config.antialiasing) {
        painter->setRenderHint(QPainter::Antialiasing, true);
    }
    
    painter->fillRect(viewport, d->config.backgroundColor);
    
    d->renderedObjects = 0;
    d->culledObjects = 0;
    d->renderedInstances = 0;
    m_renderingStack.clear();
    
    if (d->data && !d->currentStructure.isEmpty()) {
        auto structure = d->data->getStructure(d->currentStructure);
        if (structure) {
            InstanceTransform identityTransform;
            renderStructureRecursive(painter, structure, transform, identityTransform, lod, 0);
        }
    }
    
    if (d->config.showGrid) {
        drawGrid(painter, viewport, transform);
    }
    
    d->lastRenderTime = timer.elapsed();
    
    qDebug() << "Rendered:" << d->renderedObjects << "objects,"
             << d->renderedInstances << "instances in"
             << d->lastRenderTime << "ms";
}

void RenderEngine::renderStructureRecursive(QPainter* painter,
                                            const GDSStructurePtr& structure,
                                            const ViewTransform& viewTransform,
                                            const InstanceTransform& instanceTransform,
                                            LODLevel lod,
                                            int depth)
{
    if (!structure) return;
    
    if (depth > m_maxRenderDepth) {
        return;
    }
    
    if (m_renderingStack.contains(structure->name)) {
        return;
    }
    m_renderingStack.insert(structure->name);
    
    d->renderedInstances++;
    
    // Render boundaries
    for (const auto& boundary : structure->boundaries) {
        LayerStyle style = d->getOrCreateStyle(boundary.layer);
        if (!style.visible) continue;
        
        renderBoundary(painter, boundary, viewTransform, instanceTransform, lod);
        d->renderedObjects++;
    }
    
    // Render paths
    for (const auto& path : structure->paths) {
        LayerStyle style = d->getOrCreateStyle(path.layer);
        if (!style.visible) continue;
        
        if (path.points.size() >= 2) {
            renderPath(painter, path, viewTransform, instanceTransform, lod);
            d->renderedObjects++;
        }
    }
    
    // Texts only at full detail and low depth
    if (lod == LODLevel::Full && depth < 3) {
        for (const auto& text : structure->texts) {
            LayerStyle style = d->getOrCreateStyle(text.layer);
            if (!style.visible) continue;
            
            QPointF pos = instanceTransform.transformPoint(text.position.x, text.position.y, viewTransform);
            
            painter->setPen(style.borderColor);
            QFont font = painter->font();
            font.setPointSizeF(text.magnification * 10);
            painter->setFont(font);
            painter->drawText(pos, text.text);
            d->renderedObjects++;
        }
    }
    
    // Render references (SREF)
    for (const auto& ref : structure->references) {
        if (!d->data) continue;
        
        auto refStructure = d->data->getStructure(ref.structureName);
        if (!refStructure) continue;
        
        InstanceTransform refTransform;
        refTransform.offsetX = instanceTransform.offsetX + static_cast<double>(ref.position.x) * instanceTransform.magnification;
        refTransform.offsetY = instanceTransform.offsetY + static_cast<double>(ref.position.y) * instanceTransform.magnification;
        refTransform.angle = instanceTransform.angle + ref.angle;
        refTransform.magnification = instanceTransform.magnification * ref.magnification;
        refTransform.reflected = instanceTransform.reflected != ref.reflected;
        
        renderStructureRecursive(painter, refStructure, viewTransform, refTransform, lod, depth + 1);
    }
    
    // Render array references (AREF)
    for (const auto& aref : structure->arrays) {
        if (!d->data) continue;
        
        auto arefStructure = d->data->getStructure(aref.structureName);
        if (!arefStructure) continue;
        
        for (int col = 0; col < aref.columns; ++col) {
            for (int row = 0; row < aref.rows; ++row) {
                double elemX = static_cast<double>(aref.position.x) + 
                               col * static_cast<double>(aref.columnVector.x) +
                               row * static_cast<double>(aref.rowVector.x);
                double elemY = static_cast<double>(aref.position.y) + 
                               col * static_cast<double>(aref.columnVector.y) +
                               row * static_cast<double>(aref.rowVector.y);
                
                InstanceTransform arefTransform;
                arefTransform.offsetX = instanceTransform.offsetX + elemX * instanceTransform.magnification;
                arefTransform.offsetY = instanceTransform.offsetY + elemY * instanceTransform.magnification;
                arefTransform.magnification = instanceTransform.magnification;
                
                renderStructureRecursive(painter, arefStructure, viewTransform, arefTransform, lod, depth + 1);
            }
        }
    }
    
    m_renderingStack.remove(structure->name);
}

void RenderEngine::renderBoundary(QPainter* painter, const GDSBoundary& boundary,
                                  const ViewTransform& viewTransform,
                                  const InstanceTransform& instanceTransform,
                                  LODLevel lod)
{
    switch (lod) {
    case LODLevel::Full:
    case LODLevel::Medium:
        renderBoundaryFull(painter, boundary, viewTransform, instanceTransform);
        break;
    case LODLevel::Low:
        renderBoundaryOutline(painter, boundary, viewTransform, instanceTransform);
        break;
    case LODLevel::BoundingBox:
        renderBoundaryBox(painter, boundary, viewTransform, instanceTransform);
        break;
    }
}

void RenderEngine::renderPath(QPainter* painter, const GDSPath& path,
                              const ViewTransform& viewTransform,
                              const InstanceTransform& instanceTransform,
                              LODLevel lod)
{
    switch (lod) {
    case LODLevel::Full:
    case LODLevel::Medium:
        renderPathFull(painter, path, viewTransform, instanceTransform);
        break;
    case LODLevel::Low:
    case LODLevel::BoundingBox:
        renderPathOutline(painter, path, viewTransform, instanceTransform);
        break;
    }
}

void RenderEngine::renderBoundaryFull(QPainter* painter, const GDSBoundary& boundary,
                                      const ViewTransform& viewTransform,
                                      const InstanceTransform& instanceTransform)
{
    if (boundary.points.size() < 3) return;
    
    LayerStyle style = d->getOrCreateStyle(boundary.layer);
    
    QPolygonF polygon;
    polygon.reserve(boundary.points.size());
    
    for (const auto& pt : boundary.points) {
        QPointF screenPos = instanceTransform.transformPoint(pt.x, pt.y, viewTransform);
        polygon << screenPos;
    }
    
    painter->setPen(QPen(style.borderColor, style.borderWidth));
    painter->setBrush(QBrush(style.fillColor));
    painter->drawPolygon(polygon);
}

void RenderEngine::renderBoundaryOutline(QPainter* painter, const GDSBoundary& boundary,
                                         const ViewTransform& viewTransform,
                                         const InstanceTransform& instanceTransform)
{
    if (boundary.points.size() < 3) return;
    
    LayerStyle style = d->getOrCreateStyle(boundary.layer);
    
    QPolygonF polygon;
    polygon.reserve(boundary.points.size());
    
    for (const auto& pt : boundary.points) {
        QPointF screenPos = instanceTransform.transformPoint(pt.x, pt.y, viewTransform);
        polygon << screenPos;
    }
    
    painter->setPen(QPen(style.borderColor, style.borderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolygon(polygon);
}

void RenderEngine::renderBoundaryBox(QPainter* painter, const GDSBoundary& boundary,
                                     const ViewTransform& viewTransform,
                                     const InstanceTransform& instanceTransform)
{
    if (boundary.points.isEmpty()) return;
    
    LayerStyle style = d->getOrCreateStyle(boundary.layer);
    
    QPointF firstPt = instanceTransform.transformPoint(boundary.points[0].x, boundary.points[0].y, viewTransform);
    double minX = firstPt.x(), maxX = firstPt.x();
    double minY = firstPt.y(), maxY = firstPt.y();
    
    for (const auto& pt : boundary.points) {
        QPointF screenPos = instanceTransform.transformPoint(pt.x, pt.y, viewTransform);
        minX = qMin(minX, screenPos.x());
        maxX = qMax(maxX, screenPos.x());
        minY = qMin(minY, screenPos.y());
        maxY = qMax(maxY, screenPos.y());
    }
    
    painter->setPen(QPen(style.borderColor, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(QRectF(minX, minY, maxX - minX, maxY - minY));
}

void RenderEngine::renderPathFull(QPainter* painter, const GDSPath& path,
                                  const ViewTransform& viewTransform,
                                  const InstanceTransform& instanceTransform)
{
    if (path.points.size() < 2) return;
    
    LayerStyle style = d->getOrCreateStyle(path.layer);
    
    QPainterPath painterPath;
    
    QPointF firstPt = instanceTransform.transformPoint(path.points[0].x, path.points[0].y, viewTransform);
    painterPath.moveTo(firstPt);
    
    for (int i = 1; i < path.points.size(); ++i) {
        QPointF screenPos = instanceTransform.transformPoint(path.points[i].x, path.points[i].y, viewTransform);
        painterPath.lineTo(screenPos);
    }
    
    double widthPixels = qMax(1.0, static_cast<double>(path.width) * instanceTransform.magnification * viewTransform.scale);
    
    QPen pen(style.borderColor);
    pen.setWidthF(widthPixels);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(painterPath);
}

void RenderEngine::renderPathOutline(QPainter* painter, const GDSPath& path,
                                     const ViewTransform& viewTransform,
                                     const InstanceTransform& instanceTransform)
{
    if (path.points.size() < 2) return;
    
    LayerStyle style = d->getOrCreateStyle(path.layer);
    
    QPainterPath painterPath;
    
    QPointF firstPt = instanceTransform.transformPoint(path.points[0].x, path.points[0].y, viewTransform);
    painterPath.moveTo(firstPt);
    
    for (int i = 1; i < path.points.size(); ++i) {
        QPointF screenPos = instanceTransform.transformPoint(path.points[i].x, path.points[i].y, viewTransform);
        painterPath.lineTo(screenPos);
    }
    
    painter->setPen(QPen(style.borderColor, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(painterPath);
}

void RenderEngine::renderStructure(QPainter* painter, 
                                   const GDSStructurePtr& structure,
                                   const ViewTransform& transform,
                                   LODLevel lod)
{
    InstanceTransform identityTransform;
    m_renderingStack.clear();
    renderStructureRecursive(painter, structure, transform, identityTransform, lod, 0);
}

void RenderEngine::drawGrid(QPainter* painter, const QRectF& viewport,
                           const ViewTransform& transform)
{
    painter->save();
    painter->setPen(QPen(d->config.gridColor, 1));
    
    double spacing = d->config.gridSpacing;
    if (spacing < 5) spacing = 5;
    if (spacing > 200) spacing = 200;
    
    double startX = fmod(transform.offsetX, spacing);
    for (double x = startX; x < viewport.right(); x += spacing) {
        painter->drawLine(QLineF(x, viewport.top(), x, viewport.bottom()));
    }
    
    double startY = fmod(transform.offsetY, spacing);
    for (double y = startY; y < viewport.bottom(); y += spacing) {
        painter->drawLine(QLineF(viewport.left(), y, viewport.right(), y));
    }
    
    painter->restore();
}

void RenderEngine::setLayerStyles(const QVector<LayerStyle>& styles) { d->layerStyles = styles; }
LayerStyle RenderEngine::getLayerStyle(int layer) const { return d->getOrCreateStyle(layer); }

void RenderEngine::setLayerVisible(int layer, bool visible)
{
    for (auto& style : d->layerStyles) {
        if (style.layer == layer) {
            style.visible = visible;
            return;
        }
    }
    LayerStyle style = d->getOrCreateStyle(layer);
    style.visible = visible;
    d->layerStyles.append(style);
}

QVector<int> RenderEngine::getVisibleLayers() const
{
    QVector<int> layers;
    for (const auto& style : d->layerStyles) {
        if (style.visible) layers.append(style.layer);
    }
    return layers;
}

QVector<quint64> RenderEngine::pickObjects(const QPointF& screenPos) const
{
    Q_UNUSED(screenPos);
    return QVector<quint64>();
}

void RenderEngine::highlightObject(quint64 id, const QColor& color) { d->highlights[id] = color; }
void RenderEngine::clearHighlights() { d->highlights.clear(); }

bool RenderEngine::exportImage(const QString& filePath, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(d->config.backgroundColor);
    
    QPainter painter(&image);
    
    ViewTransform transform;
    transform.scale = 1.0;
    transform.offsetX = width / 2.0;
    transform.offsetY = height / 2.0;
    
    render(&painter, QRectF(0, 0, width, height), transform);
    
    return image.save(filePath);
}

QString RenderEngine::getStats() const
{
    return QString("Objects: %1, Instances: %2, Time: %3ms")
        .arg(d->renderedObjects)
        .arg(d->renderedInstances)
        .arg(d->lastRenderTime, 0, 'f', 1);
}

QRectF RenderEngine::screenToDbRect(const QRectF& screenRect, const ViewTransform& transform) const
{
    if (transform.scale <= 0) return QRectF();
    
    double left = (screenRect.left() - transform.offsetX) / transform.scale;
    double right = (screenRect.right() - transform.offsetX) / transform.scale;
    double top = -(screenRect.top() - transform.offsetY) / transform.scale;
    double bottom = -(screenRect.bottom() - transform.offsetY) / transform.scale;
    
    return QRectF(qMin(left, right), qMin(top, bottom), 
                  qAbs(right - left), qAbs(bottom - top));
}

bool RenderEngine::isObjectVisible(const DbRect& objBounds, const QRectF& viewRectDb) const
{
    return objBounds.left <= viewRectDb.right() &&
           objBounds.right >= viewRectDb.left() &&
           objBounds.top <= viewRectDb.bottom() &&
           objBounds.bottom >= viewRectDb.top();
}

} // namespace QLayoutEDA