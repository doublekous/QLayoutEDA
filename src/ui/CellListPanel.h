/**
 * @file CellListPanel.h
 * @brief Cell 列表面板（树形结构）
 * @author QLayoutEDA Team
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QHash>
#include <QSet>
#include <memory>
#include "core/Types.h"

namespace QLayoutEDA {

/**
 * @brief Cell 树形节点数据
 */
struct CellNode {
    QString name;                   ///< Cell 名称
    GDSStructurePtr structure;      ///< 结构数据
    QSet<QString> children;         ///< 子 Cell 名称集合
    QSet<QString> parents;          ///< 父 Cell 名称集合
    bool isTopLevel = false;        ///< 是否为顶层 Cell
};

/**
 * @brief Cell 列表面板
 * 
 * 显示 GDS 文件中的 Cell 层级结构（树形）
 */
class CellListPanel : public QWidget {
    Q_OBJECT

public:
    explicit CellListPanel(QWidget* parent = nullptr);
    ~CellListPanel() override;
    
    /**
     * @brief 设置 GDS 数据
     */
    void setGDSData(GDSDataPtr data);
    
    /**
     * @brief 设置当前选中的 Cell
     */
    void setCurrentCell(const QString& cellName);
    
    /**
     * @brief 获取当前选中的 Cell
     */
    QString currentCell() const;
    
    /**
     * @brief 刷新列表
     */
    void refreshList();
    
    /**
     * @brief 展开所有节点
     */
    void expandAll();
    
    /**
     * @brief 折叠所有节点
     */
    void collapseAll();

signals:
    /**
     * @brief Cell 选中信号
     */
    void cellSelected(const QString& cellName);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const QString& text);

private:
    void setupUi();
    void populateTree();
    void buildCellHierarchy();
    void addTreeItems(QTreeWidgetItem* parentItem, const QString& cellName, QSet<QString>& addedCells, int depth = 0);
    void updateItemDisplay(QTreeWidgetItem* item, const QString& name, const GDSStructurePtr& structure, int refCount = 0);
    QString getCellDisplayName(const QString& name) const;

private:
    QTreeWidget* m_treeWidget = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    GDSDataPtr m_data;
    QString m_currentCell;
    
    // Cell 层级信息
    QHash<QString, CellNode> m_cellNodes;
    QSet<QString> m_topLevelCells;
    
    // 树形项目缓存
    QHash<QString, QTreeWidgetItem*> m_treeItems;
};

} // namespace QLayoutEDA