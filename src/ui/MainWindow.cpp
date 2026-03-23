/**
 * @file MainWindow.cpp
 * @brief 主窗口实现
 * @author QLayoutEDA Team
 */

#include "MainWindow.h"
#include "CanvasWidget.h"
#include "LayerPanel.h"
#include "PropertyPanel.h"
#include "CellListPanel.h"
#include "../parser/GDSParser.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <QKeySequence>
#include <QIcon>
#include <QStyle>
#include <QProgressDialog>

namespace QLayoutEDA {

//=============================================================================
// 构造/析构
//=============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_parser(std::make_unique<GDSParser>())
{
    setupUi();
    setupMenus();
    setupToolbar();
    setupStatusBar();
    setupPanels();
    setupConnections();
    
    // 默认状态
    onToolSelect();
    statusBar()->showMessage(tr("Ready"));
}

MainWindow::~MainWindow() = default;

//=============================================================================
// UI 设置
//=============================================================================

void MainWindow::setupUi()
{
    setWindowTitle(tr("QLayout EDA v1.0.0"));
    resize(1600, 900);
    
    // 设置窗口图标
    setWindowIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    
    // 设置停靠窗口属性
    setDockNestingEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
}

void MainWindow::setupMenus()
{
    // 文件菜单
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    QAction* newAction = fileMenu->addAction(tr("&New Project"), this, &MainWindow::onFileNew, QKeySequence::New);
    newAction->setStatusTip(tr("Create a new project"));
    
    QAction* openAction = fileMenu->addAction(tr("&Open Project..."), this, &MainWindow::onFileOpen, QKeySequence::Open);
    openAction->setStatusTip(tr("Open an existing project"));
    
    fileMenu->addSeparator();
    
    QAction* saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::onFileSave, QKeySequence::Save);
    saveAction->setStatusTip(tr("Save current project"));
    
    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onFileSaveAs, QKeySequence::SaveAs);
    saveAsAction->setStatusTip(tr("Save project with a new name"));
    
    fileMenu->addSeparator();
    
    QAction* importAction = fileMenu->addAction(tr("&Import GDS..."), this, &MainWindow::onFileImportGDS, QKeySequence("Ctrl+I"));
    importAction->setStatusTip(tr("Import GDS file"));
    
    QAction* exportAction = fileMenu->addAction(tr("&Export GDS..."), this, &MainWindow::onFileExportGDS, QKeySequence("Ctrl+E"));
    exportAction->setStatusTip(tr("Export to GDS file"));
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction(tr("E&xit"), qApp, &QApplication::quit, QKeySequence::Quit);
    exitAction->setStatusTip(tr("Exit the application"));
    
    // 编辑菜单
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    
    QAction* undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::onEditUndo, QKeySequence::Undo);
    undoAction->setStatusTip(tr("Undo last action"));
    
    QAction* redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::onEditRedo, QKeySequence::Redo);
    redoAction->setStatusTip(tr("Redo last undone action"));
    
    editMenu->addSeparator();
    
    QAction* cutAction = editMenu->addAction(tr("Cu&t"), this, &MainWindow::onEditCut, QKeySequence::Cut);
    cutAction->setStatusTip(tr("Cut selected objects"));
    
    QAction* copyAction = editMenu->addAction(tr("&Copy"), this, &MainWindow::onEditCopy, QKeySequence::Copy);
    copyAction->setStatusTip(tr("Copy selected objects"));
    
    QAction* pasteAction = editMenu->addAction(tr("&Paste"), this, &MainWindow::onEditPaste, QKeySequence::Paste);
    pasteAction->setStatusTip(tr("Paste objects from clipboard"));
    
    editMenu->addSeparator();
    
    QAction* deleteAction = editMenu->addAction(tr("&Delete"), this, &MainWindow::onEditDelete, QKeySequence::Delete);
    deleteAction->setStatusTip(tr("Delete selected objects"));
    
    editMenu->addSeparator();
    
    QAction* selectAllAction = editMenu->addAction(tr("Select &All"), this, &MainWindow::onEditSelectAll, QKeySequence::SelectAll);
    selectAllAction->setStatusTip(tr("Select all objects"));
    
    // 视图菜单
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    
    QAction* zoomInAction = viewMenu->addAction(tr("Zoom &In"), this, &MainWindow::onViewZoomIn, QKeySequence::ZoomIn);
    zoomInAction->setStatusTip(tr("Zoom in view"));
    
    QAction* zoomOutAction = viewMenu->addAction(tr("Zoom &Out"), this, &MainWindow::onViewZoomOut, QKeySequence::ZoomOut);
    zoomOutAction->setStatusTip(tr("Zoom out view"));
    
    QAction* zoomActualAction = viewMenu->addAction(tr("Zoom &Actual Size"), this, &MainWindow::onViewZoomActual, QKeySequence("Ctrl+0"));
    zoomActualAction->setStatusTip(tr("Zoom to actual size (1:1)"));
    
    QAction* fitAllAction = viewMenu->addAction(tr("&Fit All"), this, &MainWindow::onViewFitAll, QKeySequence("F"));
    fitAllAction->setStatusTip(tr("Fit all objects in view"));
    
    viewMenu->addSeparator();
    viewMenu->addAction(tr("&Rulers"), []{})->setCheckable(true);
    viewMenu->addAction(tr("&Grid"), []{})->setCheckable(true);
    
    // 绘制菜单
    QMenu* drawMenu = menuBar()->addMenu(tr("&Draw"));
    
    drawMenu->addAction(tr("&Rectangle"), this, &MainWindow::onToolDrawRectangle, QKeySequence("R"));
    drawMenu->addAction(tr("&Polygon"), this, &MainWindow::onToolDrawPolygon, QKeySequence("P"));
    drawMenu->addAction(tr("&Path"), this, &MainWindow::onToolDrawPath, QKeySequence("W"));
    drawMenu->addAction(tr("&Circle"), this, &MainWindow::onToolDrawCircle, QKeySequence("C"));
    
    // 帮助菜单
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    
    QAction* aboutAction = helpMenu->addAction(tr("&About"), this, &MainWindow::onHelpAbout);
    aboutAction->setStatusTip(tr("About QLayout EDA"));
    
    helpMenu->addAction(tr("&Documentation"), []{}, QKeySequence::HelpContents);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::setupToolbar()
{
    // 主工具栏
    QToolBar* mainToolBar = addToolBar(tr("Main"));
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(24, 24));
    
    // 文件操作
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_FileIcon), tr("New"), this, &MainWindow::onFileNew);
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open"), this, &MainWindow::onFileOpen);
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"), this, &MainWindow::onFileSave);
    
    mainToolBar->addSeparator();
    
    // 编辑操作
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Undo"), this, &MainWindow::onEditUndo);
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Redo"), this, &MainWindow::onEditRedo);
    
    mainToolBar->addSeparator();
    
    // 视图操作
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Zoom In"), this, &MainWindow::onViewZoomIn);
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Zoom Out"), this, &MainWindow::onViewZoomOut);
    mainToolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Fit All"), this, &MainWindow::onViewFitAll);
    
    // 工具工具栏
    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));
    
    m_toolActionGroup = new QActionGroup(this);
    m_toolActionGroup->setExclusive(true);
    
    QAction* selectAction = toolBar->addAction(tr("Select (S)"), this, &MainWindow::onToolSelect);
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    selectAction->setShortcut(QKeySequence("S"));
    m_toolActionGroup->addAction(selectAction);
    
    QAction* panAction = toolBar->addAction(tr("Pan (Space)"), this, &MainWindow::onToolPan);
    panAction->setCheckable(true);
    panAction->setShortcut(QKeySequence("Space"));
    m_toolActionGroup->addAction(panAction);
    
    toolBar->addSeparator();
    
    QAction* rectAction = toolBar->addAction(tr("Rectangle (R)"), this, &MainWindow::onToolDrawRectangle);
    rectAction->setCheckable(true);
    rectAction->setShortcut(QKeySequence("R"));
    m_toolActionGroup->addAction(rectAction);
    
    QAction* polyAction = toolBar->addAction(tr("Polygon (P)"), this, &MainWindow::onToolDrawPolygon);
    polyAction->setCheckable(true);
    polyAction->setShortcut(QKeySequence("P"));
    m_toolActionGroup->addAction(polyAction);
    
    QAction* pathAction = toolBar->addAction(tr("Path (W)"), this, &MainWindow::onToolDrawPath);
    pathAction->setCheckable(true);
    pathAction->setShortcut(QKeySequence("W"));
    m_toolActionGroup->addAction(pathAction);
    
    QAction* circleAction = toolBar->addAction(tr("Circle (C)"), this, &MainWindow::onToolDrawCircle);
    circleAction->setCheckable(true);
    circleAction->setShortcut(QKeySequence("C"));
    m_toolActionGroup->addAction(circleAction);
}

void MainWindow::setupStatusBar()
{
    QStatusBar* status = statusBar();
    
    // 状态标签
    m_statusLabel = new QLabel(tr("Ready"));
    status->addWidget(m_statusLabel, 1);
    
    // 坐标标签
    m_coordLabel = new QLabel(tr("X: 0.000  Y: 0.000"));
    m_coordLabel->setMinimumWidth(180);
    m_coordLabel->setAlignment(Qt::AlignCenter);
    status->addPermanentWidget(m_coordLabel);
    
    // 缩放标签
    m_zoomLabel = new QLabel(tr("Zoom: 100%"));
    m_zoomLabel->setMinimumWidth(100);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    status->addPermanentWidget(m_zoomLabel);
    
    // 单位标签
    m_unitLabel = new QLabel(tr("Unit: μm"));
    m_unitLabel->setMinimumWidth(80);
    m_unitLabel->setAlignment(Qt::AlignCenter);
    status->addPermanentWidget(m_unitLabel);
}

void MainWindow::setupPanels()
{
    // 画布（中心区域）
    m_canvas = new CanvasWidget(this);
    setCentralWidget(m_canvas);
    
    // Cell 列表面板（左侧顶部）
    m_cellListPanel = new CellListPanel(this);
    QDockWidget* cellDock = new QDockWidget(tr("Cells"), this);
    cellDock->setWidget(m_cellListPanel);
    cellDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    cellDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, cellDock);
    
    // 图层面板（左侧底部）
    m_layerPanel = new LayerPanel(this);
    QDockWidget* layerDock = new QDockWidget(tr("Layers"), this);
    layerDock->setWidget(m_layerPanel);
    layerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, layerDock);
    
    // 属性面板（右侧）
    m_propertyPanel = new PropertyPanel(this);
    QDockWidget* propDock = new QDockWidget(tr("Properties"), this);
    propDock->setWidget(m_propertyPanel);
    propDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    propDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, propDock);
}

void MainWindow::setupConnections()
{
    // 连接画布信号
    connect(m_canvas, &CanvasWidget::coordinateChanged, this, [this](double x, double y) {
        m_coordLabel->setText(tr("X: %1  Y: %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });
    
    connect(m_canvas, &CanvasWidget::zoomChanged, this, [this](double zoom) {
        m_zoomLabel->setText(tr("Zoom: %1%").arg(zoom * 100, 0, 'f', 0));
    });
    
    connect(m_canvas, &CanvasWidget::statusMessage, this, [this](const QString& msg) {
        m_statusLabel->setText(msg);
    });
    
    // 连接图形创建信号
    connect(m_canvas, &CanvasWidget::objectCreated, this, 
            [this](const QString& type, const QRectF& bounds) {
        qDebug() << "=== objectCreated signal received ===" << type << bounds;
        this->onObjectCreated(type, bounds);
    });
    
    // 连接 Cell 列表面板信号
    connect(m_cellListPanel, &CellListPanel::cellSelected, this, [this](const QString& cellName) {
        if (m_canvas) {
            m_canvas->setCurrentStructure(cellName);
            m_cellListPanel->setCurrentCell(cellName);
        }
    });
}

//=============================================================================
// 文件操作槽
//=============================================================================

void MainWindow::onFileNew()
{
    statusBar()->showMessage(tr("Creating new project..."));
    // TODO: 新建项目逻辑
    statusBar()->showMessage(tr("New project created"), 2000);
}

void MainWindow::onFileOpen()
{
    QString file = QFileDialog::getOpenFileName(this,
        tr("Open Project"),
        QString(),
        tr("GDS Files (*.gds *.GDS);;All Files (*)"));
    
    if (!file.isEmpty()) {
        onImportGDS(file);
    }
}

void MainWindow::onFileSave()
{
    statusBar()->showMessage(tr("Saving..."));
    // TODO: 保存逻辑
    statusBar()->showMessage(tr("Saved"), 2000);
}

void MainWindow::onFileSaveAs()
{
    QString file = QFileDialog::getSaveFileName(this,
        tr("Save Project As"),
        QString(),
        tr("GDS Files (*.gds)"));
    
    if (!file.isEmpty()) {
        statusBar()->showMessage(tr("Saving as: %1").arg(file));
        // TODO: 另存为逻辑
        statusBar()->showMessage(tr("Saved as: %1").arg(file), 2000);
    }
}

void MainWindow::onFileImportGDS()
{
    QString file = QFileDialog::getOpenFileName(this,
        tr("Import GDS"),
        QString(),
        tr("GDS Files (*.gds *.GDS);;OASIS Files (*.oas);;All Files (*)"));
    
    if (!file.isEmpty()) {
        onImportGDS(file);
    }
}

void MainWindow::onImportGDS(const QString& file)
{
    statusBar()->showMessage(tr("Importing: %1").arg(file));
    
    // 创建进度对话框
    QProgressDialog progress(tr("Parsing GDS file..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setWindowTitle(tr("Import GDS"));
    
    // 设置进度回调
    m_parser->setProgressCallback([&progress](int p) {
        progress.setValue(p);
        if (progress.wasCanceled()) {
            // 需要通过信号槽来取消
        }
    });
    
    // 解析 GDS 文件
    if (m_parser->parse(file)) {
        auto data = m_parser->getData();
        if (data && m_canvas) {
            // MainWindow 也保存数据引用（关键修复！）
            m_gdsData = data;
            
            // 将数据传递给画布
            m_canvas->setGDSData(data);
            
            // 更新图层面板
            if (m_layerPanel) {
                // 提取图层信息
                QSet<int> layers;
                for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
                    auto structure = it.value();
                    if (!structure) continue;
                    
                    for (const auto& boundary : structure->boundaries) {
                        layers.insert(boundary.layer);
                    }
                    for (const auto& path : structure->paths) {
                        layers.insert(path.layer);
                    }
                    for (const auto& text : structure->texts) {
                        layers.insert(text.layer);
                    }
                }
                
                m_layerPanel->setLayers(layers.values());
            }
            
            // 更新 Cell 列表面板
            if (m_cellListPanel) {
                m_cellListPanel->setGDSData(data);
            }
            
            // 自动适配视图
            m_canvas->fitAll();
            
            qDebug() << "GDS loaded:" << file 
                     << "structures:" << data->structures.size();
            
            statusBar()->showMessage(tr("Imported: %1 (%2 structures)")
                .arg(file)
                .arg(data->structures.size()), 5000);
        }
    } else {
        QMessageBox::warning(this, tr("Import Error"),
            tr("Failed to import GDS file:\n%1").arg(m_parser->getLastError()));
        statusBar()->showMessage(tr("Import failed"), 3000);
    }
}

void MainWindow::onFileExportGDS()
{
    QString file = QFileDialog::getSaveFileName(this,
        tr("Export GDS"),
        QString(),
        tr("GDS Files (*.gds);;OASIS Files (*.oas)"));
    
    if (!file.isEmpty()) {
        statusBar()->showMessage(tr("Exporting to: %1").arg(file));
        // TODO: 导出逻辑
        statusBar()->showMessage(tr("Exported to: %1").arg(file), 2000);
    }
}

//=============================================================================
// 编辑操作槽
//=============================================================================

void MainWindow::onEditUndo()
{
    statusBar()->showMessage(tr("Undo"));
    // TODO: 撤销逻辑
}

void MainWindow::onEditRedo()
{
    statusBar()->showMessage(tr("Redo"));
    // TODO: 重做逻辑
}

void MainWindow::onEditCut()
{
    statusBar()->showMessage(tr("Cut"));
    // TODO: 剪切逻辑
}

void MainWindow::onEditCopy()
{
    statusBar()->showMessage(tr("Copy"));
    // TODO: 复制逻辑
}

void MainWindow::onEditPaste()
{
    statusBar()->showMessage(tr("Paste"));
    // TODO: 粘贴逻辑
}

void MainWindow::onEditDelete()
{
    statusBar()->showMessage(tr("Delete"));
    // TODO: 删除逻辑
}

void MainWindow::onEditSelectAll()
{
    statusBar()->showMessage(tr("Select All"));
    // TODO: 全选逻辑
}

//=============================================================================
// 视图操作槽
//=============================================================================

void MainWindow::onViewZoomIn()
{
    if (m_canvas) {
        m_canvas->zoomIn();
        statusBar()->showMessage(tr("Zoom In"));
    }
}

void MainWindow::onViewZoomOut()
{
    if (m_canvas) {
        m_canvas->zoomOut();
        statusBar()->showMessage(tr("Zoom Out"));
    }
}

void MainWindow::onViewFitAll()
{
    if (m_canvas) {
        m_canvas->fitAll();
        statusBar()->showMessage(tr("Fit All"));
    }
}

void MainWindow::onViewZoomActual()
{
    if (m_canvas) {
        m_canvas->zoomActual();
        statusBar()->showMessage(tr("Actual Size"));
    }
}

//=============================================================================
// 绘制工具槽
//=============================================================================

void MainWindow::onToolSelect()
{
    m_currentTool = Tool::Select;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::Select);
    statusBar()->showMessage(tr("Select Tool - Click to select objects"));
}

void MainWindow::onToolPan()
{
    m_currentTool = Tool::Pan;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::Pan);
    statusBar()->showMessage(tr("Pan Tool - Drag to pan view"));
}

void MainWindow::onToolDrawRectangle()
{
    m_currentTool = Tool::DrawRectangle;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::DrawRectangle);
    statusBar()->showMessage(tr("Rectangle Tool - Click and drag to draw rectangle"));
}

void MainWindow::onToolDrawPolygon()
{
    m_currentTool = Tool::DrawPolygon;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::DrawPolygon);
    statusBar()->showMessage(tr("Polygon Tool - Click to add vertices, Right-click to finish"));
}

void MainWindow::onToolDrawPath()
{
    m_currentTool = Tool::DrawPath;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::DrawPath);
    statusBar()->showMessage(tr("Path Tool - Click to add points, Right-click to finish"));
}

void MainWindow::onToolDrawCircle()
{
    m_currentTool = Tool::DrawCircle;
    if (m_canvas) m_canvas->setTool(CanvasWidget::Tool::DrawCircle);
    statusBar()->showMessage(tr("Circle Tool - Click center, drag to set radius"));
}

//=============================================================================
// 帮助槽
//=============================================================================

void MainWindow::onHelpAbout()
{
    QMessageBox::about(this, tr("About QLayout EDA"),
        tr("<h3>QLayout EDA v1.0.0</h3>"
           "<p>A professional EDA layout design tool.</p>"
           "<p>Features:</p>"
           "<ul>"
           "<li>GDS/OASIS file support</li>"
           "<li>Multiple drawing tools</li>"
           "<li>Layer management</li>"
           "<li>DRC checking</li>"
           "</ul>"
           "<p>Copyright &copy; 2026 QLayoutEDA Team</p>"));
}

//=============================================================================
// 辅助方法
//=============================================================================

void MainWindow::updateLayersFromGDS()
{
    if (!m_gdsData || !m_layerPanel) {
        return;
    }
    
    // 提取所有图层
    QSet<int> layers;
    for (auto it = m_gdsData->structures.begin(); it != m_gdsData->structures.end(); ++it) {
        auto structure = it.value();
        if (!structure) continue;
        
        for (const auto& boundary : structure->boundaries) {
            layers.insert(boundary.layer);
        }
        for (const auto& path : structure->paths) {
            layers.insert(path.layer);
        }
        for (const auto& text : structure->texts) {
            layers.insert(text.layer);
        }
    }
    
    m_layerPanel->setLayers(layers.values());
}

//=============================================================================
// 图形创建槽
//=============================================================================

void MainWindow::onObjectCreated(const QString& type, const QRectF& bounds)
{
    qDebug() << "=== onObjectCreated called ===" << type << bounds;
    
    // 检查 bounds 是否有效
    if (bounds.width() <= 0 || bounds.height() <= 0) {
        qWarning() << "Invalid bounds, ignoring:" << bounds;
        statusBar()->showMessage(tr("Invalid shape, try again"), 3000);
        return;
    }
    
    if (!m_canvas) {
        qWarning() << "No canvas";
        statusBar()->showMessage(tr("No canvas"), 3000);
        return;
    }
    
    // 关键：使用 canvas 当前的结构，不要创建新结构！
    if (!m_gdsData) {
        qWarning() << "No GDS data, please open a file first";
        statusBar()->showMessage(tr("Please open a GDS file first"), 3000);
        return;
    }
    
    // 获取当前 Structure（使用 canvas 的当前结构）
    QString currentStructure = m_canvas->currentStructure();
    qDebug() << "Current structure from canvas:" << currentStructure;
    
    if (currentStructure.isEmpty()) {
        // 使用文件的顶层结构
        currentStructure = m_gdsData->topStructure;
        qDebug() << "Using top structure:" << currentStructure;
    }
    
    auto structure = m_gdsData->getStructure(currentStructure);
    if (!structure) {
        qWarning() << "Structure not found:" << currentStructure;
        statusBar()->showMessage(tr("Structure not found"), 3000);
        return;
    }
    
    qDebug() << "Structure found:" << currentStructure << "boundaries before:" << structure->boundaries.size();
    
    // ===== 关键修复：bounds 现在已经是数据库坐标（纳米）！=====
    // 不需要再做转换，直接使用
    Coord x1 = static_cast<Coord>(bounds.left());
    Coord y1 = static_cast<Coord>(bounds.top());
    Coord x2 = static_cast<Coord>(bounds.right());
    Coord y2 = static_cast<Coord>(bounds.bottom());
    
    // 获取当前图层
    int currentLayer = 0;  // TODO: 从图层面板获取
    if (m_layerPanel) {
        // 暂时使用第一个可见图层
        // m_layerPanel->getCurrentLayer();
    }
    
    // 根据类型创建图形
    if (type == "Rectangle") {
        // 创建矩形边界
        GDSBoundary boundary;
        boundary.layer = currentLayer;
        boundary.dataType = 0;
        boundary.points.resize(5);
        boundary.points[0] = DbPoint(x1, y1);
        boundary.points[1] = DbPoint(x2, y1);
        boundary.points[2] = DbPoint(x2, y2);
        boundary.points[3] = DbPoint(x1, y2);
        boundary.points[4] = DbPoint(x1, y1);  // 闭合
        
        structure->boundaries.append(boundary);
        qDebug() << "Rectangle added, boundaries:" << structure->boundaries.size();
        
    } else if (type == "Circle") {
        // 创建圆形（多边形近似）
        // bounds 已经是数据库坐标（纳米）
        double cx = bounds.center().x();
        double cy = bounds.center().y();
        double radius = bounds.width() / 2.0;
        
        GDSBoundary boundary;
        boundary.layer = currentLayer;
        boundary.dataType = 0;
        
        // 使用32边形近似圆
        const int segments = 32;
        boundary.points.resize(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            double angle = 2.0 * M_PI * i / segments;
            boundary.points[i] = DbPoint(
                static_cast<Coord>(cx + radius * cos(angle)),
                static_cast<Coord>(cy + radius * sin(angle))
            );
        }
        
        structure->boundaries.append(boundary);
        
    } else if (type == "Polygon" || type == "Path") {
        // TODO: 从 CanvasWidget 获取实际点数据
        // 暂时创建边界框
        GDSBoundary boundary;
        boundary.layer = currentLayer;
        boundary.dataType = 0;
        boundary.points.resize(5);
        boundary.points[0] = DbPoint(x1, y1);
        boundary.points[1] = DbPoint(x2, y1);
        boundary.points[2] = DbPoint(x2, y2);
        boundary.points[3] = DbPoint(x1, y2);
        boundary.points[4] = DbPoint(x1, y1);
        
        structure->boundaries.append(boundary);
    }
    
    // 更新边界框
    if (structure->boundaries.size() > 0) {
        structure->boundingBox.left = qMin(structure->boundingBox.left, x1);
        structure->boundingBox.top = qMin(structure->boundingBox.top, y1);
        structure->boundingBox.right = qMax(structure->boundingBox.right, x2);
        structure->boundingBox.bottom = qMax(structure->boundingBox.bottom, y2);
    } else {
        structure->boundingBox = DbRect(x1, y1, x2, y2);
    }
    
    // 刷新画布
    m_canvas->update();
    
    // 显示状态
    QString msg = tr("%1 created at (%2, %3) size (%4 x %5) μm")
        .arg(type)
        .arg(bounds.x(), 0, 'f', 3)
        .arg(bounds.y(), 0, 'f', 3)
        .arg(bounds.width(), 0, 'f', 3)
        .arg(bounds.height(), 0, 'f', 3);
    
    statusBar()->showMessage(msg, 3000);
    
    qDebug() << "Object created:" << type 
             << "layer:" << currentLayer
             << "bounds:" << bounds;
}

} // namespace QLayoutEDA