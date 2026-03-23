/**
 * @file test_sref_render.cpp
 * @brief 测试 SREF/AREF 展开渲染
 */

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QElapsedTimer>
#include "../parser/GDSParser.h"

using namespace QLayoutEDA;

// 递归渲染统计
struct RenderStats {
    int boundaryCount = 0;
    int pathCount = 0;
    int textCount = 0;
    int srefCount = 0;
    int arefCount = 0;
    int srefExpandedCount = 0;
};

void renderCell(QPainter& painter, GDSDataPtr data, const QString& cellName,
                double offsetX, double offsetY, double scale,
                int depth, RenderStats& stats, bool expandSREF, int& srefExpandedCount,
                int maxDepth = 20, int maxSREFExpand = 1000000)  // 提高到 100 万
{
    if (depth > maxDepth) return;
    
    auto structure = data->getStructure(cellName);
    if (!structure) return;
    
    // 渲染 Boundary
    for (const auto& boundary : structure->boundaries) {
        if (boundary.points.size() < 3) continue;
        
        QPolygonF poly;
        for (const auto& pt : boundary.points) {
            double x = pt.x * scale + offsetX;
            double y = -pt.y * scale + offsetY;
            poly << QPointF(x, y);
        }
        
        int hue = (boundary.layer * 137) % 360;
        QColor color = QColor::fromHsl(hue, 180, 160);
        painter.setPen(QPen(color.darker(150), 1));
        painter.setBrush(QBrush(color));
        painter.drawPolygon(poly);
        
        stats.boundaryCount++;
    }
    
    // 渲染 Path
    for (const auto& path : structure->paths) {
        if (path.points.size() < 2) continue;
        
        QPainterPath pp;
        for (int i = 0; i < path.points.size(); i++) {
            double x = path.points[i].x * scale + offsetX;
            double y = -path.points[i].y * scale + offsetY;
            if (i == 0) pp.moveTo(x, y);
            else pp.lineTo(x, y);
        }
        
        int hue = (path.layer * 137) % 360;
        QColor color = QColor::fromHsl(hue, 180, 160);
        painter.setPen(QPen(color, qMax(1.0, (double)path.width * scale)));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(pp);
        
        stats.pathCount++;
    }
    
    // 渲染 SREF
    for (const auto& sref : structure->references) {
        stats.srefCount++;
        
        bool shouldExpand = expandSREF && (srefExpandedCount < maxSREFExpand);
        
        if (shouldExpand) {
            srefExpandedCount++;
            stats.srefExpandedCount++;
            
            double childOffsetX = offsetX + sref.position.x * scale;
            double childOffsetY = offsetY + sref.position.y * scale;
            double childScale = scale * sref.magnification;
            
            renderCell(painter, data, sref.structureName,
                       childOffsetX, childOffsetY, childScale,
                       depth + 1, stats, expandSREF, srefExpandedCount,
                       maxDepth, maxSREFExpand);
        }
    }
    
    // 渲染 AREF
    for (const auto& aref : structure->arrays) {
        stats.arefCount++;
        
        for (int row = 0; row < aref.rows; ++row) {
            for (int col = 0; col < aref.columns; ++col) {
                double instanceX = aref.position.x + col * aref.columnVector.x + row * aref.rowVector.x;
                double instanceY = aref.position.y + col * aref.columnVector.y + row * aref.rowVector.y;
                
                double childOffsetX = offsetX + instanceX * scale;
                double childOffsetY = offsetY + instanceY * scale;
                
                renderCell(painter, data, aref.structureName,
                           childOffsetX, childOffsetY, scale,
                           depth + 1, stats, expandSREF, srefExpandedCount,
                           maxDepth, maxSREFExpand);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString testFile = "D:\\testFile\\20250530\\Topcell_all_layout.gds";
    if (argc > 1) testFile = argv[1];
    
    qDebug() << "=== SREF/AREF 渲染测试 ===";
    qDebug() << "文件:" << testFile;
    
    // 解析 GDS
    GDSParser parser;
    if (!parser.parse(testFile)) {
        qCritical() << "解析失败:" << parser.getLastError();
        return 1;
    }
    
    auto data = parser.getData();
    qDebug() << "结构数:" << data->structures.size();
    
    // 选择要渲染的 Cell
    QString targetCell = "topcell000111";
    if (!data->structures.contains(targetCell)) {
        targetCell = data->structures.keys().first();
    }
    
    auto structure = data->getStructure(targetCell);
    if (!structure) {
        qCritical() << "找不到结构:" << targetCell;
        return 1;
    }
    
    qDebug() << "\n=== 目标 Cell ===";
    qDebug() << "名称:" << targetCell;
    qDebug() << "Boundary:" << structure->boundaries.size();
    qDebug() << "Path:" << structure->paths.size();
    qDebug() << "Text:" << structure->texts.size();
    qDebug() << "SREF:" << structure->references.size();
    qDebug() << "AREF:" << structure->arrays.size();
    
    // 计算边界框
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
    qDebug() << "边界框:" << dbW << "x" << dbH << "数据库单位";
    
    // 创建图像
    int imgW = 2000, imgH = 1500;
    QImage image(imgW, imgH, QImage::Format_ARGB32);
    image.fill(QColor(30, 30, 30));
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 计算缩放和偏移
    double baseScale = qMin((imgW - 100.0) / dbW, (imgH - 100.0) / dbH);
    double offsetY = (imgH + dbH * baseScale) / 2.0 + minY * baseScale;
    double offsetX = (imgW - dbW * baseScale) / 2.0 - minX * baseScale;
    
    qDebug() << "\n=== 渲染参数 ===";
    qDebug() << "基础缩放:" << baseScale;
    qDebug() << "偏移 X:" << offsetX << "Y:" << offsetY;
    
    // 渲染（不展开 SREF）
    qDebug() << "\n=== 测试 1: 不展开 SREF ===";
    RenderStats stats1;
    int srefExpanded1 = 0;
    QElapsedTimer timer1;
    timer1.start();
    renderCell(painter, data, targetCell, offsetX, offsetY, baseScale,
               0, stats1, false, srefExpanded1);
    qDebug() << "耗时:" << timer1.elapsed() << "ms";
    qDebug() << "Boundary:" << stats1.boundaryCount;
    qDebug() << "SREF (未展开):" << stats1.srefCount;
    
    // 保存图像
    image.save("E:\\test_sref_no_expand.png");
    qDebug() << "保存: E:\\test_sref_no_expand.png";
    
    // 重新渲染（展开 SREF，使用默认缩放 - scale >= 1.0 即展开）
    qDebug() << "\n=== 测试 2: 展开 SREF (默认缩放，模拟 fitAll) ===";
    qDebug() << "注意：scale = 1.0，应该触发展开 (>= 1.0)";
    image.fill(QColor(30, 30, 30));
    RenderStats stats2;
    int srefExpanded2 = 0;
    QElapsedTimer timer2;
    timer2.start();
    renderCell(painter, data, targetCell, offsetX, offsetY, baseScale,
               0, stats2, true, srefExpanded2);  // expandSREF = true (模拟 scale >= 1.0)
    qDebug() << "耗时:" << timer2.elapsed() << "ms";
    qDebug() << "Boundary:" << stats2.boundaryCount;
    qDebug() << "SREF 展开:" << stats2.srefExpandedCount << "/" << stats2.srefCount;
    qDebug() << "覆盖率:" << QString::number(100.0 * stats2.srefExpandedCount / qMax(1, stats2.srefCount), 'f', 1) << "%";
    
    image.save("E:\\test_sref_expanded.png");
    qDebug() << "保存: E:\\test_sref_expanded.png";
    
    qDebug() << "\n=== 测试完成 ===";
    
    return 0;
}