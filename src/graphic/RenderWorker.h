/**
 * @file RenderWorker.h
 * @brief 渲染工作线程
 */

#pragma once

#include "QuadTree.h"
#include "core/Types.h"  // 包含全局 LODLevel
#include <QRunnable>
#include <QObject>
#include <QImage>
#include <QPainterPath>

namespace QLayoutEDA {

/**
 * @brief 渲染工作线程
 * 
 * 在线程池中执行，负责渲染一个瓦片区域
 */
class RenderWorker : public QObject, public QRunnable {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     */
    RenderWorker(int taskId, int tileIndex, const QRectF& viewport,
                 double scale, const QSize& imageSize, LODLevel lod,
                 GDSDataPtr data, QuadTree* spatialIndex);
    
    ~RenderWorker() override = default;
    
    /**
     * @brief 执行渲染
     */
    void run() override;
    
    /**
     * @brief 获取任务ID
     */
    int getTaskId() const { return m_taskId; }
    
    /**
     * @brief 获取渲染结果
     */
    QImage getResult() const { return m_result; }
    
signals:
    /**
     * @brief 渲染完成信号
     */
    void finished();
    
private:
    /**
     * @brief 渲染图形
     */
    void renderShapes(QPainter& painter);
    
    /**
     * @brief 渲染 Boundary
     */
    void renderBoundary(QPainter& painter, const GDSBoundary& boundary);
    
    /**
     * @brief 渲染 Path
     */
    void renderPath(QPainter& painter, const GDSPath& path);
    
    /**
     * @brief 渲染 Text
     */
    void renderText(QPainter& painter, const GDSText& text);
    
    /**
     * @brief 世界坐标转屏幕坐标
     */
    QPointF worldToScreen(const QPointF& world) const;
    
    /**
     * @brief 获取图层颜色
     */
    QColor getLayerColor(int layer) const;
    
    // 任务参数
    int m_taskId;
    int m_tileIndex;
    QRectF m_viewport;
    double m_scale;
    QSize m_imageSize;
    LODLevel m_lod;
    
    // 数据源
    GDSDataPtr m_data;
    QuadTree* m_spatialIndex;
    
    // 结果
    QImage m_result;
    
    // 坐标转换参数
    double m_offsetX = 0;
    double m_offsetY = 0;
    double m_renderScale = 1.0;
};

} // namespace QLayoutEDA