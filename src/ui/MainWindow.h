/**
 * @file MainWindow.h
 * @brief 主窗口
 * @author QLayoutEDA Team
 */

#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QActionGroup>
#include <memory>
#include "core/Types.h"

namespace QLayoutEDA {

class CanvasWidget;
class LayerPanel;
class PropertyPanel;
class CellListPanel;
class GDSParser;

/**
 * @brief 主窗口类
 * 
 * 负责管理整体界面布局、菜单、工具栏和面板
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // 文件操作
    void onFileNew();
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();
    void onFileExportGDS();
    void onFileImportGDS();
    
    // 编辑操作
    void onEditUndo();
    void onEditRedo();
    void onEditCut();
    void onEditCopy();
    void onEditPaste();
    void onEditDelete();
    void onEditSelectAll();
    
    // 视图操作
    void onViewZoomIn();
    void onViewZoomOut();
    void onViewFitAll();
    void onViewZoomActual();
    
    // 绘制工具
    void onToolSelect();
    void onToolPan();
    void onToolDrawRectangle();
    void onToolDrawPolygon();
    void onToolDrawPath();
    void onToolDrawCircle();
    
    // 帮助
    void onHelpAbout();

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setupPanels();
    void setupConnections();
    void updateStatusBar();
    void updateLayersFromGDS();
    
    // UI 组件
    CanvasWidget* m_canvas = nullptr;
    LayerPanel* m_layerPanel = nullptr;
    PropertyPanel* m_propertyPanel = nullptr;
    CellListPanel* m_cellListPanel = nullptr;
    
    // 状态栏标签
    QLabel* m_statusLabel = nullptr;
    QLabel* m_coordLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QLabel* m_unitLabel = nullptr;
    
    // 工具栏动作组
    QActionGroup* m_toolActionGroup = nullptr;
    
    // 当前工具
    enum class Tool {
        Select,
        Pan,
        DrawRectangle,
        DrawPolygon,
        DrawPath,
        DrawCircle
    };
    Tool m_currentTool = Tool::Select;
    
    // GDS 解析器
    std::unique_ptr<GDSParser> m_parser;
    GDSDataPtr m_gdsData;
};

} // namespace QLayoutEDA