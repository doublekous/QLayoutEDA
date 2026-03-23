/**
 * @file test_render_diagnose.cpp
 * @brief 渲染诊断工具 - 分析 Cell 数据和渲染问题
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QElapsedTimer>
#include "../include/core/Types.h"
#include "../src/parser/GDSParser.cpp"

using namespace QLayoutEDA;

QTextStream out(stdout);

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    
    QString file = argc > 1 ? argv[1] : "D:\\testFile\\20250530\\Topcell_all_layout.gds";
    QString cellName = argc > 2 ? argv[2] : "topcell000111";
    
    out << "\n=== 渲染诊断 ===\n";
    out << "文件: " << file << "\n";
    out << "目标 Cell: " << cellName << "\n\n";
    
    QElapsedTimer timer;
    timer.start();
    
    GDSParser parser;
    if (!parser.parse(file)) {
        out << "解析失败: " << parser.getLastError() << "\n";
        return 1;
    }
    
    qint64 parseTime = timer.elapsed();
    out << "解析耗时: " << parseTime << " ms\n";
    
    auto data = parser.getData();
    if (!data) {
        out << "无数据\n";
        return 1;
    }
    
    out << "结构总数: " << data->structures.size() << "\n";
    
    // 获取目标 Cell
    auto structure = data->getStructure(cellName);
    if (!structure) {
        out << "Cell 不存在: " << cellName << "\n";
        out << "\n可用的 Cell:\n";
        for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
            out << "  - " << it.key() << "\n";
        }
        return 1;
    }
    
    out << "\n=== Cell 数据统计 ===\n";
    out << "Cell: " << cellName << "\n";
    out << "BOUNDARY: " << structure->boundaries.size() << "\n";
    out << "PATH: " << structure->paths.size() << "\n";
    out << "TEXT: " << structure->texts.size() << "\n";
    out << "SREF: " << structure->references.size() << "\n";
    out << "AREF: " << structure->arrays.size() << "\n";
    
    // 计算边界框
    qint64 minX = LLONG_MAX, maxX = LLONG_MIN;
    qint64 minY = LLONG_MAX, maxY = LLONG_MIN;
    qint64 totalPoints = 0;
    
    for (const auto& b : structure->boundaries) {
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x);
            maxX = qMax(maxX, p.x);
            minY = qMin(minY, p.y);
            maxY = qMax(maxY, p.y);
            totalPoints++;
        }
    }
    
    out << "\n=== 边界框 ===\n";
    out << "X: " << minX << " ~ " << maxX << " (" << (maxX - minX) << " db units)\n";
    out << "Y: " << minY << " ~ " << maxY << " (" << (maxY - minY) << " db units)\n";
    
    double widthUm = (maxX - minX) / 1000.0;
    double heightUm = (maxY - minY) / 1000.0;
    out << "尺寸: " << widthUm << " x " << heightUm << " μm\n";
    out << "尺寸: " << (widthUm / 1000.0) << " x " << (heightUm / 1000.0) << " mm\n";
    
    out << "\n=== 渲染预估 ===\n";
    out << "总顶点数: " << totalPoints << "\n";
    
    // 假设画布大小
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    
    double dbW = maxX - minX;
    double dbH = maxY - minY;
    double baseScale = qMin((canvasWidth - 100.0) / dbW, (canvasHeight - 100.0) / dbH);
    
    out << "假设画布: " << canvasWidth << " x " << canvasHeight << "\n";
    out << "基础缩放: " << baseScale << "\n";
    out << "每像素对应: " << (1.0 / baseScale) << " db units (" << (1.0 / baseScale / 1000.0) << " μm)\n";
    
    // 分析图形分布
    out << "\n=== 图形分布分析 ===\n";
    
    // 按图层统计
    QHash<qint32, int> layerCounts;
    for (const auto& b : structure->boundaries) {
        layerCounts[b.layer]++;
    }
    
    out << "图层分布 (前10个):\n";
    QList<qint32> layers = layerCounts.keys();
    std::sort(layers.begin(), layers.end());
    int count = 0;
    for (qint32 layer : layers) {
        out << "  Layer " << layer << ": " << layerCounts[layer] << " 个\n";
        if (++count >= 10) break;
    }
    
    // 检查是否有异常大的图形
    out << "\n=== 图形大小分析 ===\n";
    int hugePolygons = 0;
    int smallPolygons = 0;
    qint64 maxVertices = 0;
    
    for (const auto& b : structure->boundaries) {
        if (b.points.size() > 10000) hugePolygons++;
        if (b.points.size() < 4) smallPolygons++;
        maxVertices = qMax(maxVertices, static_cast<qint64>(b.points.size()));
    }
    
    out << "超大图形 (>10000 顶点): " << hugePolygons << " 个\n";
    out << "异常图形 (<4 顶点): " << smallPolygons << " 个\n";
    out << "最大顶点数: " << maxVertices << "\n";
    
    out << "\n=== 结论 ===\n";
    if (totalPoints > 10000000) {
        out << "[!] 警告: 顶点数超过 1000 万，渲染可能有性能问题\n";
    }
    if (hugePolygons > 100) {
        out << "[!] 警告: 超大图形数量过多，可能导致渲染缓慢\n";
    }
    
    return 0;
}