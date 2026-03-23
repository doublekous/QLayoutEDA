/**
 * @file PropertyPanel.h
 * @brief 属性面板
 * @author QLayoutEDA Team
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include "core/Types.h"

namespace QLayoutEDA {

/**
 * @brief 属性面板
 * 
 * 显示和编辑选中对象的属性
 */
class PropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);
    ~PropertyPanel() override;
    
    /**
     * @brief 设置显示的对象
     */
    void setObject(const QString& type, const QRectF& bounds, int layer);
    
    /**
     * @brief 清空属性显示
     */
    void clear();

signals:
    /**
     * @brief 属性变化信号
     */
    void propertyChanged(const QString& name, const QVariant& value);

private slots:
    void onLayerChanged(int index);
    void onXChanged(double value);
    void onYChanged(double value);
    void onWidthChanged(double value);
    void onHeightChanged(double value);
    void onRotationChanged(double value);

private:
    void setupUi();
    void updateDisplay();
    void enableEditing(bool enabled);

private:
    // 对象信息
    QString m_objectType;
    QRectF m_objectBounds;
    int m_objectLayer = 0;
    double m_rotation = 0.0;
    
    // UI 组件 - 基本信息
    QLabel* m_typeLabel = nullptr;
    QLabel* m_idLabel = nullptr;
    
    // UI 组件 - 图层
    QComboBox* m_layerCombo = nullptr;
    
    // UI 组件 - 位置
    QDoubleSpinBox* m_xSpin = nullptr;
    QDoubleSpinBox* m_ySpin = nullptr;
    
    // UI 组件 - 尺寸
    QDoubleSpinBox* m_widthSpin = nullptr;
    QDoubleSpinBox* m_heightSpin = nullptr;
    
    // UI 组件 - 旋转
    QDoubleSpinBox* m_rotationSpin = nullptr;
    
    // UI 组件 - 只读信息
    QLabel* m_areaLabel = nullptr;
    QLabel* m_perimeterLabel = nullptr;
    
    // 状态
    bool m_updating = false;
};

} // namespace QLayoutEDA