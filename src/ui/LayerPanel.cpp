/**
 * @file LayerPanel.cpp
 * @brief 图层面板实现
 * @author QLayoutEDA Team
 */

#include "LayerPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QHeaderView>
#include <QColorDialog>
#include <QDebug>
#include <algorithm>

namespace QLayoutEDA {

//=============================================================================
// 静态常量
//=============================================================================

const QVector<QColor> LayerPanel::s_layerColors = {
    QColor(255, 0, 0),      // 红
    QColor(0, 255, 0),      // 绿
    QColor(0, 0, 255),      // 蓝
    QColor(255, 255, 0),    // 黄
    QColor(255, 0, 255),    // 紫
    QColor(0, 255, 255),    // 青
    QColor(255, 128, 0),    // 橙
    QColor(128, 0, 255),    // 紫罗兰
    QColor(0, 128, 255),    // 天蓝
    QColor(255, 0, 128),    // 玫红
    QColor(128, 255, 0),    // 黄绿
    QColor(0, 255, 128),    // 春绿
    QColor(128, 128, 255),  // 淡蓝
    QColor(255, 128, 128),  // 淡红
    QColor(128, 255, 128),  // 淡绿
    QColor(200, 200, 200),  // 灰
};

//=============================================================================
// 枚举定义
//=============================================================================

enum LayerColumn {
    Col_Visible = 0,    ///< 可见性
    Col_Selectable,     ///< 可选性
    Col_Color,          ///< 颜色
    Col_Name,           ///< 图层名
    Col_LayerNo,        ///< 层号
    Col_Count           ///< 列数
};

//=============================================================================
// 构造/析构
//=============================================================================

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    populateLayers();
}

LayerPanel::~LayerPanel() = default;

//=============================================================================
// 公共方法
//=============================================================================

void LayerPanel::setLayerManager(LayerManager* manager)
{
    m_layerManager = manager;
    refreshLayers();
}

void LayerPanel::refreshLayers()
{
    populateLayers();
}

int LayerPanel::currentLayer() const
{
    int row = m_table->currentRow();
    if (row >= 0) {
        auto* item = m_table->item(row, Col_LayerNo);
        if (item) {
            return item->data(Qt::DisplayRole).toInt();
        }
    }
    return 0;
}

void LayerPanel::setLayers(const QList<int>& layers)
{
    m_table->setRowCount(0);
    
    // 按层号排序
    QList<int> sortedLayers = layers;
    std::sort(sortedLayers.begin(), sortedLayers.end());
    
    m_table->setRowCount(sortedLayers.size());
    
    for (int i = 0; i < sortedLayers.size(); ++i) {
        int layerNo = sortedLayers[i];
        LayerData layerData;
        layerData.layer = layerNo;
        layerData.name = QString("Layer %1").arg(layerNo);
        layerData.style.visible = true;
        layerData.style.selectable = true;
        layerData.style.fillColor = getLayerColor(layerNo);
        layerData.style.borderColor = getLayerColor(layerNo).darker(120);
        
        updateLayerRow(i, layerData);
    }
    
    // 选中第一行
    if (m_table->rowCount() > 0) {
        m_table->selectRow(0);
    }
}

//=============================================================================
// 私有方法
//=============================================================================

void LayerPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // 标题栏
    auto* titleLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel(tr("Layers"));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    // 添加图层按钮
    auto* addBtn = new QPushButton(tr("+"));
    addBtn->setFixedSize(24, 24);
    addBtn->setToolTip(tr("Add Layer"));
    connect(addBtn, &QPushButton::clicked, this, []() {
        // TODO: 添加图层
    });
    titleLayout->addWidget(addBtn);
    
    mainLayout->addLayout(titleLayout);
    
    // 图层表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(Col_Count);
    m_table->setHorizontalHeaderLabels({
        tr("V"), tr("S"), tr("Color"), tr("Name"), tr("Layer")
    });
    
    // 设置表头
    m_table->horizontalHeader()->setSectionResizeMode(Col_Visible, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(Col_Selectable, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(Col_Color, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(Col_Name, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(Col_LayerNo, QHeaderView::Fixed);
    
    m_table->setColumnWidth(Col_Visible, 25);
    m_table->setColumnWidth(Col_Selectable, 25);
    m_table->setColumnWidth(Col_Color, 30);
    m_table->setColumnWidth(Col_LayerNo, 40);
    
    // 设置表格样式
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 连接信号
    connect(m_table, &QTableWidget::cellClicked, this, &LayerPanel::onCellClicked);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &LayerPanel::onCellDoubleClicked);
    
    mainLayout->addWidget(m_table);
    
    // 底部按钮栏
    auto* btnLayout = new QHBoxLayout();
    
    auto* editBtn = new QPushButton(tr("Edit"));
    editBtn->setToolTip(tr("Edit layer style"));
    connect(editBtn, &QPushButton::clicked, this, [this]() {
        emit editLayerStyle(currentLayer());
    });
    btnLayout->addWidget(editBtn);
    
    auto* allVisibleBtn = new QPushButton(tr("All"));
    allVisibleBtn->setToolTip(tr("Show all layers"));
    connect(allVisibleBtn, &QPushButton::clicked, this, [this]() {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto* item = m_table->item(row, Col_Visible);
            if (item) {
                item->setCheckState(Qt::Checked);
            }
        }
    });
    btnLayout->addWidget(allVisibleBtn);
    
    auto* noneVisibleBtn = new QPushButton(tr("None"));
    noneVisibleBtn->setToolTip(tr("Hide all layers"));
    connect(noneVisibleBtn, &QPushButton::clicked, this, [this]() {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto* item = m_table->item(row, Col_Visible);
            if (item) {
                item->setCheckState(Qt::Unchecked);
            }
        }
    });
    btnLayout->addWidget(noneVisibleBtn);
    
    mainLayout->addLayout(btnLayout);
}

void LayerPanel::populateLayers()
{
    m_table->setRowCount(0);
    
    // 定义默认图层（模拟 GDS 常用图层）
    struct DefaultLayer {
        int layerNo;
        QString name;
    };
    
    QVector<DefaultLayer> defaultLayers = {
        {1, "Metal1"},
        {2, "Metal2"},
        {3, "Metal3"},
        {4, "Metal4"},
        {5, "Metal5"},
        {10, "Poly"},
        {11, "Poly2"},
        {20, "Active"},
        {21, "P-Active"},
        {22, "N-Active"},
        {30, "N-Well"},
        {31, "P-Well"},
        {40, "Contact"},
        {41, "Via1"},
        {42, "Via2"},
        {43, "Via3"},
        {50, "N-Implant"},
        {51, "P-Implant"},
        {60, "Pad"},
        {100, "Text"},
    };
    
    m_table->setRowCount(defaultLayers.size());
    
    for (int i = 0; i < defaultLayers.size(); ++i) {
        const auto& layer = defaultLayers[i];
        LayerData layerData;
        layerData.layer = layer.layerNo;
        layerData.name = layer.name;
        layerData.style.visible = true;
        layerData.style.selectable = true;
        layerData.style.fillColor = getLayerColor(layer.layerNo);
        layerData.style.borderColor = getLayerColor(layer.layerNo).darker(120);
        
        updateLayerRow(i, layerData);
    }
    
    // 选中第一行
    if (m_table->rowCount() > 0) {
        m_table->selectRow(0);
    }
}

void LayerPanel::updateLayerRow(int row, const LayerData& data)
{
    // 可见性复选框
    auto* visibleItem = new QTableWidgetItem();
    visibleItem->setCheckState(data.style.visible ? Qt::Checked : Qt::Unchecked);
    visibleItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, Col_Visible, visibleItem);
    
    // 可选性复选框
    auto* selectableItem = new QTableWidgetItem();
    selectableItem->setCheckState(data.style.selectable ? Qt::Checked : Qt::Unchecked);
    selectableItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, Col_Selectable, selectableItem);
    
    // 颜色预览
    auto* colorItem = new QTableWidgetItem();
    colorItem->setBackground(data.style.fillColor);
    colorItem->setToolTip(tr("Double-click to change color"));
    m_table->setItem(row, Col_Color, colorItem);
    
    // 图层名
    auto* nameItem = new QTableWidgetItem(data.name);
    m_table->setItem(row, Col_Name, nameItem);
    
    // 层号
    auto* layerItem = new QTableWidgetItem(QString::number(data.layer));
    layerItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, Col_LayerNo, layerItem);
    
    // 存储图层数据
    QVariant v;
    v.setValue(data);
    visibleItem->setData(Qt::UserRole, v);
}

QColor LayerPanel::getLayerColor(int layer) const
{
    return s_layerColors[layer % s_layerColors.size()];
}

//=============================================================================
// 槽函数
//=============================================================================

void LayerPanel::onCellClicked(int row, int column)
{
    Q_UNUSED(row);
    
    if (column == Col_Visible || column == Col_Selectable) {
        // 切换状态
        auto* item = m_table->item(row, column);
        if (item) {
            Qt::CheckState state = item->checkState();
            item->setCheckState(state == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            
            if (column == Col_Visible) {
                int layerNo = m_table->item(row, Col_LayerNo)->text().toInt();
                emit layerVisibilityChanged(layerNo, item->checkState() == Qt::Checked);
            }
        }
    }
}

void LayerPanel::onCellDoubleClicked(int row, int column)
{
    if (column == Col_Color) {
        // 打开颜色选择器
        auto* item = m_table->item(row, Col_Color);
        QColor currentColor = item->background().color();
        
        QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Layer Color"));
        if (newColor.isValid()) {
            item->setBackground(newColor);
            
            // 更新边框颜色
            auto* borderItem = m_table->item(row, Col_Color);
            if (borderItem) {
                borderItem->setBackground(newColor);
            }
        }
    } else if (column == Col_Name) {
        // TODO: 编辑图层名称
    }
}

void LayerPanel::onItemChanged(QTableWidgetItem* item)
{
    if (!item) return;
    
    int column = item->column();
    int layerNo = m_table->item(item->row(), Col_LayerNo)->text().toInt();
    
    if (column == Col_Visible) {
        bool visible = item->checkState() == Qt::Checked;
        emit layerVisibilityChanged(layerNo, visible);
    }
}

} // namespace QLayoutEDA