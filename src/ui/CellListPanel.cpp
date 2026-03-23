/**
 * @file CellListPanel.cpp
 * @brief Cell 列表面板实现（树形结构）- 参考 KLayout
 * @author QLayoutEDA Team
 */

#include "CellListPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QHeaderView>
#include <QQueue>
#include <QDebug>

namespace QLayoutEDA {

//=============================================================================
// 构造/析构
//=============================================================================

CellListPanel::CellListPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

CellListPanel::~CellListPanel() = default;

//=============================================================================
// 公共方法
//=============================================================================

void CellListPanel::setGDSData(GDSDataPtr data)
{
    m_data = data;
    m_currentCell.clear();
    m_cellNodes.clear();
    m_topLevelCells.clear();
    m_treeItems.clear();
    
    if (data) {
        buildCellHierarchy();
    }
    
    populateTree();
}

void CellListPanel::setCurrentCell(const QString& cellName)
{
    if (m_currentCell == cellName) {
        return;
    }
    
    m_currentCell = cellName;
    
    // 更新树形列表选中状态
    if (m_treeItems.contains(cellName)) {
        QTreeWidgetItem* item = m_treeItems[cellName];
        m_treeWidget->setCurrentItem(item);
        m_treeWidget->scrollToItem(item);
    }
}

QString CellListPanel::currentCell() const
{
    return m_currentCell;
}

void CellListPanel::refreshList()
{
    populateTree();
}

void CellListPanel::expandAll()
{
    m_treeWidget->expandAll();
}

void CellListPanel::collapseAll()
{
    m_treeWidget->collapseAll();
}

//=============================================================================
// 私有方法
//=============================================================================

void CellListPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // 标题栏
    auto* titleLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel(tr("Cells"));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    // 展开/折叠按钮
    auto* expandBtn = new QPushButton(tr("+"));
    expandBtn->setFixedSize(24, 24);
    expandBtn->setToolTip(tr("Expand All"));
    connect(expandBtn, &QPushButton::clicked, this, &CellListPanel::expandAll);
    titleLayout->addWidget(expandBtn);
    
    auto* collapseBtn = new QPushButton(tr("-"));
    collapseBtn->setFixedSize(24, 24);
    collapseBtn->setToolTip(tr("Collapse All"));
    connect(collapseBtn, &QPushButton::clicked, this, &CellListPanel::collapseAll);
    titleLayout->addWidget(collapseBtn);
    
    mainLayout->addLayout(titleLayout);
    
    // 过滤输入框
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter cells..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &CellListPanel::onFilterChanged);
    mainLayout->addWidget(m_filterEdit);
    
    // 树形控件
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << tr("Cell") << tr("Objects"));
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setAnimated(true);
    
    // 设置列宽
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_treeWidget->setColumnWidth(1, 60);
    
    // 连接信号
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &CellListPanel::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &CellListPanel::onItemDoubleClicked);
    
    mainLayout->addWidget(m_treeWidget);
    
    // 状态标签
    auto* statusLayout = new QHBoxLayout();
    auto* statusLabel = new QLabel(tr("No file loaded"));
    statusLabel->setObjectName("statusLabel");
    statusLayout->addWidget(statusLabel);
    mainLayout->addLayout(statusLayout);
}

void CellListPanel::buildCellHierarchy()
{
    if (!m_data) return;
    
    qDebug() << "=== Building Cell Hierarchy (KLayout Style) ===";
    
    // 1. 初始化所有 Cell 节点
    QSet<QString> allCells;
    for (auto it = m_data->structures.begin(); it != m_data->structures.end(); ++it) {
        const QString& name = it.key();
        auto structure = it.value();
        
        CellNode node;
        node.name = name;
        node.structure = structure;
        node.isTopLevel = true;  // 初始假设都是顶层
        
        m_cellNodes[name] = node;
        allCells.insert(name);
    }
    
    // 2. 收集所有被引用的 Cell 名称
    QSet<QString> referencedCells;
    for (auto it = m_data->structures.begin(); it != m_data->structures.end(); ++it) {
        auto structure = it.value();
        if (!structure) continue;
        
        // 收集 SREF 引用
        for (const auto& ref : structure->references) {
            referencedCells.insert(ref.structureName);
        }
        
        // 收集 AREF 引用
        for (const auto& arr : structure->arrays) {
            referencedCells.insert(arr.structureName);
        }
    }
    
    qDebug() << "Total cells:" << allCells.size();
    qDebug() << "Referenced cells:" << referencedCells.size();
    
    // 3. 建立父子关系
    for (auto it = m_data->structures.begin(); it != m_data->structures.end(); ++it) {
        const QString& parentName = it.key();
        auto structure = it.value();
        
        if (!structure) continue;
        
        // 处理 SREF
        for (const auto& ref : structure->references) {
            const QString& childName = ref.structureName;
            
            if (m_cellNodes.contains(childName)) {
                m_cellNodes[parentName].children.insert(childName);
                m_cellNodes[childName].parents.insert(parentName);
                // 标记为非顶层（被引用）
                m_cellNodes[childName].isTopLevel = false;
            }
        }
        
        // 处理 AREF
        for (const auto& arr : structure->arrays) {
            const QString& childName = arr.structureName;
            
            if (m_cellNodes.contains(childName)) {
                m_cellNodes[parentName].children.insert(childName);
                m_cellNodes[childName].parents.insert(parentName);
                m_cellNodes[childName].isTopLevel = false;
            }
        }
    }
    
    // 4. 确定顶层 Cell（不被任何其他 Cell 引用）
    m_topLevelCells.clear();
    for (auto it = m_cellNodes.begin(); it != m_cellNodes.end(); ++it) {
        if (it->isTopLevel) {
            m_topLevelCells.insert(it->name);
        }
    }
    
    // 5. 如果 GDS 指定了顶层结构，添加到顶层列表
    if (!m_data->topStructure.isEmpty()) {
        if (m_cellNodes.contains(m_data->topStructure)) {
            m_topLevelCells.insert(m_data->topStructure);
            // 如果这个 Cell 被引用，说明有循环引用，需要特殊处理
            if (m_cellNodes[m_data->topStructure].parents.size() > 0) {
                qDebug() << "Warning: topStructure" << m_data->topStructure 
                         << "is referenced by other cells, treating as top-level anyway";
            }
        }
    }
    
    // 6. 如果没有顶层 Cell，将第一个结构作为顶层
    if (m_topLevelCells.isEmpty() && !m_cellNodes.isEmpty()) {
        QString firstCell = m_cellNodes.begin().key();
        m_topLevelCells.insert(firstCell);
    }
    
    // 7. 输出调试信息
    qDebug() << "\n=== Top-level Cells (" << m_topLevelCells.size() << ") ===";
    for (const QString& name : m_topLevelCells) {
        qDebug() << "  " << name;
        if (m_cellNodes.contains(name)) {
            qDebug() << "    - children:" << m_cellNodes[name].children.size();
        }
    }
    
    // 8. 特别检查用户关注的四个 Cell
    QStringList focusCells = {
        "Res_bansui_1_ab_Flip_$1",
        "Singlebit_V8_2_GRID_ARRAY(1)$1", 
        "TiNPad&Inpillar(3)",
        "topcell000111"
    };
    
    qDebug() << "\n=== Focus Cells Analysis ===";
    for (const QString& name : focusCells) {
        if (m_cellNodes.contains(name)) {
            const CellNode& node = m_cellNodes[name];
            qDebug() << "\nCell:" << name;
            qDebug() << "  isTopLevel:" << node.isTopLevel;
            qDebug() << "  parents count:" << node.parents.size();
            qDebug() << "  children count:" << node.children.size();
            if (!node.parents.isEmpty()) {
                qDebug() << "  parents:" << node.parents.values();
            }
        } else {
            qDebug() << "\nCell:" << name << "(NOT FOUND)";
        }
    }
    
    qDebug() << "\n=== Cell Parent-Child Summary ===";
    for (auto it = m_cellNodes.begin(); it != m_cellNodes.end(); ++it) {
        const CellNode& node = it.value();
        if (node.parents.size() > 0 || node.children.size() > 0) {
            qDebug() << node.name 
                     << "| parents:" << node.parents.size()
                     << "| children:" << node.children.size()
                     << "| isTopLevel:" << node.isTopLevel;
        }
    }
    qDebug() << "================================";
}

void CellListPanel::populateTree()
{
    m_treeWidget->clear();
    m_treeItems.clear();
    
    if (!m_data || m_cellNodes.isEmpty()) {
        auto* item = new QTreeWidgetItem(QStringList() << tr("(No data)") << "");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_treeWidget->addTopLevelItem(item);
        return;
    }
    
    // 跟踪已添加的 Cell（避免重复）
    QSet<QString> addedCells;
    
    // 添加顶层 Cell 及其子树
    for (const QString& topCell : m_topLevelCells) {
        addTreeItems(nullptr, topCell, addedCells, 0);
    }
    
    // 检查是否有未添加的孤立 Cell（循环引用或无引用）
    for (auto it = m_cellNodes.begin(); it != m_cellNodes.end(); ++it) {
        if (!addedCells.contains(it->name)) {
            // 作为新的顶层添加
            addTreeItems(nullptr, it->name, addedCells, 0);
        }
    }
    
    // 更新状态
    auto* statusLabel = findChild<QLabel*>("statusLabel");
    if (statusLabel) {
        statusLabel->setText(tr("%1 cells, %2 top-level")
            .arg(m_cellNodes.size())
            .arg(m_topLevelCells.size()));
    }
    
    // 展开第一层
    m_treeWidget->expandToDepth(0);
    
    // 选中第一个顶层 Cell
    if (m_treeWidget->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstItem = m_treeWidget->topLevelItem(0);
        QString cellName = firstItem->data(0, Qt::UserRole).toString();
        setCurrentCell(cellName);
    }
}

void CellListPanel::addTreeItems(QTreeWidgetItem* parentItem, const QString& cellName, QSet<QString>& addedCells, int depth)
{
    // 防止无限递归
    if (depth > 100) {
        qWarning() << "Cell hierarchy depth exceeds limit:" << cellName;
        return;
    }
    
    // 防止重复添加（处理循环引用）
    if (addedCells.contains(cellName)) {
        return;
    }
    
    if (!m_cellNodes.contains(cellName)) return;
    
    const CellNode& node = m_cellNodes[cellName];
    
    // 创建树形项目
    QStringList texts;
    texts << getCellDisplayName(cellName);
    
    qint64 objCount = 0;
    if (node.structure) {
        objCount = node.structure->boundaries.size() 
                 + node.structure->paths.size() 
                 + node.structure->texts.size();
    }
    texts << QString::number(objCount);
    
    QTreeWidgetItem* item = nullptr;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem, texts);
    } else {
        item = new QTreeWidgetItem(m_treeWidget, texts);
    }
    
    // 存储数据
    item->setData(0, Qt::UserRole, cellName);
    
    // 标记为已添加
    addedCells.insert(cellName);
    
    // 更新显示
    updateItemDisplay(item, cellName, node.structure, node.children.size());
    
    // 缓存项目
    m_treeItems[cellName] = item;
    
    // 递归添加子 Cell
    for (const QString& childName : node.children) {
        addTreeItems(item, childName, addedCells, depth + 1);
    }
}

void CellListPanel::updateItemDisplay(QTreeWidgetItem* item, const QString& name, const GDSStructurePtr& structure, int refCount)
{
    // 设置图标
    if (m_topLevelCells.contains(name)) {
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    } else if (refCount > 0) {
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    } else {
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }
    
    // 设置工具提示
    QString tooltip = QString("Cell: %1\n").arg(name);
    if (structure) {
        tooltip += QString("Boundaries: %1\n").arg(structure->boundaries.size());
        tooltip += QString("Paths: %1\n").arg(structure->paths.size());
        tooltip += QString("Texts: %1\n").arg(structure->texts.size());
        tooltip += QString("References: %1\n").arg(structure->references.size());
        tooltip += QString("Arrays: %1\n").arg(structure->arrays.size());
        
        if (structure->boundingBox.isValid()) {
            double width = (structure->boundingBox.right - structure->boundingBox.left) / 1000.0;
            double height = (structure->boundingBox.bottom - structure->boundingBox.top) / 1000.0;
            tooltip += QString("Size: %1 x %2 μm").arg(width, 0, 'f', 2).arg(height, 0, 'f', 2);
        }
    }
    
    // 添加层级信息
    if (m_topLevelCells.contains(name)) {
        tooltip += "\n(Top-level cell)";
    }
    if (refCount > 0) {
        tooltip += QString("\nContains %1 child cells").arg(refCount);
    }
    
    item->setToolTip(0, tooltip);
    
    // 高亮顶层 Cell
    if (m_topLevelCells.contains(name)) {
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
    }
}

QString CellListPanel::getCellDisplayName(const QString& name) const
{
    return name;
}

//=============================================================================
// 槽函数
//=============================================================================

void CellListPanel::onItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) return;
    
    QString cellName = item->data(0, Qt::UserRole).toString();
    if (cellName.isEmpty()) return;
    
    m_currentCell = cellName;
    emit cellSelected(cellName);
}

void CellListPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) return;
    
    item->setExpanded(!item->isExpanded());
    
    QString cellName = item->data(0, Qt::UserRole).toString();
    if (!cellName.isEmpty()) {
        onItemClicked(item, 0);
    }
}

void CellListPanel::onFilterChanged(const QString& text)
{
    QString filter = text.toLower();
    
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        QTreeWidgetItem* item = *it;
        QString name = item->data(0, Qt::UserRole).toString().toLower();
        
        bool visible = filter.isEmpty() || name.contains(filter);
        item->setHidden(!visible);
        
        ++it;
    }
}

} // namespace QLayoutEDA