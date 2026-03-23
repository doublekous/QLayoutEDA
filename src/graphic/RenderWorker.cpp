/**
 * @file RenderWorker.cpp
 * @brief 渲染工作线程实现
 */

#include "RenderWorker.h"
#include "GeometryAlgorithms.h"
#include <QPainter>
#include <QElapsedTimer>
#include <QDebug>

namespace QLayoutEDA {

RenderWorker::RenderWorker(int taskId, int tileIndex, const QRectF& viewport,
                           double scale, const QSize& imageSize, LODLevel lod,
                           GDSDataPtr data, QuadTree* spatialIndex)
    : m_taskId(taskId)
    , m_tileIndex(tileIndex)
    , m_viewport(viewport)
    , m_scale(scale)
    , m_imageSize(imageSize)
    , m_lod(lod)
    , m_data(data)
    , m_spatialIndex(spatialIndex)
{
    setAutoDelete(true);
}

void RenderWorker::run()
{
    QElapsedTimer timer;
    timer.start();
    
    // 创建图像
    m_result = QImage(m_imageSize, QImage::Format_ARGB32_Premultiplied);
    m_result.fill(QColor(30, 30, 30));
    
    if (!m_data) {
        emit finished();
        return;
    }
    
    // 获取当前结构
    QString structName = m_data->topStructure;
    if (structName.isEmpty() && !m_data->structures.isEmpty()) {
        structName = m_data->structures.keys().first();
    }
    
    auto structure = m_data->getStructure(structName);
    if (!structure) {
        emit finished();
        return;
    }
    
    // 计算坐标转换参数
    // viewport 是微米单位，需要转换到屏幕坐标
    m_renderScale = qMin(m_imageSize.width() / m_viewport.width(),
                         m_imageSize.height() / m_viewport.height());
    m_renderScale *= m_scale;
    
    m_offsetX = -m_viewport.left() * m_renderScale;
    m_offsetY = m_viewport.bottom() * m_renderScale;  // Y 轴翻转
    
    // 渲染
    QPainter painter(&m_result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    renderShapes(painter);
    
    painter.end();
    
    qDebug() << "RenderWorker[" << m_tileIndex << "] 完成, 耗时:" << timer.elapsed() << "ms";
    
    emit finished();
}

void RenderWorker::renderShapes(QPainter& painter)
{
    QString structName = m_data->topStructure;
    if (structName.isEmpty()) structName = m_data->structures.keys().first();
    
    auto structure = m_data->getStructure(structName);
    if (!structure) return;
    
    // 使用四叉树查询可见图形
    QVector<ShapeProxy> visibleShapes;
    if (m_spatialIndex) {
        visibleShapes = m_spatialIndex->query(m_viewport);
    }
    
    int renderedCount = 0;
    
    if (!visibleShapes.isEmpty() && m_spatialIndex) {
        // 使用四叉树结果
        for (const auto& shape : visibleShapes) {
            switch (shape.type) {
            case ShapeProxy::Boundary:
                if (shape.index < static_cast<quint32>(structure->boundaries.size())) {
                    renderBoundary(painter, structure->boundaries[shape.index]);
                    renderedCount++;
                }
                break;
            case ShapeProxy::Path:
                if (shape.index < static_cast<quint32>(structure->paths.size())) {
                    renderPath(painter, structure->paths[shape.index]);
                    renderedCount++;
                }
                break;
            case ShapeProxy::Text:
                if (shape.index < static_cast<quint32>(structure->texts.size())) {
                    renderText(painter, structure->texts[shape.index]);
                    renderedCount++;
                }
                break;
            }
        }
    } else {
        // 线性遍历（无四叉树时）
        for (const auto& boundary : structure->boundaries) {
            renderBoundary(painter, boundary);
            renderedCount++;
        }
        for (const auto& path : structure->paths) {
            renderPath(painter, path);
            renderedCount++;
        }
        for (const auto& text : structure->texts) {
            renderText(painter, text);
            renderedCount++;
        }
    }
    
    qDebug() << "RenderWorker[" << m_tileIndex << "] 渲染图形数:" << renderedCount;
}

void RenderWorker::renderBoundary(QPainter& painter, const GDSBoundary& boundary)
{
    if (boundary.points.size() < 3) return;
    
    QColor fillColor = getLayerColor(boundary.layer);
    fillColor.setAlpha(180);
    QColor borderColor = fillColor.darker(150);
    
    // LOD 处理
    if (m_lod == LODLevel::BoundingBox) {
        // 只画边界框
        double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
        for (const auto& p : boundary.points) {
            double x = p.x / 1000.0;  // nm -> μm
            double y = p.y / 1000.0;
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
            minY = qMin(minY, y);
            maxY = qMax(maxY, y);
        }
        
        QPointF tl = worldToScreen(QPointF(minX, maxY));
        QPointF br = worldToScreen(QPointF(maxX, minY));
        
        painter.setPen(QPen(borderColor, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(tl, br));
        return;
    }
    
    // 绘制多边形
    QPolygonF poly;
    poly.reserve(boundary.points.size());
    
    for (const auto& p : boundary.points) {
        double x = p.x / 1000.0;  // nm -> μm
        double y = p.y / 1000.0;
        poly.append(worldToScreen(QPointF(x, y)));
    }
    
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(QBrush(fillColor));
    painter.drawPolygon(poly);
}

void RenderWorker::renderPath(QPainter& painter, const GDSPath& path)
{
    if (path.points.size() < 2) return;
    
    QColor color = getLayerColor(path.layer);
    double penWidth = qMax(1.0, static_cast<double>(path.width) / 1000.0 * m_renderScale);
    
    QPainterPath pp;
    bool first = true;
    
    for (const auto& p : path.points) {
        double x = p.x / 1000.0;
        double y = p.y / 1000.0;
        QPointF screen = worldToScreen(QPointF(x, y));
        
        if (first) {
            pp.moveTo(screen);
            first = false;
        } else {
            pp.lineTo(screen);
        }
    }
    
    painter.setPen(QPen(color, penWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(pp);
}

void RenderWorker::renderText(QPainter& painter, const GDSText& text)
{
    QColor color = getLayerColor(text.layer);
    
    double x = text.position.x / 1000.0;
    double y = text.position.y / 1000.0;
    QPointF screen = worldToScreen(QPointF(x, y));
    
    painter.setPen(color);
    painter.save();
    painter.translate(screen);
    painter.rotate(-text.angle);
    painter.drawText(QPointF(0, 0), text.text);
    painter.restore();
}

QPointF RenderWorker::worldToScreen(const QPointF& world) const
{
    // Y 轴翻转
    double x = (world.x() - m_viewport.left()) * m_renderScale;
    double y = (m_viewport.bottom() - world.y()) * m_renderScale;
    return QPointF(x, y);
}

QColor RenderWorker::getLayerColor(int layer) const
{
    int hue = (layer * 137) % 360;
    return QColor::fromHsl(hue, 180, 160);
}

} // namespace QLayoutEDA