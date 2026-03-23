/**
 * @file CanvasWidget.cpp
 * @brief 画布控件实现
 * @author QLayoutEDA Team
 */

#include "CanvasWidget.h"
#include "../graphic/RenderEngine.h"
#include "../graphic/GeometryAlgorithms.h"
#include "../graphic/QuadTree.h"
#include "../graphic/CellRenderCache.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QtMath>
#include <QDebug>
#include <QElapsedTimer>

namespace QLayoutEDA {

//=============================================================================
// 辅助函数
//=============================================================================

uint qHash(const SelectedObject& obj, uint seed)
{
    uint h = qHash(obj.structureName, seed);
    h ^= ::qHash(obj.objectType, seed);
    h ^= ::qHash(obj.index, seed);
    return h;
}

//=============================================================================
// 常量定义
//=============================================================================

namespace {
    constexpr double ZOOM_FACTOR = 1.2;         // 缩放因子
    constexpr double MIN_ZOOM = 1e-6;           // 最小缩放
    constexpr double MAX_ZOOM = 1e6;            // 最大缩放
    constexpr int GRID_MAJOR_SPACING = 100;     // 主网格间距
    constexpr int GRID_MINOR_SPACING = 10;      // 次网格间距
    constexpr double SELECTION_TOLERANCE = 5.0; // 选择容差（像素）
    constexpr int MAX_RECURSION_DEPTH = 20;     // 最大递归深度
    
    // ========== 关键修复：参考 KLayout，无 SREF 展开数量限制 ==========
    // KLayout 策略：遍历所有实例，通过视口裁剪 + Cell Cache 优化
    constexpr double SREF_EXPAND_THRESHOLD = 1.0;  // fitAll 即展开
    constexpr int MAX_SREF_BOUNDING_BOX = 1000000;  // 边界框上限提高到 100 万
    constexpr int MAX_SREF_EXPAND_COUNT = 10000000; // 展开上限提高到 1000 万（实际无限制）
}

//=============================================================================
// 构造/析构
//=============================================================================

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    
    // 设置背景
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, m_renderConfig.backgroundColor);
    setPalette(pal);
    
    // 更新定时器（性能优化）
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(16); // ~60fps
    connect(m_updateTimer, &QTimer::timeout, this, QOverload<>::of(&CanvasWidget::update));
    
    // 初始化视图变换
    m_transform.scale = 1.0;
    m_transform.offsetX = 0;
    m_transform.offsetY = 0;
    
    // ========== 初始化 Cell 渲染缓存（参考 KLayout）==========
    m_cellRenderCache = std::make_unique<CellRenderCache>(100, 200 * 1024 * 1024);
    
    updateCursor();
}

CanvasWidget::~CanvasWidget() = default;

//=============================================================================
// 工具设置
//=============================================================================

void CanvasWidget::setTool(Tool tool)
{
    if (m_currentTool != tool) {
        // 取消当前绘制
        if (m_isDrawing) {
            m_isDrawing = false;
            m_drawPoints.clear();
        }
        m_currentTool = tool;
        updateCursor();
    }
}

void CanvasWidget::setGDSData(GDSDataPtr data)
{
    m_gdsData = data;
    if (m_renderEngine && data) {
        m_renderEngine->setData(data);
    }
    
    // 设置当前结构
    if (data && !data->structures.isEmpty()) {
        if (!data->topStructure.isEmpty()) {
            m_currentStructure = data->topStructure;
        } else {
            m_currentStructure = data->structures.keys().first();
        }
        
        // ========== 构建四叉树空间索引 ==========
        QElapsedTimer buildTimer;
        buildTimer.start();
        
        auto structure = m_gdsData->getStructure(m_currentStructure);
        if (structure) {
            m_quadTree = std::make_unique<QuadTree>();
            m_quadTree->build(structure->boundaries, structure->paths, structure->texts);
            m_quadTreeDirty = false;
            
            qint64 buildTime = buildTimer.elapsed();
            auto stats = m_quadTree->getStats();
            qDebug() << "=== 四叉树构建完成 ===";
            qDebug() << "节点数:" << stats.totalNodes;
            qDebug() << "图形数:" << stats.totalShapes;
            qDebug() << "最大深度:" << stats.maxDepth;
            qDebug() << "构建耗时:" << buildTime << "ms";
        }
    } else {
        m_currentStructure.clear();
        m_quadTree.reset();
    }
    
    // 清除选择
    m_selectedObjects.clear();
    
    update();
}

void CanvasWidget::setCurrentStructure(const QString& structureName)
{
    if (m_currentStructure == structureName) {
        return;
    }
    
    if (!m_gdsData || !m_gdsData->structures.contains(structureName)) {
        return;
    }
    
    m_currentStructure = structureName;
    m_selectedObjects.clear();
    
    // 重置绘制状态
    m_isDrawing = false;
    m_drawPoints.clear();
    m_drawCircleRadius = 0;
    m_drawRect = QRectF();
    
    // 清除图像缓存
    m_imageCache.clear();
    
    // 自动适配视图
    fitAll();
    
    emit statusMessage(tr("Switched to cell: %1").arg(structureName));
}

//=============================================================================
// 视图操作
//=============================================================================

void CanvasWidget::zoomIn()
{
    setZoom(m_transform.scale * ZOOM_FACTOR);
}

void CanvasWidget::zoomOut()
{
    setZoom(m_transform.scale / ZOOM_FACTOR);
}

void CanvasWidget::zoomActual()
{
    setZoom(1.0);
}

void CanvasWidget::fitAll()
{
    if (m_gdsData && !m_gdsData->structures.isEmpty()) {
        // 获取当前结构
        QString structName = m_currentStructure;
        if (structName.isEmpty()) {
            structName = m_gdsData->topStructure;
        }
        if (structName.isEmpty() && !m_gdsData->structures.isEmpty()) {
            structName = m_gdsData->structures.keys().first();
        }
        
        auto structure = m_gdsData->getStructure(structName);
        if (!structure) {
            return;
        }
        
        // 使用 test_render.cpp 的边界计算逻辑
        qint64 minX = LLONG_MAX, maxX = LLONG_MIN, minY = LLONG_MAX, maxY = LLONG_MIN;
        bool hasValidBounds = false;
        
        for (const auto& b : structure->boundaries) {
            for (const auto& p : b.points) {
                minX = qMin(minX, p.x);
                maxX = qMax(maxX, p.x);
                minY = qMin(minY, p.y);
                maxY = qMax(maxY, p.y);
                hasValidBounds = true;
            }
        }
        
        for (const auto& path : structure->paths) {
            for (const auto& p : path.points) {
                minX = qMin(minX, p.x);
                maxX = qMax(maxX, p.x);
                minY = qMin(minY, p.y);
                maxY = qMax(maxY, p.y);
                hasValidBounds = true;
            }
        }
        
        if (hasValidBounds) {
            qint64 dbW = maxX - minX;
            qint64 dbH = maxY - minY;
            
            if (dbW > 0 && dbH > 0) {
                // 计算基础缩放（fitAll 时 scale = 1.0）
                double baseScale = qMin((width() - 100.0) / dbW, (height() - 100.0) / dbH);
                
                // 重置视图变换
                m_transform.scale = 1.0;
                m_transform.offsetX = 0;
                m_transform.offsetY = 0;
                
                // 记录 fitAll 的缓存键（标记为 precious）
                m_lastFitAllKey = ImageCacheKey(structName, m_transform.scale,
                                                m_transform.offsetX, m_transform.offsetY,
                                                width(), height());
                
                update();
                emit zoomChanged(baseScale);
                return;
            }
        }
    }
    
    // 默认视图
    m_transform.scale = 1.0;
    m_transform.offsetX = 0;
    m_transform.offsetY = 0;
    
    // 清除 fitAll 缓存键
    m_lastFitAllKey = ImageCacheKey();
    
    update();
    emit zoomChanged(1.0);
}

void CanvasWidget::setZoom(double scale)
{
    scale = qBound(MIN_ZOOM, scale, MAX_ZOOM);
    if (!qFuzzyCompare(m_transform.scale, scale)) {
        // 以画布中心为缩放中心
        QPointF center = screenToWorld(QPointF(width() / 2.0, height() / 2.0));
        m_transform.scale = scale;
        QPointF newCenter = screenToWorld(QPointF(width() / 2.0, height() / 2.0));
        m_transform.offsetX += (newCenter.x() - center.x()) * m_transform.scale;
        m_transform.offsetY += (newCenter.y() - center.y()) * m_transform.scale;
        
        update();
        emit zoomChanged(m_transform.scale);
    }
}

void CanvasWidget::setCurrentLayer(int layer)
{
    m_currentLayer = layer;
}

//=============================================================================
// 选择操作
//=============================================================================

void CanvasWidget::selectObject(const SelectedObject& obj)
{
    if (!m_selectedObjects.contains(obj)) {
        m_selectedObjects.insert(obj);
        emit selectionChanged(m_selectedObjects.size());
        update();
    }
}

void CanvasWidget::deselectObject(const SelectedObject& obj)
{
    if (m_selectedObjects.remove(obj)) {
        emit selectionChanged(m_selectedObjects.size());
        update();
    }
}

void CanvasWidget::selectAll()
{
    if (!m_gdsData) return;
    
    m_selectedObjects.clear();
    
    // 获取当前结构
    QString structName = m_currentStructure;
    if (structName.isEmpty() && !m_gdsData->structures.isEmpty()) {
        structName = m_gdsData->structures.keys().first();
    }
    
    auto structure = m_gdsData->getStructure(structName);
    if (!structure) return;
    
    // 选择所有边界
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 0; // boundary
        obj.index = i;
        m_selectedObjects.insert(obj);
    }
    
    // 选择所有路径
    for (int i = 0; i < structure->paths.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 1; // path
        obj.index = i;
        m_selectedObjects.insert(obj);
    }
    
    // 选择所有文本
    for (int i = 0; i < structure->texts.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 2; // text
        obj.index = i;
        m_selectedObjects.insert(obj);
    }
    
    emit selectionChanged(m_selectedObjects.size());
    emit statusMessage(tr("Selected %1 objects").arg(m_selectedObjects.size()));
    update();
}

void CanvasWidget::clearSelection()
{
    if (!m_selectedObjects.isEmpty()) {
        m_selectedObjects.clear();
        emit selectionChanged(0);
        emit statusMessage(tr("Selection cleared"));
        update();
    }
}

//=============================================================================
// 坐标转换
//=============================================================================

QPointF CanvasWidget::screenToWorld(const QPointF& screen) const
{
    return QPointF(
        (screen.x() - width() / 2.0) / m_transform.scale - m_transform.offsetX,
        (height() / 2.0 - screen.y()) / m_transform.scale - m_transform.offsetY
    );
}

QPointF CanvasWidget::worldToScreen(const QPointF& world) const
{
    return QPointF(
        (world.x() + m_transform.offsetX) * m_transform.scale + width() / 2.0,
        height() / 2.0 - (world.y() + m_transform.offsetY) * m_transform.scale
    );
}

QPointF CanvasWidget::screenToDatabase(const QPointF& screen) const
{
    // 调试输出
    qDebug() << "=== screenToDatabase ===" 
             << "screen:" << screen 
             << "m_gdsData:" << (m_gdsData ? "valid" : "null")
             << "m_currentStructure:" << m_currentStructure;
    
    // 使用与 drawObjects 相同的坐标系统
    if (!m_gdsData || m_currentStructure.isEmpty()) {
        // 无数据时：使用合理的默认值
        // 假设画布中心为原点，1 像素 ≈ 1μm（根据典型 EDA 设计）
        // 但实际上应该参考已加载的数据范围
        qDebug() << "No GDS data, using default conversion";
        return QPointF(screen.x() * 1000, -screen.y() * 1000);
    }
    
    auto structure = m_gdsData->getStructure(m_currentStructure);
    if (!structure) {
        qDebug() << "Structure not found:" << m_currentStructure;
        return QPointF(screen.x() * 1000, -screen.y() * 1000);
    }
    
    qDebug() << "Structure found, boundaries:" << structure->boundaries.size();
    
    // 计算边界框（与 drawObjects 相同）
    qint64 minX = LLONG_MAX, maxX = LLONG_MIN, minY = LLONG_MAX, maxY = LLONG_MIN;
    bool hasValidBounds = false;
    
    for (const auto& b : structure->boundaries) {
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x);
            maxX = qMax(maxX, p.x);
            minY = qMin(minY, p.y);
            maxY = qMax(maxY, p.y);
            hasValidBounds = true;
        }
    }
    
    if (!hasValidBounds) {
        return QPointF(screen.x() * 1000, -screen.y() * 1000);
    }
    
    qint64 dbW = maxX - minX;
    qint64 dbH = maxY - minY;
    
    if (dbW <= 0 || dbH <= 0) {
        return QPointF(screen.x() * 1000, -screen.y() * 1000);
    }
    
    // 使用与 drawObjects 相同的计算
    double baseScale = qMin((width() - 100.0) / dbW, (height() - 100.0) / dbH);
    double scale = baseScale * m_transform.scale;
    
    double offsetY = (height() + dbH * scale) / 2.0 + minY * scale;
    double offsetX = (width() - dbW * scale) / 2.0 - minX * scale;
    offsetX += m_transform.offsetX * scale;
    offsetY -= m_transform.offsetY * scale;
    
    // 逆变换：屏幕坐标 → 数据库坐标
    double dbX = (screen.x() - offsetX) / scale;
    double dbY = (offsetY - screen.y()) / scale;  // Y轴翻转
    
    return QPointF(dbX, dbY);
}

QPointF CanvasWidget::databaseToScreen(const QPointF& db) const
{
    if (!m_gdsData || m_currentStructure.isEmpty()) {
        return QPointF(db.x() / 1000, -db.y() / 1000);
    }
    
    auto structure = m_gdsData->getStructure(m_currentStructure);
    if (!structure) {
        return QPointF(db.x() / 1000, -db.y() / 1000);
    }
    
    // 计算边界框
    qint64 minX = LLONG_MAX, maxX = LLONG_MIN, minY = LLONG_MAX, maxY = LLONG_MIN;
    bool hasValidBounds = false;
    
    for (const auto& b : structure->boundaries) {
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x);
            maxX = qMax(maxX, p.x);
            minY = qMin(minY, p.y);
            maxY = qMax(maxY, p.y);
            hasValidBounds = true;
        }
    }
    
    if (!hasValidBounds) {
        return QPointF(db.x() / 1000, -db.y() / 1000);
    }
    
    qint64 dbW = maxX - minX;
    qint64 dbH = maxY - minY;
    
    if (dbW <= 0 || dbH <= 0) {
        return QPointF(db.x() / 1000, -db.y() / 1000);
    }
    
    // 使用与 drawObjects 相同的计算
    double baseScale = qMin((width() - 100.0) / dbW, (height() - 100.0) / dbH);
    double scale = baseScale * m_transform.scale;
    
    double offsetY = (height() + dbH * scale) / 2.0 + minY * scale;
    double offsetX = (width() - dbW * scale) / 2.0 - minX * scale;
    offsetX += m_transform.offsetX * scale;
    offsetY -= m_transform.offsetY * scale;
    
    // 正变换：数据库坐标 → 屏幕坐标
    double screenX = db.x() * scale + offsetX;
    double screenY = offsetY - db.y() * scale;  // Y轴翻转
    
    return QPointF(screenX, screenY);
}

//=============================================================================
// 绘制事件
//=============================================================================

void CanvasWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, m_renderConfig.antialiasing);
    
    // 绘制背景
    painter.fillRect(rect(), m_renderConfig.backgroundColor);
    
    // 绘制网格
    if (m_renderConfig.showGrid) {
        drawGrid(painter);
    }
    
    // 同步渲染
    drawObjects(painter);
    
    // 绘制选择
    drawSelection(painter);
    
    // 绘制橡皮筋
    if (m_rubberBandActive) {
        drawRubberBand(painter);
    }
    
    // 绘制预览
    if (m_isDrawing) {
        drawPreview(painter);
    }
    
    // 绘制标尺
    if (m_renderConfig.showRulers) {
        drawRuler(painter);
    }
}

void CanvasWidget::drawGrid(QPainter& painter)
{
    painter.save();
    
    // 计算可见范围
    QPointF topLeft = screenToWorld(QPointF(0, 0));
    QPointF bottomRight = screenToWorld(QPointF(width(), height()));
    
    // 根据缩放级别确定网格间距
    double gridSpacing = GRID_MAJOR_SPACING;
    if (m_transform.scale < 0.01) gridSpacing = GRID_MAJOR_SPACING * 100;
    else if (m_transform.scale < 0.1) gridSpacing = GRID_MAJOR_SPACING * 10;
    else if (m_transform.scale > 10) gridSpacing = GRID_MAJOR_SPACING / 10;
    
    // 绘制次网格（更细的线）
    painter.setPen(QPen(QColor(40, 40, 40), 0.5));
    double minorSpacing = gridSpacing / 10;
    
    double startX = qFloor(topLeft.x() / minorSpacing) * minorSpacing;
    double startY = qFloor(bottomRight.y() / minorSpacing) * minorSpacing;
    
    for (double x = startX; x <= bottomRight.x(); x += minorSpacing) {
        QPointF screen = worldToScreen(QPointF(x, 0));
        painter.drawLine(screen.x(), 0, screen.x(), height());
    }
    
    for (double y = startY; y <= topLeft.y(); y += minorSpacing) {
        QPointF screen = worldToScreen(QPointF(0, y));
        painter.drawLine(0, screen.y(), width(), screen.y());
    }
    
    // 绘制主网格
    painter.setPen(QPen(QColor(60, 60, 60), 1.0));
    
    startX = qFloor(topLeft.x() / gridSpacing) * gridSpacing;
    startY = qFloor(bottomRight.y() / gridSpacing) * gridSpacing;
    
    for (double x = startX; x <= bottomRight.x(); x += gridSpacing) {
        QPointF screen = worldToScreen(QPointF(x, 0));
        painter.drawLine(screen.x(), 0, screen.x(), height());
    }
    
    for (double y = startY; y <= topLeft.y(); y += gridSpacing) {
        QPointF screen = worldToScreen(QPointF(0, y));
        painter.drawLine(0, screen.y(), width(), screen.y());
    }
    
    // 绘制原点坐标轴
    painter.setPen(QPen(QColor(100, 100, 100), 2.0));
    QPointF origin = worldToScreen(QPointF(0, 0));
    painter.drawLine(origin.x() - 20, origin.y(), origin.x() + 20, origin.y());
    painter.drawLine(origin.x(), origin.y() - 20, origin.x(), origin.y() + 20);
    
    painter.restore();
}

void CanvasWidget::drawObjects(QPainter& painter)
{
    painter.save();
    
    QElapsedTimer renderTimer;
    renderTimer.start();
    
    // 如果有 GDS 数据，渲染实际对象
    if (m_gdsData && !m_gdsData->structures.isEmpty()) {
        // 获取当前结构
        QString structName = m_currentStructure;
        if (structName.isEmpty()) {
            structName = m_gdsData->topStructure;
        }
        if (structName.isEmpty() && !m_gdsData->structures.isEmpty()) {
            structName = m_gdsData->structures.keys().first();
        }
        
        auto structure = m_gdsData->getStructure(structName);
        if (!structure) {
            painter.restore();
            return;
        }
        
        // ========== Image Cache 检查 ==========
        // 暂时禁用缓存，调试缩放问题
        bool enableCache = false;  // 临时禁用
        
        double scaleRounded = std::round(m_transform.scale * 10) / 10.0;
        int offsetXRounded = static_cast<int>(m_transform.offsetX * 10);
        int offsetYRounded = static_cast<int>(m_transform.offsetY * 10);
        
        ImageCacheKey cacheKey(structName, scaleRounded, 
                               offsetXRounded, offsetYRounded,
                               width(), height());
        
        QImage cachedImage;
        if (enableCache) {
            cachedImage = m_imageCache.get(cacheKey);
        }
        if (!cachedImage.isNull()) {
            // 缓存命中，直接绘制
            painter.drawImage(0, 0, cachedImage);
            double renderTime = renderTimer.elapsed();
            if (renderTime > 10) {
                qDebug() << "=== Image Cache 命中 === 耗时:" << renderTime << "ms";
            }
            painter.restore();
            return;
        }
        
        // 计算结构边界框
        qint64 minX = LLONG_MAX, maxX = LLONG_MIN, minY = LLONG_MAX, maxY = LLONG_MIN;
        bool hasValidBounds = false;
        
        for (const auto& b : structure->boundaries) {
            for (const auto& p : b.points) {
                minX = qMin(minX, p.x);
                maxX = qMax(maxX, p.x);
                minY = qMin(minY, p.y);
                maxY = qMax(maxY, p.y);
                hasValidBounds = true;
            }
        }
        
        for (const auto& path : structure->paths) {
            for (const auto& p : path.points) {
                minX = qMin(minX, p.x);
                maxX = qMax(maxX, p.x);
                minY = qMin(minY, p.y);
                maxY = qMax(maxY, p.y);
                hasValidBounds = true;
            }
        }
        
        if (!hasValidBounds) {
            painter.restore();
            return;
        }
        
        qint64 dbW = maxX - minX;
        qint64 dbH = maxY - minY;
        
        if (dbW <= 0 || dbH <= 0) {
            painter.restore();
            return;
        }
        
        // 计算缩放和偏移
        double baseScale = qMin((width() - 100.0) / dbW, (height() - 100.0) / dbH);
        double scale = baseScale * m_transform.scale;
        
        // Y轴翻转偏移
        double offsetY = (height() + dbH * scale) / 2.0 + minY * scale;
        double offsetX = (width() - dbW * scale) / 2.0 - minX * scale;
        
        // 应用用户平移
        offsetX += m_transform.offsetX * scale;
        offsetY -= m_transform.offsetY * scale;
        
        // 预定义图层颜色
        auto getLayerColor = [](int layer) -> QColor {
            int hue = (layer * 137) % 360;
            return QColor::fromHsl(hue, 180, 160);
        };
        
        // ========== 使用递归渲染展开 SREF/AREF ==========
        // 关键优化：根据缩放决定是否展开 SREF
        bool expandSREF = (m_transform.scale >= SREF_EXPAND_THRESHOLD);
        int srefExpandedCount = 0;
        
        // ========== 离屏渲染（用于缓存）==========
        // 创建离屏图像
        QImage renderImage(size(), QImage::Format_ARGB32_Premultiplied);
        renderImage.fill(m_renderConfig.backgroundColor);
        
        QPainter imagePainter(&renderImage);
        imagePainter.setRenderHint(QPainter::Antialiasing, m_renderConfig.antialiasing);
        
        // 渲染到离屏图像
        RenderStats stats;
        renderCell(imagePainter, structName, offsetX, offsetY, scale, 0, false, 0, 
                   stats, expandSREF, srefExpandedCount);
        imagePainter.end();
        
        // 绘制到屏幕
        painter.drawImage(0, 0, renderImage);
        
        // 存入 Image Cache（临时禁用）
        bool isPrecious = (cacheKey == m_lastFitAllKey);
        if (enableCache) {
            m_imageCache.put(cacheKey, renderImage, isPrecious);
        }
        
        // 性能日志（仅在渲染慢时输出）
        double renderTime = renderTimer.elapsed();
        
        // 调试输出：显示渲染统计
        static bool firstRender = true;
        if (firstRender || renderTime > 100) {
            qDebug() << "\n=== 渲染统计 ===";
            qDebug() << "结构:" << structName;
            qDebug() << "数据库坐标范围: X[" << minX << "," << maxX << "] Y[" << minY << "," << maxY << "]";
            qDebug() << "数据库尺寸:" << dbW << "x" << dbH << "nm =" << dbW/1000.0 << "x" << dbH/1000.0 << "μm";
            qDebug() << "缩放:" << m_transform.scale << "(展开阈值:" << SREF_EXPAND_THRESHOLD << ")";
            qDebug() << "SREF 总数:" << stats.srefCount << "| 展开数:" << srefExpandedCount;
            qDebug() << "AREF:" << stats.arefCount;
            qDebug() << "Boundary:" << stats.boundaryCount;
            qDebug() << "Path:" << stats.pathCount;
            qDebug() << "耗时:" << renderTime << "ms";
            qDebug() << "缓存大小:" << m_imageCache.getStats().count;
            qDebug() << "==============\n";
            firstRender = false;
        }
        
    } else {
        // 无数据时绘制示例对象
        // 使用 5 μm 作为默认尺寸
        qint64 defaultSize = DEFAULT_SHAPE_SIZE;  // 5000 nm = 5 μm
        
        // 示例矩形：5 x 3 μm
        qint64 sampleMinX = -defaultSize;   // -5 μm
        qint64 sampleMaxX = defaultSize;    // 5 μm
        qint64 sampleMinY = -defaultSize * 3 / 5;  // -3 μm
        qint64 sampleMaxY = defaultSize * 3 / 5;   // 3 μm
        
        // 使用与 drawObjects 相同的计算方式
        qint64 dbW = sampleMaxX - sampleMinX;
        qint64 dbH = sampleMaxY - sampleMinY;
        double baseScale = qMin((width() - 100.0) / qMax(1LL, dbW), (height() - 100.0) / qMax(1LL, dbH));
        double scale = baseScale * m_transform.scale;
        
        double offsetY = (height() + dbH * scale) / 2.0 + sampleMinY * scale;
        double offsetX = (width() - dbW * scale) / 2.0 - sampleMinX * scale;
        
        // 示例矩形
        QPolygonF rectPoly;
        rectPoly << QPointF(sampleMinX * scale + offsetX, -sampleMinY * scale + offsetY)
                 << QPointF(sampleMaxX * scale + offsetX, -sampleMinY * scale + offsetY)
                 << QPointF(sampleMaxX * scale + offsetX, -sampleMaxY * scale + offsetY)
                 << QPointF(sampleMinX * scale + offsetX, -sampleMaxY * scale + offsetY);
        
        painter.setPen(QPen(QColor(100, 150, 255), 2));
        painter.setBrush(QColor(100, 150, 255, 100));
        painter.drawPolygon(rectPoly);
        
        // 示例圆：半径 2.5 μm
        qint64 circleX = -defaultSize * 3 / 2;  // -7.5 μm
        qint64 circleY = defaultSize / 2;       // 2.5 μm
        qint64 circleR = defaultSize / 2;       // 2.5 μm
        
        QPointF circleCenter(circleX * scale + offsetX, -circleY * scale + offsetY);
        double radius = circleR * scale;
        
        painter.setPen(QPen(QColor(150, 255, 100), 2));
        painter.setBrush(QColor(150, 255, 100, 100));
        painter.drawEllipse(circleCenter, radius, radius);
    }
    
    painter.restore();
}

void CanvasWidget::drawRubberBand(QPainter& painter)
{
    painter.save();
    
    QPointF start = worldToScreen(m_rubberBandStart);
    QPointF end = worldToScreen(m_rubberBandEnd);
    QRectF rect(start, end);
    
    painter.setPen(QPen(QColor(255, 255, 255), 1, Qt::DashLine));
    painter.setBrush(QColor(100, 150, 255, 30));
    painter.drawRect(rect.normalized());
    
    painter.restore();
}

void CanvasWidget::drawPreview(QPainter& painter)
{
    painter.save();
    
    painter.setPen(QPen(QColor(255, 255, 0), 2, Qt::DashLine));
    painter.setBrush(QColor(255, 255, 0, 50));
    
    // 关键修复：m_drawPoints 现在存储数据库坐标，需要用 databaseToScreen 转换
    switch (m_currentTool) {
    case Tool::DrawRectangle: {
        // 使用 databaseToScreen 转换
        QPointF tl = databaseToScreen(m_drawRect.topLeft());
        QPointF br = databaseToScreen(m_drawRect.bottomRight());
        painter.drawRect(QRectF(tl, br).normalized());
        
        // 显示尺寸信息（数据库坐标是纳米，转换为微米显示）
        double width_nm = m_drawRect.width();
        double height_nm = m_drawRect.height();
        QString info = QString("W: %1 μm  H: %2 μm").arg(width_nm / 1000.0, 0, 'f', 2).arg(height_nm / 1000.0, 0, 'f', 2);
        painter.setPen(Qt::white);
        QPointF center = databaseToScreen(m_drawRect.center());
        painter.drawText(center + QPointF(10, -10), info);
        break;
    }
    
    case Tool::DrawPolygon:
    case Tool::DrawPath: {
        if (!m_drawPoints.isEmpty()) {
            QPolygonF screenPoly;
            for (const QPointF& pt : m_drawPoints) {
                screenPoly << databaseToScreen(pt);  // 使用 databaseToScreen
            }
            screenPoly << databaseToScreen(m_currentDrawPoint);  // 使用 databaseToScreen
            
            if (m_currentTool == Tool::DrawPolygon) {
                painter.drawPolygon(screenPoly);
            } else {
                painter.drawPolyline(screenPoly);
            }
            
            // 绘制顶点
            painter.setBrush(Qt::yellow);
            for (const QPointF& pt : screenPoly) {
                painter.drawEllipse(pt, 4, 4);
            }
        }
        break;
    }
    
    case Tool::DrawCircle: {
        // 只有在半径大于 0 时才绘制
        if (m_drawCircleRadius <= 0) {
            break;
        }
        
        // 使用 databaseToScreen 转换圆心
        QPointF center = databaseToScreen(m_drawCircleCenter);
        
        // 计算屏幕上的半径（需要考虑缩放）
        // m_drawCircleRadius 是数据库坐标（纳米）
        // 需要转换为屏幕像素
        if (!m_gdsData || m_currentStructure.isEmpty()) {
            // 无数据时使用简化方法
            double radius = m_drawCircleRadius / 1000.0 * m_transform.scale;
            painter.drawEllipse(center, radius, radius);
        } else {
            auto structure = m_gdsData->getStructure(m_currentStructure);
            if (structure) {
                // 使用与渲染相同的缩放
                qint64 minX = LLONG_MAX, maxX = LLONG_MIN, minY = LLONG_MAX, maxY = LLONG_MIN;
                for (const auto& b : structure->boundaries) {
                    for (const auto& p : b.points) {
                        minX = qMin(minX, p.x);
                        maxX = qMax(maxX, p.x);
                        minY = qMin(minY, p.y);
                        maxY = qMax(maxY, p.y);
                    }
                }
                qint64 dbW = maxX - minX;
                qint64 dbH = maxY - minY;
                double baseScale = qMin((width() - 100.0) / qMax(1LL, dbW), (height() - 100.0) / qMax(1LL, dbH));
                double scale = baseScale * m_transform.scale;
                double radius = m_drawCircleRadius * scale;
                painter.drawEllipse(center, radius, radius);
                
                // 显示半径信息
                QString info = QString("R: %1 μm").arg(m_drawCircleRadius / 1000.0, 0, 'f', 2);
                painter.setPen(Qt::white);
                painter.drawText(center + QPointF(radius + 10, 0), info);
            }
        }
        break;
    }
    
    default:
        break;
    }
    
    painter.restore();
}

void CanvasWidget::drawSelection(QPainter& painter)
{
    if (m_selectedObjects.isEmpty() || !m_gdsData) {
        return;
    }
    
    painter.save();
    
    // 选择高亮样式
    QPen highlightPen(QColor(255, 255, 0), 2); // 黄色边框
    highlightPen.setStyle(Qt::DashLine);
    painter.setPen(highlightPen);
    painter.setBrush(QColor(255, 255, 0, 50)); // 半透明黄色填充
    
    for (const auto& obj : m_selectedObjects) {
        auto structure = m_gdsData->getStructure(obj.structureName);
        if (!structure) continue;
        
        QRectF bounds = getObjectBounds(obj);
        if (!bounds.isNull()) {
            // 绘制选中边框
            QPointF tl = worldToScreen(bounds.topLeft());
            QPointF br = worldToScreen(bounds.bottomRight());
            QRectF screenRect(tl, br);
            painter.drawRect(screenRect.adjusted(-2, -2, 2, 2));
        }
    }
    
    painter.restore();
}

void CanvasWidget::drawRuler(QPainter& painter)
{
    painter.save();
    
    // 顶部标尺
    int rulerHeight = 25;
    painter.fillRect(0, 0, width(), rulerHeight, QColor(50, 50, 50));
    painter.setPen(QColor(150, 150, 150));
    painter.drawLine(0, rulerHeight - 1, width(), rulerHeight - 1);
    
    // 计算刻度间距
    double pixelPerMicron = m_transform.scale;
    double tickSpacing = 10; // μm
    while (tickSpacing * pixelPerMicron < 50) tickSpacing *= 2;
    while (tickSpacing * pixelPerMicron > 200) tickSpacing /= 2;
    
    // 绘制刻度
    QPointF leftWorld = screenToWorld(QPointF(rulerHeight, 0));
    QPointF rightWorld = screenToWorld(QPointF(width(), 0));
    
    double startX = qFloor(leftWorld.x() / tickSpacing) * tickSpacing;
    
    for (double x = startX; x <= rightWorld.x(); x += tickSpacing) {
        QPointF screen = worldToScreen(QPointF(x, 0));
        int tickHeight = 10;
        
        // 主刻度
        if (qFuzzyCompare(fmod(x, tickSpacing * 5), 0.0) || 
            qFuzzyCompare(fmod(x, tickSpacing * 5), tickSpacing * 5)) {
            tickHeight = 15;
            painter.drawText(screen.x() - 20, 12, QString::number(x, 'f', 0) + " μm");
        }
        
        painter.drawLine(screen.x(), rulerHeight - tickHeight, screen.x(), rulerHeight);
    }
    
    // 左侧标尺
    int rulerWidth = 40;
    painter.fillRect(0, rulerHeight, rulerWidth, height() - rulerHeight, QColor(50, 50, 50));
    painter.drawLine(rulerWidth - 1, rulerHeight, rulerWidth - 1, height());
    
    QPointF topWorld = screenToWorld(QPointF(0, rulerHeight));
    QPointF bottomWorld = screenToWorld(QPointF(0, height()));
    
    double startY = qFloor(topWorld.y() / tickSpacing) * tickSpacing;
    
    painter.save();
    painter.translate(rulerWidth - 5, 0);
    painter.rotate(-90);
    
    for (double y = startY; y <= bottomWorld.y(); y += tickSpacing) {
        QPointF screen = worldToScreen(QPointF(0, y));
        int tickHeight = 10;
        
        if (qFuzzyCompare(fmod(y, tickSpacing * 5), 0.0) || 
            qFuzzyCompare(fmod(y, tickSpacing * 5), tickSpacing * 5)) {
            tickHeight = 15;
            painter.drawText(-screen.y() - 20, 12, QString::number(y, 'f', 0) + " μm");
        }
        
        painter.drawLine(-screen.y(), 0, -screen.y(), tickHeight);
    }
    
    painter.restore();
    painter.restore();
}

//=============================================================================
// 鼠标事件
//=============================================================================

void CanvasWidget::mousePressEvent(QMouseEvent* event)
{
    m_mousePressed = true;
    m_lastMousePos = event->pos();
    m_lastWorldPos = screenToWorld(event->pos());
    
    switch (m_currentTool) {
    case Tool::Select:
        handleSelectPress(m_lastWorldPos, event->button(), event->modifiers());
        break;
    case Tool::Pan:
        handlePanPress(m_lastWorldPos, event->button());
        break;
    default:
        handleDrawPress(m_lastWorldPos, event->button());
        break;
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPointF worldPos = screenToWorld(event->pos());
    updateCoordinateDisplay(worldPos);
    
    // 平移处理
    if (m_isPanning || (m_mousePressed && m_currentTool == Tool::Pan) || m_spacePressed) {
        QPointF delta = event->pos() - m_lastMousePos;
        m_transform.offsetX += delta.x() / m_transform.scale;
        m_transform.offsetY -= delta.y() / m_transform.scale;
        m_lastMousePos = event->pos();
        update();
        return;
    }
    
    // 橡皮筋选择
    if (m_rubberBandActive && m_mousePressed) {
        m_rubberBandEnd = worldPos;
        update();
        return;
    }
    
    // 绘制预览 - 使用数据库坐标系统
    if (m_isDrawing) {
        // 关键修复：使用 screenToDatabase 获取与渲染一致的坐标
        QPointF dbPos = screenToDatabase(event->pos());
        m_currentDrawPoint = dbPos;
        
        if (m_currentTool == Tool::DrawRectangle) {
            m_drawRect = QRectF(m_drawPoints.first(), dbPos).normalized();
        } else if (m_currentTool == Tool::DrawCircle) {
            QPointF diff = dbPos - m_drawCircleCenter;
            m_drawCircleRadius = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
        }
        
        update();
    }
    
    m_lastMousePos = event->pos();
    m_lastWorldPos = worldPos;
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_mousePressed = false;
    
    if (m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    if (m_rubberBandActive) {
        m_rubberBandActive = false;
        
        // 计算选择区域
        QPointF start = m_rubberBandStart;
        QPointF end = screenToWorld(event->pos());
        
        bool addToSelection = event->modifiers().testFlag(Qt::ControlModifier) || 
                              event->modifiers().testFlag(Qt::ShiftModifier);
        
        // 判断是点击还是框选
        double dx = qAbs(end.x() - start.x());
        double dy = qAbs(end.y() - start.y());
        double tolerance = SELECTION_TOLERANCE / m_transform.scale;
        
        if (dx < tolerance && dy < tolerance) {
            // 点击选择
            performPointSelection(start, addToSelection);
        } else {
            // 框选
            QRectF selectionRect(start, end);
            performRectSelection(selectionRect.normalized(), addToSelection);
        }
        
        update();
    }
    
    // 完成矩形绘制（单击拖拽模式）
    if (m_isDrawing && m_currentTool == Tool::DrawRectangle && m_drawPoints.size() == 1) {
        QRectF rect(m_drawPoints.first(), screenToWorld(event->pos()));
        if (rect.width() > 1 && rect.height() > 1) {
            emit objectCreated("Rectangle", rect);
        }
        m_isDrawing = false;
        m_drawPoints.clear();
        update();
    }
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    // 双击完成多边形/路径绘制
    if (m_isDrawing && (m_currentTool == Tool::DrawPolygon || m_currentTool == Tool::DrawPath)) {
        if (m_drawPoints.size() >= 3) {
            QRectF bounds;
            for (const QPointF& pt : m_drawPoints) {
                bounds = bounds.united(QRectF(pt, pt));
            }
            emit objectCreated(m_currentTool == Tool::DrawPolygon ? "Polygon" : "Path", bounds);
        }
        m_isDrawing = false;
        m_drawPoints.clear();
        update();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent* event)
{
    // 获取鼠标位置作为缩放中心
    QPointF mousePos = event->position();
    
    // 计算缩放前鼠标位置对应的世界坐标
    double worldX = (mousePos.x() - width() / 2.0) / m_transform.scale - m_transform.offsetX;
    double worldY = (height() / 2.0 - mousePos.y()) / m_transform.scale - m_transform.offsetY;
    
    // 进行缩放
    double delta = event->angleDelta().y() / 120.0;
    double factor = std::pow(ZOOM_FACTOR, delta);
    double newScale = qBound(MIN_ZOOM, m_transform.scale * factor, MAX_ZOOM);
    
    if (!qFuzzyCompare(m_transform.scale, newScale)) {
        m_transform.scale = newScale;
        
        // 计算新的偏移量，使鼠标位置的世界坐标保持不变
        m_transform.offsetX = (mousePos.x() - width() / 2.0) / newScale - worldX;
        m_transform.offsetY = (height() / 2.0 - mousePos.y()) / newScale - worldY;
        
        update();
        emit zoomChanged(m_transform.scale);
    }
    
    event->accept();
}

//=============================================================================
// 键盘事件
//=============================================================================

void CanvasWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        setCursor(Qt::OpenHandCursor);
    }
    
    if (event->key() == Qt::Key_Escape) {
        // 取消当前操作
        m_isDrawing = false;
        m_rubberBandActive = false;
        m_drawPoints.clear();
        
        // 清除选择
        clearSelection();
        
        update();
        emit statusMessage(tr("Operation cancelled"));
    }
    
    // Ctrl+A 全选
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        selectAll();
    }
    
    // Delete 删除选中对象
    if (event->key() == Qt::Key_Delete) {
        if (!m_selectedObjects.isEmpty()) {
            // TODO: 实现删除功能
            emit statusMessage(tr("Delete %1 objects (not implemented)").arg(m_selectedObjects.size()));
        }
    }
    
    QWidget::keyPressEvent(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        updateCursor();
    }
    
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::focusOutEvent(QFocusEvent* event)
{
    m_spacePressed = false;
    updateCursor();
    QWidget::focusOutEvent(event);
}

//=============================================================================
// 选择辅助函数
//=============================================================================

QRectF CanvasWidget::getObjectBounds(const SelectedObject& obj) const
{
    if (!m_gdsData) return QRectF();
    
    auto structure = m_gdsData->getStructure(obj.structureName);
    if (!structure) return QRectF();
    
    QRectF bounds;
    
    if (obj.objectType == 0 && obj.index < structure->boundaries.size()) {
        // Boundary
        const auto& boundary = structure->boundaries[obj.index];
        if (!boundary.points.isEmpty()) {
            double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
            for (const auto& pt : boundary.points) {
                double x = pt.x / 1000.0;
                double y = pt.y / 1000.0;
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
            bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
        }
    } else if (obj.objectType == 1 && obj.index < structure->paths.size()) {
        // Path
        const auto& path = structure->paths[obj.index];
        if (!path.points.isEmpty()) {
            double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
            for (const auto& pt : path.points) {
                double x = pt.x / 1000.0;
                double y = pt.y / 1000.0;
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
            // 考虑路径宽度
            double halfWidth = path.width / 2000.0;
            bounds = QRectF(QPointF(minX - halfWidth, minY - halfWidth), 
                           QPointF(maxX + halfWidth, maxY + halfWidth));
        }
    } else if (obj.objectType == 2 && obj.index < structure->texts.size()) {
        // Text
        const auto& text = structure->texts[obj.index];
        double x = text.position.x / 1000.0;
        double y = text.position.y / 1000.0;
        bounds = QRectF(x - 10, y - 5, 20, 10); // 近似文本大小
    }
    
    return bounds;
}

bool CanvasWidget::isPointInObject(const QPointF& point, const SelectedObject& obj) const
{
    QRectF bounds = getObjectBounds(obj);
    if (bounds.isNull()) return false;
    
    // 使用容差扩展边界
    double tolerance = SELECTION_TOLERANCE / m_transform.scale;
    bounds.adjust(-tolerance, -tolerance, tolerance, tolerance);
    
    return bounds.contains(point);
}

bool CanvasWidget::isRectIntersectObject(const QRectF& rect, const SelectedObject& obj) const
{
    QRectF bounds = getObjectBounds(obj);
    if (bounds.isNull()) return false;
    
    return rect.intersects(bounds);
}

void CanvasWidget::performPointSelection(const QPointF& worldPos, bool addToSelection)
{
    if (!m_gdsData) return;
    
    // 获取当前结构
    QString structName = m_currentStructure;
    if (structName.isEmpty() && !m_gdsData->structures.isEmpty()) {
        structName = m_gdsData->structures.keys().first();
    }
    
    auto structure = m_gdsData->getStructure(structName);
    if (!structure) return;
    
    // 查找点击的对象（从后往前，后绘制的在上面）
    SelectedObject hitObj;
    hitObj.structureName = structName;
    hitObj.objectType = -1;
    hitObj.index = -1;
    
    // 检查文本
    for (int i = structure->texts.size() - 1; i >= 0; --i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 2;
        obj.index = i;
        if (isPointInObject(worldPos, obj)) {
            hitObj = obj;
            break;
        }
    }
    
    // 检查路径
    if (hitObj.objectType < 0) {
        for (int i = structure->paths.size() - 1; i >= 0; --i) {
            SelectedObject obj;
            obj.structureName = structName;
            obj.objectType = 1;
            obj.index = i;
            if (isPointInObject(worldPos, obj)) {
                hitObj = obj;
                break;
            }
        }
    }
    
    // 检查边界
    if (hitObj.objectType < 0) {
        for (int i = structure->boundaries.size() - 1; i >= 0; --i) {
            SelectedObject obj;
            obj.structureName = structName;
            obj.objectType = 0;
            obj.index = i;
            if (isPointInObject(worldPos, obj)) {
                hitObj = obj;
                break;
            }
        }
    }
    
    // 更新选择
    if (!addToSelection) {
        m_selectedObjects.clear();
    }
    
    if (hitObj.objectType >= 0) {
        if (m_selectedObjects.contains(hitObj)) {
            // 已选中则取消选择
            m_selectedObjects.remove(hitObj);
        } else {
            // 添加到选择
            m_selectedObjects.insert(hitObj);
        }
    }
    
    emit selectionChanged(m_selectedObjects.size());
    update();
}

void CanvasWidget::performRectSelection(const QRectF& worldRect, bool addToSelection)
{
    if (!m_gdsData) return;
    
    // 获取当前结构
    QString structName = m_currentStructure;
    if (structName.isEmpty() && !m_gdsData->structures.isEmpty()) {
        structName = m_gdsData->structures.keys().first();
    }
    
    auto structure = m_gdsData->getStructure(structName);
    if (!structure) return;
    
    if (!addToSelection) {
        m_selectedObjects.clear();
    }
    
    // 检查边界
    for (int i = 0; i < structure->boundaries.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 0;
        obj.index = i;
        if (isRectIntersectObject(worldRect, obj)) {
            m_selectedObjects.insert(obj);
        }
    }
    
    // 检查路径
    for (int i = 0; i < structure->paths.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 1;
        obj.index = i;
        if (isRectIntersectObject(worldRect, obj)) {
            m_selectedObjects.insert(obj);
        }
    }
    
    // 检查文本
    for (int i = 0; i < structure->texts.size(); ++i) {
        SelectedObject obj;
        obj.structureName = structName;
        obj.objectType = 2;
        obj.index = i;
        if (isRectIntersectObject(worldRect, obj)) {
            m_selectedObjects.insert(obj);
        }
    }
    
    emit selectionChanged(m_selectedObjects.size());
    emit statusMessage(tr("Selected %1 objects").arg(m_selectedObjects.size()));
    update();
}

//=============================================================================
// 交互处理
//=============================================================================

void CanvasWidget::handleSelectPress(const QPointF& worldPos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    if (button == Qt::LeftButton) {
        bool addToSelection = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::ShiftModifier);
        
        // 开始橡皮筋选择
        m_rubberBandActive = true;
        m_rubberBandStart = worldPos;
        m_rubberBandEnd = worldPos;
        
        if (!addToSelection) {
            emit statusMessage(tr("Drag to select objects, Ctrl+Click to add to selection"));
        } else {
            emit statusMessage(tr("Adding to selection..."));
        }
    }
}

void CanvasWidget::handlePanPress(const QPointF& worldPos, Qt::MouseButton button)
{
    Q_UNUSED(worldPos);
    
    if (button == Qt::LeftButton) {
        m_isPanning = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void CanvasWidget::handleDrawPress(const QPointF& screenPos, Qt::MouseButton button)
{
    // 关键修复：使用 screenToDatabase 获取与渲染一致的数据库坐标
    QPointF dbPos = screenToDatabase(screenPos);
    
    qDebug() << "=== handleDrawPress === button:" << button 
             << "screenPos:" << screenPos 
             << "dbPos:" << dbPos
             << "canvas size:" << width() << "x" << height();
    
    if (button == Qt::LeftButton) {
        if (!m_isDrawing) {
            // 开始绘制
            m_isDrawing = true;
            m_drawPoints.clear();
            m_drawPoints.append(dbPos);  // 使用数据库坐标
            
            if (m_currentTool == Tool::DrawCircle) {
                m_drawCircleCenter = dbPos;
                m_drawCircleRadius = 0;
                qDebug() << "Circle drawing started, center:" << m_drawCircleCenter;
            } else if (m_currentTool == Tool::DrawRectangle) {
                m_drawRect = QRectF(dbPos, dbPos);
                qDebug() << "Rectangle drawing started, rect:" << m_drawRect;
            }
            
            qDebug() << "Drawing started, tool:" << (int)m_currentTool;
            emit statusMessage(tr("Drawing... Right-click to finish"));
        } else {
            // 添加点（用于 Polygon/Path）
            m_drawPoints.append(dbPos);  // 使用数据库坐标
            
            if (m_currentTool == Tool::DrawPolygon) {
                emit statusMessage(tr("Polygon: %1 points. Right-click to finish").arg(m_drawPoints.size()));
            } else if (m_currentTool == Tool::DrawPath) {
                emit statusMessage(tr("Path: %1 points. Right-click to finish").arg(m_drawPoints.size()));
            }
        }
    } else if (button == Qt::RightButton && m_isDrawing) {
        qDebug() << "Right button pressed, finishing drawing...";
        
        // 右键完成绘制
        if (m_currentTool == Tool::DrawRectangle) {
            // 矩形：限制尺寸范围
            QRectF rect = m_drawRect;
            double w = qAbs(rect.width());
            double h = qAbs(rect.height());
            
            if (w < MIN_SHAPE_SIZE || h < MIN_SHAPE_SIZE) {
                // 使用默认尺寸
                qreal cx = rect.center().x();
                qreal cy = rect.center().y();
                rect = QRectF(cx - DEFAULT_SHAPE_SIZE/2, cy - DEFAULT_SHAPE_SIZE/2,
                              DEFAULT_SHAPE_SIZE, DEFAULT_SHAPE_SIZE);
                qDebug() << "Rectangle too small, using default 5μm";
            } else if (w > MAX_SHAPE_SIZE || h > MAX_SHAPE_SIZE) {
                // 限制最大尺寸
                qreal cx = rect.center().x();
                qreal cy = rect.center().y();
                double newSize = qMin(w, h);
                newSize = qMin(newSize, (double)MAX_SHAPE_SIZE);
                rect = QRectF(cx - newSize/2, cy - newSize/2, newSize, newSize);
                qDebug() << "Rectangle too large, limiting to" << newSize/1000.0 << "μm";
            }
            
            qDebug() << "Emitting Rectangle:" << rect << "size:" << rect.width()/1000.0 << "x" << rect.height()/1000.0 << "μm";
            emit objectCreated("Rectangle", rect);
            
        } else if (m_currentTool == Tool::DrawPolygon && m_drawPoints.size() >= 3) {
            // 正确计算 bounds
            qreal minX = m_drawPoints.first().x(), maxX = minX;
            qreal minY = m_drawPoints.first().y(), maxY = minY;
            for (const QPointF& pt : m_drawPoints) {
                minX = qMin(minX, pt.x());
                maxX = qMax(maxX, pt.x());
                minY = qMin(minY, pt.y());
                maxY = qMax(maxY, pt.y());
            }
            QRectF bounds(minX, minY, maxX - minX, maxY - minY);
            qDebug() << "Emitting Polygon:" << m_drawPoints.size() << "points, bounds:" << bounds;
            emit objectCreated("Polygon", bounds);
            
        } else if (m_currentTool == Tool::DrawPath && m_drawPoints.size() >= 2) {
            // 正确计算 bounds
            qreal minX = m_drawPoints.first().x(), maxX = minX;
            qreal minY = m_drawPoints.first().y(), maxY = minY;
            for (const QPointF& pt : m_drawPoints) {
                minX = qMin(minX, pt.x());
                maxX = qMax(maxX, pt.x());
                minY = qMin(minY, pt.y());
                maxY = qMax(maxY, pt.y());
            }
            QRectF bounds(minX, minY, maxX - minX, maxY - minY);
            qDebug() << "Emitting Path:" << m_drawPoints.size() << "points, bounds:" << bounds;
            emit objectCreated("Path", bounds);
            
        } else if (m_currentTool == Tool::DrawCircle) {
            // 限制半径范围：最小 1μm，最大 50μm
            double radius = m_drawCircleRadius;
            if (radius < MIN_SHAPE_SIZE) {
                radius = DEFAULT_SHAPE_SIZE;  // 默认 5 μm
                qDebug() << "Circle radius too small, using default:" << radius << "nm";
            } else if (radius > MAX_SHAPE_SIZE) {
                radius = MAX_SHAPE_SIZE;  // 最大 50 μm
                qDebug() << "Circle radius too large, limiting to:" << radius << "nm";
            }
            
            QRectF bounds(m_drawCircleCenter.x() - radius,
                          m_drawCircleCenter.y() - radius,
                          radius * 2,
                          radius * 2);
            qDebug() << "Emitting Circle, radius:" << radius << "nm (" << (radius/1000.0) << "μm) bounds:" << bounds;
            emit objectCreated("Circle", bounds);
        }
        
        m_isDrawing = false;
        m_drawPoints.clear();
        update();
    }
}

//=============================================================================
// 辅助函数
//=============================================================================

void CanvasWidget::updateCursor()
{
    switch (m_currentTool) {
    case Tool::Select:
        setCursor(Qt::ArrowCursor);
        break;
    case Tool::Pan:
        setCursor(Qt::OpenHandCursor);
        break;
    case Tool::DrawRectangle:
    case Tool::DrawPolygon:
    case Tool::DrawPath:
    case Tool::DrawCircle:
        setCursor(Qt::CrossCursor);
        break;
    }
}

void CanvasWidget::updateCoordinateDisplay(const QPointF& worldPos)
{
    emit coordinateChanged(worldPos.x(), worldPos.y());
}

//=============================================================================
// SREF/AREF 递归渲染
//=============================================================================

// 缓存的图层颜色
static QColor getLayerColor(int layer) {
    static QHash<int, QColor> colorCache;
    if (!colorCache.contains(layer)) {
        int hue = (layer * 137) % 360;
        colorCache[layer] = QColor::fromHsl(hue, 180, 160);
    }
    return colorCache[layer];
}

void CanvasWidget::renderCell(QPainter& painter, const QString& cellName,
                               double offsetX, double offsetY, double scale,
                               double rotation, bool mirrorX,
                               int depth, RenderStats& stats,
                               bool expandSREF, int& srefExpandedCount)
{
    // 防止无限递归
    if (depth > MAX_RECURSION_DEPTH) {
        return;
    }
    
    auto structure = m_gdsData->getStructure(cellName);
    if (!structure) {
        return;
    }
    
    // ========== Cell 缓存（参考 KLayout）==========
    // 简化版：直接渲染到屏幕，后续再优化离屏缓存
    
    // 检查缓存（仅子Cell）
    if (m_cellRenderCache && depth > 0) {
        CellCacheKey cacheKey(cellName, scale, rotation, mirrorX, 0);
        QImage cachedFill;
        QPointF cacheOffset;
        
        if (m_cellRenderCache->get(cacheKey, cachedFill, cacheOffset)) {
            // ✅ 缓存命中！直接绘制
            double drawX = offsetX + cacheOffset.x();
            double drawY = offsetY + cacheOffset.y();
            painter.drawImage(QPointF(drawX, drawY), cachedFill);
            stats.boundaryCount += structure->boundaries.size();
            return;
        }
    }
    
    // ========== 视口裁剪：计算当前视口范围 ==========
    // 暂时禁用视口裁剪，调试缩放问题
    QRectF viewport;
    bool enableViewportCulling = false;  // 临时禁用
    
    if (enableViewportCulling) {
        QPointF viewTopLeft = screenToWorld(QPointF(0, 0));
        QPointF viewBottomRight = screenToWorld(QPointF(width(), height()));
        viewport = QRectF(
            qMin(viewTopLeft.x(), viewBottomRight.x()),
            qMin(viewTopLeft.y(), viewBottomRight.y()),
            qAbs(viewBottomRight.x() - viewTopLeft.x()),
            qAbs(viewBottomRight.y() - viewTopLeft.y())
        );
    }
    
    // 渲染直接包含的 Boundary
    int boundaryCulled = 0;
    for (const auto& boundary : structure->boundaries) {
        // 视口裁剪（Boundary）
        if (enableViewportCulling && boundary.points.size() >= 3) {
            // 快速边界框测试
            qint64 bMinX = LLONG_MAX, bMaxX = LLONG_MIN;
            qint64 bMinY = LLONG_MAX, bMaxY = LLONG_MIN;
            for (const auto& p : boundary.points) {
                bMinX = qMin(bMinX, p.x);
                bMaxX = qMax(bMaxX, p.x);
                bMinY = qMin(bMinY, p.y);
                bMaxY = qMax(bMaxY, p.y);
            }
            
            // 转换到世界坐标（微米）
            QRectF bBox(bMinX / 1000.0, bMinY / 1000.0, 
                       (bMaxX - bMinX) / 1000.0, (bMaxY - bMinY) / 1000.0);
            
            if (!viewport.intersects(bBox)) {
                boundaryCulled++;
                continue;
            }
        }
        
        renderBoundary(painter, boundary, offsetX, offsetY, scale, rotation, mirrorX);
        stats.boundaryCount++;
    }
    
    // 渲染直接包含的 Path
    for (const auto& path : structure->paths) {
        renderPath(painter, path, offsetX, offsetY, scale, rotation, mirrorX);
        stats.pathCount++;
    }
    
    // 渲染直接包含的 Text
    for (const auto& text : structure->texts) {
        renderText(painter, text, offsetX, offsetY, scale, rotation, mirrorX);
        stats.textCount++;
    }
    
    // ========== 视口裁剪：SREF 处理 ==========
    int srefCulled = 0;
    int srefBoundingBoxCount = 0;
    
    for (const auto& sref : structure->references) {
        stats.srefCount++;
        
        // ========== 视口裁剪判断 ==========
        if (enableViewportCulling) {
            auto childStructure = m_gdsData->getStructure(sref.structureName);
            if (childStructure && childStructure->boundingBox.isValid()) {
                // 计算 SREF 实例的世界坐标边界框
                double srefX = sref.position.x / 1000.0;  // nm → μm
                double srefY = sref.position.y / 1000.0;
                
                double childW = (childStructure->boundingBox.right - childStructure->boundingBox.left) / 1000.0;
                double childH = (childStructure->boundingBox.bottom - childStructure->boundingBox.top) / 1000.0;
                
                QRectF srefBBox(srefX, srefY, childW * sref.magnification, childH * sref.magnification);
                
                // 视口外，跳过
                if (!viewport.intersects(srefBBox)) {
                    srefCulled++;
                    continue;
                }
            }
        }
        
        // ========== 关键优化：SREF 展开 ==========
        // 参考 KLayout：始终展开所有 SREF，无数量限制
        // 性能优化依赖 Cell Cache 和视口裁剪
        
        // 展开渲染子 Cell
        srefExpandedCount++;
            
        double childOffsetX = offsetX + sref.position.x * scale * (mirrorX ? -1 : 1);
        double childOffsetY = offsetY + sref.position.y * scale;
        double childRotation = rotation + sref.angle;
        double childScale = scale * sref.magnification;
        bool childMirrorX = mirrorX != sref.reflected;
        
        renderCell(painter, sref.structureName,
                   childOffsetX, childOffsetY, childScale,
                   childRotation, childMirrorX,
                   depth + 1, stats, expandSREF, srefExpandedCount);
    }
    
    // 日志输出（仅顶层 Cell，只打印最终统计）
    // 移除循环中的日志，避免刷屏
    
    // 处理 AREF（阵列引用）
    for (const auto& aref : structure->arrays) {
        stats.arefCount++;
        
        // AREF 通常数量较少，直接展开
        for (int row = 0; row < aref.rows; ++row) {
            for (int col = 0; col < aref.columns; ++col) {
                double instanceX = aref.position.x + col * aref.columnVector.x + row * aref.rowVector.x;
                double instanceY = aref.position.y + col * aref.columnVector.y + row * aref.rowVector.y;
                
                double childOffsetX = offsetX + instanceX * scale * (mirrorX ? -1 : 1);
                double childOffsetY = offsetY + instanceY * scale;
                
                renderCell(painter, aref.structureName,
                           childOffsetX, childOffsetY, scale,
                           rotation, mirrorX,
                           depth + 1, stats, expandSREF, srefExpandedCount);
            }
        }
    }
    
    // ========== 存入 Cell Cache ==========
    // 参考 KLayout：渲染完成后存入缓存（下次复用）
    // 注意：这里简化实现，实际上应该渲染到离屏图像
    // 当前实现：不存入，因为我们是直接渲染到屏幕
    
    // TODO: 正确实现离屏渲染后存入缓存
}

void CanvasWidget::renderBoundary(QPainter& painter, const GDSBoundary& boundary,
                                    double offsetX, double offsetY, double scale,
                                    double rotation, bool mirrorX)
{
    if (boundary.points.size() < 3) return;
    
    QColor fillColor = getLayerColor(boundary.layer);
    fillColor.setAlpha(180);
    QColor borderColor = fillColor.darker(150);
    
    QPolygonF poly;
    poly.reserve(boundary.points.size());
    
    for (const auto& pt : boundary.points) {
        double x = pt.x * scale;
        double y = pt.y * scale;
        
        if (mirrorX) x = -x;
        x += offsetX;
        y = -y + offsetY;
        
        poly << QPointF(x, y);
    }
    
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(QBrush(fillColor));
    painter.drawPolygon(poly);
}

void CanvasWidget::renderPath(QPainter& painter, const GDSPath& path,
                               double offsetX, double offsetY, double scale,
                               double rotation, bool mirrorX)
{
    if (path.points.size() < 2) return;
    
    auto getLayerColor = [](int layer) -> QColor {
        int hue = (layer * 137) % 360;
        return QColor::fromHsl(hue, 180, 160);
    };
    
    QColor color = getLayerColor(path.layer);
    double penWidth = qMax(1.0, static_cast<double>(path.width) * scale);
    
    QPainterPath pp;
    bool first = true;
    
    for (const auto& pt : path.points) {
        double x = pt.x * scale;
        double y = pt.y * scale;
        
        if (mirrorX) x = -x;
        
        x += offsetX;
        y = -y + offsetY;
        
        if (first) {
            pp.moveTo(x, y);
            first = false;
        } else {
            pp.lineTo(x, y);
        }
    }
    
    painter.setPen(QPen(color, penWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(pp);
}

void CanvasWidget::renderText(QPainter& painter, const GDSText& text,
                                double offsetX, double offsetY, double scale,
                                double rotation, bool mirrorX)
{
    auto getLayerColor = [](int layer) -> QColor {
        int hue = (layer * 137) % 360;
        return QColor::fromHsl(hue, 180, 160);
    };
    
    QColor color = getLayerColor(text.layer);
    
    double x = text.position.x * scale;
    double y = text.position.y * scale;
    
    if (mirrorX) x = -x;
    
    x += offsetX;
    y = -y + offsetY;
    
    painter.setPen(color);
    painter.save();
    painter.translate(x, y);
    painter.rotate(-(rotation + text.angle));
    painter.drawText(QPointF(0, 0), text.text);
    painter.restore();
}

} // namespace QLayoutEDA