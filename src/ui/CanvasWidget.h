/**
 * @file CanvasWidget.h
 * @brief 画布控件
 * @author QLayoutEDA Team
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QSet>
#include <memory>
#include "core/Types.h"
#include "ImageCache.h"

namespace QLayoutEDA {

class RenderEngine;
class QuadTree;
class CellRenderCache;

/**
 * @brief 选中对象信息
 */
struct SelectedObject {
    QString structureName;  ///< 所属结构名
    int objectType;         ///< 对象类型 (0=boundary, 1=path, 2=text)
    int index;              ///< 在数组中的索引
    
    bool operator==(const SelectedObject& other) const {
        return structureName == other.structureName 
            && objectType == other.objectType 
            && index == other.index;
    }
};

uint qHash(const SelectedObject& obj, uint seed = 0);

/**
 * @brief 画布控件
 * 
 * 负责图形渲染、鼠标交互、绘制操作
 */
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 工具类型
     */
    enum class Tool {
        Select,         ///< 选择工具
        Pan,            ///< 平移工具
        DrawRectangle,  ///< 绘制矩形
        DrawPolygon,    ///< 绘制多边形
        DrawPath,       ///< 绘制路径
        DrawCircle      ///< 绘制圆形
    };

    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget() override;

    // 工具设置
    void setTool(Tool tool);
    Tool currentTool() const { return m_currentTool; }
    
    // 数据设置
    void setGDSData(GDSDataPtr data);
    
    /**
     * @brief 设置当前显示的结构（Cell）
     */
    void setCurrentStructure(const QString& structureName);
    
    /**
     * @brief 获取当前结构名
     */
    QString currentStructure() const { return m_currentStructure; }
    
    /**
     * @brief 获取当前视图变换
     */
    ViewTransform getViewTransform() const { return m_transform; }
    
    // 视图操作
    void zoomIn();
    void zoomOut();
    void zoomActual();
    void fitAll();
    void setZoom(double scale);
    
    // 图层设置
    void setCurrentLayer(int layer);
    int currentLayer() const { return m_currentLayer; }
    
    // 选择操作
    void selectObject(const SelectedObject& obj);
    void deselectObject(const SelectedObject& obj);
    void selectAll();
    void clearSelection();
    QSet<SelectedObject> getSelectedObjects() const { return m_selectedObjects; }
    bool hasSelection() const { return !m_selectedObjects.isEmpty(); }
    int selectionCount() const { return m_selectedObjects.size(); }

signals:
    /**
     * @brief 鼠标坐标变化信号
     */
    void coordinateChanged(double x, double y);
    
    /**
     * @brief 缩放变化信号
     */
    void zoomChanged(double zoom);
    
    /**
     * @brief 状态消息信号
     */
    void statusMessage(const QString& message);
    
    /**
     * @brief 对象创建信号
     */
    void objectCreated(const QString& type, const QRectF& bounds);
    
    /**
     * @brief 选择变化信号
     */
    void selectionChanged(int count);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    // 坐标转换
    QPointF screenToWorld(const QPointF& screen) const;
    QPointF worldToScreen(const QPointF& world) const;
    
    /**
     * @brief 屏幕坐标转数据库坐标（用于绘制）
     * 与渲染使用相同的坐标系统
     */
    QPointF screenToDatabase(const QPointF& screen) const;
    
    /**
     * @brief 数据库坐标转屏幕坐标（用于预览）
     */
    QPointF databaseToScreen(const QPointF& db) const;
    
    // 绘制函数
    void drawGrid(QPainter& painter);
    void drawObjects(QPainter& painter);
    void drawRubberBand(QPainter& painter);
    void drawPreview(QPainter& painter);
    void drawSelection(QPainter& painter);
    void drawRuler(QPainter& painter);
    
    // ========== SREF/AREF 递归渲染 ==========
    /**
     * @brief 递归渲染 Cell（包含 SREF/AREF 展开）
     * @param painter 画笔
     * @param cellName Cell 名称
     * @param transform 累积变换矩阵
     * @param depth 递归深度（防止无限循环）
     * @param stats 渲染统计
     */
    struct RenderStats {
        int boundaryCount = 0;
        int pathCount = 0;
        int textCount = 0;
        int srefCount = 0;
        int arefCount = 0;
    };
    void renderCell(QPainter& painter, const QString& cellName,
                    double offsetX, double offsetY, double scale,
                    double rotation, bool mirrorX,
                    int depth, RenderStats& stats,
                    bool expandSREF, int& srefExpandedCount);
    
    /**
     * @brief 渲染 Boundary（应用变换）
     */
    void renderBoundary(QPainter& painter, const GDSBoundary& boundary,
                        double offsetX, double offsetY, double scale,
                        double rotation, bool mirrorX);
    
    /**
     * @brief 渲染 Path（应用变换）
     */
    void renderPath(QPainter& painter, const GDSPath& path,
                    double offsetX, double offsetY, double scale,
                    double rotation, bool mirrorX);
    
    /**
     * @brief 渲染 Text（应用变换）
     */
    void renderText(QPainter& painter, const GDSText& text,
                    double offsetX, double offsetY, double scale,
                    double rotation, bool mirrorX);
    
    // 选择相关
    void performPointSelection(const QPointF& worldPos, bool addToSelection);
    void performRectSelection(const QRectF& worldRect, bool addToSelection);
    bool isPointInObject(const QPointF& point, const SelectedObject& obj) const;
    bool isRectIntersectObject(const QRectF& rect, const SelectedObject& obj) const;
    QRectF getObjectBounds(const SelectedObject& obj) const;
    
    // 交互处理
    void handleSelectPress(const QPointF& worldPos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void handlePanPress(const QPointF& worldPos, Qt::MouseButton button);
    void handleDrawPress(const QPointF& worldPos, Qt::MouseButton button);
    
    // 更新显示
    void updateCursor();
    void updateCoordinateDisplay(const QPointF& worldPos);

private:
    // 视图变换
    ViewTransform m_transform;
    double m_baseZoom = 1.0;
    
    // 当前工具
    Tool m_currentTool = Tool::Select;
    int m_currentLayer = 0;
    
    // 鼠标状态
    bool m_mousePressed = false;
    bool m_isPanning = false;
    bool m_isDrawing = false;
    bool m_spacePressed = false;
    QPointF m_lastMousePos;
    QPointF m_lastWorldPos;
    
    // 橡皮筋选择
    bool m_rubberBandActive = false;
    QPointF m_rubberBandStart;
    QPointF m_rubberBandEnd;
    
    // 绘制状态
    QVector<QPointF> m_drawPoints;      ///< 绘制点集合
    QPointF m_currentDrawPoint;          ///< 当前绘制点
    QRectF m_drawRect;                   ///< 绘制矩形预览
    QPointF m_drawCircleCenter;          ///< 圆心
    double m_drawCircleRadius = 0;       ///< 圆半径
    
    // 默认绘制参数（单位：纳米）
    static constexpr qint64 DEFAULT_SHAPE_SIZE = 5000;  ///< 默认 5 μm = 5000 nm
    static constexpr qint64 MAX_SHAPE_SIZE = 50000;     ///< 最大 50 μm = 50000 nm
    static constexpr qint64 MIN_SHAPE_SIZE = 1000;      ///< 最小 1 μm = 1000 nm
    
    // 渲染配置
    RenderConfig m_renderConfig;
    
    // GDS 数据
    GDSDataPtr m_gdsData;
    QString m_currentStructure;          ///< 当前显示的结构名
    
    // 渲染引擎
    std::unique_ptr<RenderEngine> m_renderEngine;
    
    // 选择状态
    QSet<SelectedObject> m_selectedObjects;
    
    // 性能优化 - 四叉树空间索引
    std::unique_ptr<QuadTree> m_quadTree;
    bool m_quadTreeDirty = true;         ///< 四叉树需要重建
    int m_lastQueryCount = 0;            ///< 上次查询结果数
    double m_lastQueryTime = 0;          ///< 上次查询耗时(ms)
    
    // 图像缓存
    ImageCache m_imageCache;
    ImageCacheKey m_lastFitAllKey;       ///< 上次 fitAll 的缓存键（标记为 precious）
    
    // 性能优化
    QTimer* m_updateTimer = nullptr;
    bool m_needsFullUpdate = true;
    
    // ========== Cell 渲染缓存（参考 KLayout）==========
    std::unique_ptr<CellRenderCache> m_cellRenderCache;
};

} // namespace QLayoutEDA