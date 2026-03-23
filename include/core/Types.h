/**
 * @file Types.h
 * @brief 核心类型定义
 * @author QLayoutEDA Team
 */

#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <QHash>
#include <QVariant>
#include <memory>

namespace QLayoutEDA {

//=============================================================================
// 基础类型
//=============================================================================

/**
 * @brief LOD 细节层次
 */
enum class LODLevel {
    BoundingBox,    ///< 极度缩小：只画边界框
    Low,            ///< 低细节
    Medium,         ///< 中等细节
    Simplified,     ///< 简化顶点
    Full            ///< 完整渲染
};

/**
 * @brief 数据库坐标类型（64位整数，纳米级精度）
 */
using Coord = qint64;

/**
 * @brief 数据库点（64位整数坐标）
 */
struct DbPoint {
    Coord x;
    Coord y;
    
    DbPoint() : x(0), y(0) {}
    DbPoint(Coord x_, Coord y_) : x(x_), y(y_) {}
    
    bool operator==(const DbPoint& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const DbPoint& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 数据库矩形
 */
struct DbRect {
    Coord left;
    Coord top;
    Coord right;
    Coord bottom;
    
    DbRect() : left(0), top(0), right(0), bottom(0) {}
    DbRect(Coord l, Coord t, Coord r, Coord b) 
        : left(l), top(t), right(r), bottom(b) {}
    
    Coord width() const { return right - left; }
    Coord height() const { return bottom - top; }
    bool isValid() const { return left <= right && top <= bottom; }
    
    bool contains(const DbPoint& p) const {
        return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
    }
};

//=============================================================================
// GDS 数据结构
//=============================================================================

/**
 * @brief GDS 边界元素
 */
struct GDSBoundary {
    int layer;                  ///< 层号
    int dataType;               ///< 数据类型
    QVector<DbPoint> points;    ///< 顶点坐标
    
    GDSBoundary() : layer(0), dataType(0) {}
};

/**
 * @brief GDS 路径元素
 */
struct GDSPath {
    int layer;                  ///< 层号
    int pathType;               ///< 路径类型 (0=flush, 1=round, 2=square)
    Coord width;                ///< 宽度
    QVector<DbPoint> points;    ///< 路径点
    
    GDSPath() : layer(0), pathType(0), width(0) {}
};

/**
 * @brief GDS 文本元素
 */
struct GDSText {
    int layer;                  ///< 层号
    int textType;               ///< 文本类型
    DbPoint position;           ///< 位置
    QString text;               ///< 文本内容
    double angle;               ///< 旋转角度
    double magnification;       ///< 缩放倍数
    
    GDSText() : layer(0), textType(0), angle(0), magnification(1.0) {}
};

/**
 * @brief GDS 引用元素
 */
struct GDSReference {
    QString structureName;      ///< 引用的结构名
    DbPoint position;           ///< 位置
    double angle;               ///< 旋转角度
    double magnification;       ///< 缩放倍数
    bool reflected;             ///< 是否镜像
    
    GDSReference() : angle(0), magnification(1.0), reflected(false) {}
};

/**
 * @brief GDS 阵列引用元素
 */
struct GDSArrayReference {
    QString structureName;      ///< 引用的结构名
    DbPoint position;           ///< 基准位置
    int columns;                ///< 列数
    int rows;                   ///< 行数
    DbPoint columnVector;       ///< 列方向向量
    DbPoint rowVector;          ///< 行方向向量
    
    GDSArrayReference() : columns(1), rows(1) {}
};

/**
 * @brief GDS 结构（单元）
 */
struct GDSStructure {
    QString name;                           ///< 结构名
    QVector<GDSBoundary> boundaries;        ///< 边界元素
    QVector<GDSPath> paths;                 ///< 路径元素
    QVector<GDSText> texts;                 ///< 文本元素
    QVector<GDSReference> references;       ///< 引用元素
    QVector<GDSArrayReference> arrays;      ///< 阵列引用
    
    DbRect boundingBox;                     ///< 边界框
    
    GDSStructure() = default;
};

/**
 * @brief GDS 库数据
 */
struct GDSData {
    QString libraryName;                    ///< 库名
    QString libraryPath;                    ///< 文件路径
    QString version;                        ///< GDS 版本
    double dbUnitsPerMeter;                 ///< 数据库单位/米
    double userUnitsPerMeter;               ///< 用户单位/米
    
    QHash<QString, std::shared_ptr<GDSStructure>> structures;  ///< 结构集合
    QString topStructure;                   ///< 顶层结构名
    
    GDSData() : dbUnitsPerMeter(1e-9), userUnitsPerMeter(1e-6) {}
    
    std::shared_ptr<GDSStructure> getStructure(const QString& name) const {
        return structures.value(name, nullptr);
    }
};

using GDSDataPtr = std::shared_ptr<GDSData>;
using GDSStructurePtr = std::shared_ptr<GDSStructure>;

//=============================================================================
// 图层相关
//=============================================================================

/**
 * @brief 图层样式
 */
struct LayerStyle {
    int layer;              ///< 层号
    QColor fillColor;       ///< 填充颜色
    QColor borderColor;     ///< 边框颜色
    int borderWidth;        ///< 边框宽度
    bool visible;           ///< 是否可见
    bool selectable;        ///< 是否可选
    QString fillPattern;    ///< 填充图案名
    
    LayerStyle() 
        : layer(0)
        , borderWidth(1)
        , visible(true)
        , selectable(true) 
    {}
};

/**
 * @brief 图层数据
 */
struct LayerData {
    int layer;                  ///< 层号
    QString name;               ///< 图层名
    LayerStyle style;           ///< 样式
    int objectCount;            ///< 对象数量
    
    LayerData() : layer(0), objectCount(0) {}
};

//=============================================================================
// 渲染相关
//=============================================================================

/**
 * @brief 视图变换
 */
struct ViewTransform {
    double scale;           ///< 缩放比例
    double offsetX;         ///< X 偏移
    double offsetY;         ///< Y 偏移
    
    ViewTransform() : scale(1.0), offsetX(0), offsetY(0) {}
};

/**
 * @brief 渲染配置
 */
struct RenderConfig {
    bool antialiasing;      ///< 抗锯齿
    bool showGrid;          ///< 显示网格
    bool showRulers;        ///< 显示标尺
    double gridSpacing;     ///< 网格间距
    QColor backgroundColor; ///< 背景色
    QColor gridColor;       ///< 网格色
    
    RenderConfig() 
        : antialiasing(true)
        , showGrid(true)
        , showRulers(true)
        , gridSpacing(100.0)
        , backgroundColor(30, 30, 30)
        , gridColor(60, 60, 60)
    {}
};

//=============================================================================
// 常量定义
//=============================================================================

namespace Constants {
    constexpr Coord kMinCoord = LLONG_MIN / 2;
    constexpr Coord kMaxCoord = LLONG_MAX / 2;
    constexpr int kMaxLayers = 256;
    constexpr int kMaxStructureDepth = 32;
    constexpr double kMinScale = 1e-6;
    constexpr double kMaxScale = 1e6;
    constexpr double kDefaultScale = 1.0;
}

} // namespace QLayoutEDA

// 注册 Qt 元类型
Q_DECLARE_METATYPE(QLayoutEDA::LayerData)
Q_DECLARE_METATYPE(QLayoutEDA::LayerStyle)