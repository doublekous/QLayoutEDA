/**
 * @file RenderCoordinator.cpp
 */

#include "RenderCoordinator.h"
#include "QuadTree.h"
#include "../core/Types.h"
#include <QPainter>
#include <QElapsedTimer>
#include <QtConcurrent>

namespace QLayoutEDA {

RenderCoordinator::RenderCoordinator(QObject* parent)
    : QObject(parent)
    , m_pool(QThreadPool::globalInstance())
    , m_watcher(new QFutureWatcher<RenderResult>(this))
    , m_quadTree(nullptr)
    , m_cancelled(false)
    , m_taskId(0)
{
    m_pool->setMaxThreadCount(qMax(2, QThread::idealThreadCount()));
    connect(m_watcher, &QFutureWatcher<RenderResult>::finished, [this]() {
        if (!m_cancelled) emit renderComplete(m_watcher->result());
    });
}

void RenderCoordinator::setData(GDSDataPtr data) { m_data = data; }
void RenderCoordinator::setQuadTree(QuadTree* tree) { m_quadTree = tree; }

quint64 RenderCoordinator::requestRender(const RenderTask& task) {
    if (m_watcher->isRunning()) { m_cancelled = true; m_watcher->waitForFinished(); }
    m_cancelled = false;
    m_taskId++;
    QFuture<RenderResult> f = QtConcurrent::run(m_pool, [this, task]() { return doRender(task); });
    m_watcher->setFuture(f);
    return m_taskId;
}

void RenderCoordinator::cancelRender() { m_cancelled = true; if (m_watcher->isRunning()) m_watcher->waitForFinished(); }
bool RenderCoordinator::isRendering() const { return m_watcher->isRunning(); }

LODLevel RenderCoordinator::calculateLOD(double scale) {
    if (scale < 0.001) return LODLevel::BoundingBox;
    if (scale < 0.01) return LODLevel::Low;
    if (scale < 0.1) return LODLevel::Medium;
    if (scale < 0.5) return LODLevel::Simplified;
    return LODLevel::Full;
}

RenderResult RenderCoordinator::doRender(const RenderTask& task) {
    RenderResult r;
    r.cellName = task.cellName;
    r.success = false;
    r.shapeCount = 0;
    r.objectCount = 0;
    
    if (!m_data || m_cancelled) return r;
    
    QElapsedTimer t; t.start();
    
    r.image = QImage(task.imageSize, QImage::Format_ARGB32_Premultiplied);
    r.image.fill(QColor(30, 30, 30));
    
    QString cellName = task.cellName.isEmpty() ? m_data->topStructure : task.cellName;
    auto structure = m_data->getStructure(cellName);
    if (!structure) return r;
    
    QPainter p(&r.image);
    
    qint64 minX=LLONG_MAX, maxX=LLONG_MIN, minY=LLONG_MAX, maxY=LLONG_MIN;
    for (auto& b : structure->boundaries) {
        for (auto& pt : b.points) {
            minX = qMin(minX, pt.x); maxX = qMax(maxX, pt.x);
            minY = qMin(minY, pt.y); maxY = qMax(maxY, pt.y);
        }
    }
    
    double baseScale = qMin((task.imageSize.width()-100.0)/(maxX-minX), (task.imageSize.height()-100.0)/(maxY-minY));
    double scale = baseScale * task.scale;
    double ox = (task.imageSize.width() - (maxX-minX)*scale)/2.0 - minX*scale;
    double oy = (task.imageSize.height() + (maxY-minY)*scale)/2.0 + minY*scale;
    
    LODLevel lod = calculateLOD(task.scale);
    
    auto getColor = [](int layer) { return QColor::fromHsl((layer*137)%360, 180, 160); };
    
    for (auto& b : structure->boundaries) {
        if (m_cancelled) break;
        if (b.points.size() < 3) continue;
        
        qint64 bMinX=LLONG_MAX, bMaxX=LLONG_MIN, bMinY=LLONG_MAX, bMaxY=LLONG_MIN;
        for (auto& pt : b.points) {
            bMinX = qMin(bMinX, pt.x); bMaxX = qMax(bMaxX, pt.x);
            bMinY = qMin(bMinY, pt.y); bMaxY = qMax(bMaxY, pt.y);
        }
        
        if (lod == LODLevel::BoundingBox) {
            double x1 = bMinX*scale + ox, y1 = -bMaxY*scale + oy;
            double x2 = bMaxX*scale + ox, y2 = -bMinY*scale + oy;
            QColor c = getColor(b.layer); c.setAlpha(128);
            p.fillRect(QRectF(x1, y1, x2-x1, y2-y1), c);
            r.shapeCount++;
            continue;
        }
        
        QPolygonF poly;
        for (auto& pt : b.points) poly << QPointF(pt.x*scale + ox, -pt.y*scale + oy);
        
        QColor c = getColor(b.layer); c.setAlpha(180);
        p.setPen(QPen(c.darker(150), 1));
        p.setBrush(QBrush(c));
        p.drawPolygon(poly);
        r.shapeCount++;
    }
    
    r.objectCount = r.shapeCount;
    r.renderTime = t.elapsed();
    r.success = !m_cancelled;
    return r;
}

} // namespace QLayoutEDA