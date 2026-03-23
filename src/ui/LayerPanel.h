/**
 * @file LayerPanel.h
 * @brief 图层面板
 * @author QLayoutEDA Team
 */

#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include "core/Types.h"

namespace QLayoutEDA {

class LayerManager;

/**
 * @brief 图层面板
 * 
 * 显示图层列表，支持可见性切换和样式编辑
 */
class LayerPanel : public QWidget {
    Q_OBJECT

public:
    explicit LayerPanel(QWidget* parent = nullptr);
    ~LayerPanel() override;
    
    /**
     * @brief 设置图层管理器
     */
    void setLayerManager(LayerManager* manager);
    
    /**
     * @brief 刷新图层列表
     */
    void refreshLayers();
    
    /**
     * @brief 获取当前选中图层
     */
    int currentLayer() const;
    
    /**
     * @brief 设置图层列表（从GDS数据提取）
     */
    void setLayers(const QList<int>& layers);

signals:
    /**
     * @brief 当前图层变化信号
     */
    void currentLayerChanged(int layer);
    
    /**
     * @brief 图层可见性变化信号
     */
    void layerVisibilityChanged(int layer, bool visible);
    
    /**
     * @brief 图层样式编辑请求
     */
    void editLayerStyle(int layer);

private slots:
    void onCellClicked(int row, int column);
    void onCellDoubleClicked(int row, int column);
    void onItemChanged(QTableWidgetItem* item);

private:
    void setupUi();
    void populateLayers();
    void updateLayerRow(int row, const LayerData& data);
    QColor getLayerColor(int layer) const;

private:
    QTableWidget* m_table = nullptr;
    LayerManager* m_layerManager = nullptr;
    
    // 图层颜色预设
    static const QVector<QColor> s_layerColors;
};

} // namespace QLayoutEDA