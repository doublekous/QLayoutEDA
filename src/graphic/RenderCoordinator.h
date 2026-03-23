/**
 * @file RenderCoordinator.h
 * @brief 多线程渲染协调器
 */

#ifndef RENDERCOORDINATOR_H
#define RENDERCOORDINATOR_H

#include "core/Types.h"  // 包含全局 LODLevel
#include <QObject>
#include <QImage>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QRectF>
#include <QSize>
#include <memory>

namespace QLayoutEDA {

class QuadTree;
struct GDSData;

struct RenderTask {
    QString cellName;
    QRectF viewport;
    double scale;
    QSize imageSize;
    LODLevel lod;
};

struct RenderResult {
    QImage image;
    QString cellName;
    qint64 renderTime;
    qint64 objectCount;
    int shapeCount;
    bool success;
};

class RenderCoordinator : public QObject {
    Q_OBJECT
public:
    explicit RenderCoordinator(QObject* parent = nullptr);
    
    void setData(GDSDataPtr data);
    void setDataSource(GDSDataPtr data) { setData(data); }
    void setQuadTree(QuadTree* tree);
    void setSpatialIndex(QuadTree* tree) { setQuadTree(tree); }
    
    quint64 requestRender(const RenderTask& task);
    void cancelRender();
    bool isRendering() const;
    
    static LODLevel calculateLOD(double scale);
    
signals:
    void renderComplete(const RenderResult& result);
    void renderProgress(int percent);
    
private:
    RenderResult doRender(const RenderTask& task);
    
    QThreadPool* m_pool;
    QFutureWatcher<RenderResult>* m_watcher;
    GDSDataPtr m_data;
    QuadTree* m_quadTree;
    bool m_cancelled;
    quint64 m_taskId;
};

} // namespace QLayoutEDA

#endif