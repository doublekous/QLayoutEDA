/**
 * @file IRenderEngine.h
 * @brief 渲染引擎接口
 */

#pragma once

#include "core/Types.h"
#include <QPainter>
#include <QObject>

namespace QLayoutEDA {

/**
 * @brief 渲染引擎接口
 * 
 * 负责 GDS 数据的可视化渲染
 */
class IRenderEngine {
public:
    virtual ~IRenderEngine() = default;
    
    /**
     * @brief 初始化渲染引擎
     */
    virtual bool initialize(const RenderConfig& config) = 0;
    
    /**
     * @brief 设置数据
     */
    virtual void setData(GDSDataPtr data) = 0;
    
    /**
     * @brief 设置当前结构
     */
    virtual void setCurrentStructure(const QString& name) = 0;
    
    /**
     * @brief 渲染
     */
    virtual void render(QPainter* painter, const QRectF& viewport, 
                        const ViewTransform& transform) = 0;
    
    /**
     * @brief 设置图层样式
     */
    virtual void setLayerStyles(const QVector<LayerStyle>& styles) = 0;
    
    /**
     * @brief 获取图层样式
     */
    virtual LayerStyle getLayerStyle(int layer) const = 0;
    
    /**
     * @brief 设置图层可见性
     */
    virtual void setLayerVisible(int layer, bool visible) = 0;
    
    /**
     * @brief 获取可见图层列表
     */
    virtual QVector<int> getVisibleLayers() const = 0;
    
    /**
     * @brief 拾取对象
     */
    virtual QVector<quint64> pickObjects(const QPointF& screenPos) const = 0;
    
    /**
     * @brief 高亮对象
     */
    virtual void highlightObject(quint64 id, const QColor& color) = 0;
    
    /**
     * @brief 清除高亮
     */
    virtual void clearHighlights() = 0;
    
    /**
     * @brief 导出图像
     */
    virtual bool exportImage(const QString& filePath, int width, int height) = 0;
    
    /**
     * @brief 获取渲染统计
     */
    virtual QString getStats() const = 0;
};

} // namespace QLayoutEDA

Q_DECLARE_INTERFACE(QLayoutEDA::IRenderEngine, "com.QLayoutEDA.IRenderEngine/1.0")