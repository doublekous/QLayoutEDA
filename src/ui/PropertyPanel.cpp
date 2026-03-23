/**
 * @file PropertyPanel.cpp
 * @brief 属性面板实现
 * @author QLayoutEDA Team
 */

#include "PropertyPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QtMath>

namespace QLayoutEDA {

//=============================================================================
// 构造/析构
//=============================================================================

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    clear();
}

PropertyPanel::~PropertyPanel() = default;

//=============================================================================
// 公共方法
//=============================================================================

void PropertyPanel::setObject(const QString& type, const QRectF& bounds, int layer)
{
    m_objectType = type;
    m_objectBounds = bounds;
    m_objectLayer = layer;
    
    updateDisplay();
    enableEditing(true);
}

void PropertyPanel::clear()
{
    m_objectType.clear();
    m_objectBounds = QRectF();
    m_objectLayer = 0;
    m_rotation = 0.0;
    
    if (m_typeLabel) m_typeLabel->setText(tr("None"));
    if (m_idLabel) m_idLabel->setText("-");
    
    enableEditing(false);
}

//=============================================================================
// 私有方法
//=============================================================================

void PropertyPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // 标题
    auto* titleLabel = new QLabel(tr("Properties"));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    mainLayout->addWidget(titleLabel);
    
    // 基本信息组
    auto* infoGroup = new QGroupBox(tr("Object Info"));
    auto* infoLayout = new QFormLayout(infoGroup);
    infoLayout->setSpacing(4);
    
    m_typeLabel = new QLabel("-");
    infoLayout->addRow(tr("Type:"), m_typeLabel);
    
    m_idLabel = new QLabel("-");
    infoLayout->addRow(tr("ID:"), m_idLabel);
    
    mainLayout->addWidget(infoGroup);
    
    // 图层组
    auto* layerGroup = new QGroupBox(tr("Layer"));
    auto* layerLayout = new QHBoxLayout(layerGroup);
    layerLayout->setSpacing(4);
    
    m_layerCombo = new QComboBox();
    m_layerCombo->addItem("Metal1 (1)", 1);
    m_layerCombo->addItem("Metal2 (2)", 2);
    m_layerCombo->addItem("Metal3 (3)", 3);
    m_layerCombo->addItem("Metal4 (4)", 4);
    m_layerCombo->addItem("Metal5 (5)", 5);
    m_layerCombo->addItem("Poly (10)", 10);
    m_layerCombo->addItem("Active (20)", 20);
    m_layerCombo->addItem("N-Well (30)", 30);
    m_layerCombo->addItem("Contact (40)", 40);
    m_layerCombo->addItem("Via1 (41)", 41);
    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PropertyPanel::onLayerChanged);
    
    layerLayout->addWidget(m_layerCombo);
    mainLayout->addWidget(layerGroup);
    
    // 位置组
    auto* posGroup = new QGroupBox(tr("Position"));
    auto* posLayout = new QFormLayout(posGroup);
    posLayout->setSpacing(4);
    
    m_xSpin = new QDoubleSpinBox();
    m_xSpin->setRange(-1e9, 1e9);
    m_xSpin->setDecimals(3);
    m_xSpin->setSuffix(" μm");
    m_xSpin->setSingleStep(0.1);
    connect(m_xSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &PropertyPanel::onXChanged);
    posLayout->addRow(tr("X:"), m_xSpin);
    
    m_ySpin = new QDoubleSpinBox();
    m_ySpin->setRange(-1e9, 1e9);
    m_ySpin->setDecimals(3);
    m_ySpin->setSuffix(" μm");
    m_ySpin->setSingleStep(0.1);
    connect(m_ySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &PropertyPanel::onYChanged);
    posLayout->addRow(tr("Y:"), m_ySpin);
    
    mainLayout->addWidget(posGroup);
    
    // 尺寸组
    auto* sizeGroup = new QGroupBox(tr("Size"));
    auto* sizeLayout = new QFormLayout(sizeGroup);
    sizeLayout->setSpacing(4);
    
    m_widthSpin = new QDoubleSpinBox();
    m_widthSpin->setRange(0, 1e9);
    m_widthSpin->setDecimals(3);
    m_widthSpin->setSuffix(" μm");
    m_widthSpin->setSingleStep(0.1);
    connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &PropertyPanel::onWidthChanged);
    sizeLayout->addRow(tr("Width:"), m_widthSpin);
    
    m_heightSpin = new QDoubleSpinBox();
    m_heightSpin->setRange(0, 1e9);
    m_heightSpin->setDecimals(3);
    m_heightSpin->setSuffix(" μm");
    m_heightSpin->setSingleStep(0.1);
    connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &PropertyPanel::onHeightChanged);
    sizeLayout->addRow(tr("Height:"), m_heightSpin);
    
    mainLayout->addWidget(sizeGroup);
    
    // 变换组
    auto* transformGroup = new QGroupBox(tr("Transform"));
    auto* transformLayout = new QFormLayout(transformGroup);
    transformLayout->setSpacing(4);
    
    m_rotationSpin = new QDoubleSpinBox();
    m_rotationSpin->setRange(-360, 360);
    m_rotationSpin->setDecimals(1);
    m_rotationSpin->setSuffix(" °");
    m_rotationSpin->setSingleStep(1.0);
    connect(m_rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &PropertyPanel::onRotationChanged);
    transformLayout->addRow(tr("Rotation:"), m_rotationSpin);
    
    mainLayout->addWidget(transformGroup);
    
    // 只读信息组
    auto* readonlyGroup = new QGroupBox(tr("Calculated"));
    auto* readonlyLayout = new QFormLayout(readonlyGroup);
    readonlyLayout->setSpacing(4);
    
    m_areaLabel = new QLabel("-");
    readonlyLayout->addRow(tr("Area:"), m_areaLabel);
    
    m_perimeterLabel = new QLabel("-");
    readonlyLayout->addRow(tr("Perimeter:"), m_perimeterLabel);
    
    mainLayout->addWidget(readonlyGroup);
    
    // 添加弹性空间
    mainLayout->addStretch();
    
    // 操作按钮
    auto* btnLayout = new QHBoxLayout();
    
    auto* applyBtn = new QPushButton(tr("Apply"));
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        // TODO: 应用属性更改
    });
    btnLayout->addWidget(applyBtn);
    
    auto* resetBtn = new QPushButton(tr("Reset"));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        updateDisplay();
    });
    btnLayout->addWidget(resetBtn);
    
    mainLayout->addLayout(btnLayout);
}

void PropertyPanel::updateDisplay()
{
    m_updating = true;
    
    // 基本信息
    m_typeLabel->setText(m_objectType.isEmpty() ? tr("None") : m_objectType);
    
    // 图层
    int layerIndex = m_layerCombo->findData(m_objectLayer);
    if (layerIndex >= 0) {
        m_layerCombo->setCurrentIndex(layerIndex);
    }
    
    // 位置（中心点）
    m_xSpin->setValue(m_objectBounds.center().x());
    m_ySpin->setValue(m_objectBounds.center().y());
    
    // 尺寸
    m_widthSpin->setValue(m_objectBounds.width());
    m_heightSpin->setValue(m_objectBounds.height());
    
    // 旋转
    m_rotationSpin->setValue(m_rotation);
    
    // 计算值
    double area = m_objectBounds.width() * m_objectBounds.height();
    double perimeter = 2 * (m_objectBounds.width() + m_objectBounds.height());
    
    m_areaLabel->setText(QString::number(area, 'f', 3) + " μm²");
    m_perimeterLabel->setText(QString::number(perimeter, 'f', 3) + " μm");
    
    m_updating = false;
}

void PropertyPanel::enableEditing(bool enabled)
{
    m_layerCombo->setEnabled(enabled);
    m_xSpin->setEnabled(enabled);
    m_ySpin->setEnabled(enabled);
    m_widthSpin->setEnabled(enabled);
    m_heightSpin->setEnabled(enabled);
    m_rotationSpin->setEnabled(enabled);
}

//=============================================================================
// 槽函数
//=============================================================================

void PropertyPanel::onLayerChanged(int index)
{
    if (m_updating) return;
    
    int layer = m_layerCombo->itemData(index).toInt();
    m_objectLayer = layer;
    emit propertyChanged("layer", layer);
}

void PropertyPanel::onXChanged(double value)
{
    if (m_updating) return;
    
    double offsetX = value - m_objectBounds.center().x();
    m_objectBounds.moveLeft(m_objectBounds.left() + offsetX);
    emit propertyChanged("x", value);
}

void PropertyPanel::onYChanged(double value)
{
    if (m_updating) return;
    
    double offsetY = value - m_objectBounds.center().y();
    m_objectBounds.moveTop(m_objectBounds.top() + offsetY);
    emit propertyChanged("y", value);
}

void PropertyPanel::onWidthChanged(double value)
{
    if (m_updating) return;
    
    double center = m_objectBounds.center().x();
    m_objectBounds.setWidth(value);
    m_objectBounds.moveCenter(QPointF(center, m_objectBounds.center().y()));
    emit propertyChanged("width", value);
    
    // 更新面积和周长
    updateDisplay();
}

void PropertyPanel::onHeightChanged(double value)
{
    if (m_updating) return;
    
    double center = m_objectBounds.center().y();
    m_objectBounds.setHeight(value);
    m_objectBounds.moveCenter(QPointF(m_objectBounds.center().x(), center));
    emit propertyChanged("height", value);
    
    // 更新面积和周长
    updateDisplay();
}

void PropertyPanel::onRotationChanged(double value)
{
    if (m_updating) return;
    
    m_rotation = value;
    emit propertyChanged("rotation", value);
}

} // namespace QLayoutEDA