/**
 * @file test_quadtree.cpp
 * @brief 四叉树测试
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QElapsedTimer>
#include "../src/graphic/QuadTree.h"
#include "../src/parser/GDSParser.cpp"

using namespace QLayoutEDA;

QTextStream out(stdout);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = argc > 1 ? argv[1] : "D:\\CMA6-164-4.gds";
    
    out << "=== 四叉树测试 ===\n";
    out << "文件: " << filePath << "\n";
    
    // 解析文件
    GDSParser parser;
    QElapsedTimer timer;
    timer.start();
    
    if (!parser.parse(filePath)) {
        out << "解析失败: " << parser.getLastError() << "\n";
        return 1;
    }
    
    qint64 parseTime = timer.elapsed();
    out << "解析耗时: " << parseTime << " ms\n";
    
    auto data = parser.getData();
    if (!data || data->structures.isEmpty()) {
        out << "无数据\n";
        return 1;
    }
    
    // 获取第一个结构
    QString structName = data->topStructure.isEmpty() 
        ? data->structures.keys().first() 
        : data->topStructure;
    auto structure = data->getStructure(structName);
    
    out << "结构: " << structName << "\n";
    out << "BOUNDARY: " << structure->boundaries.size() << "\n";
    out << "PATH: " << structure->paths.size() << "\n";
    out << "TEXT: " << structure->texts.size() << "\n";
    
    // 构建四叉树
    out << "\n构建四叉树...\n";
    timer.restart();
    
    QuadTree quadTree;
    quadTree.build(structure->boundaries, structure->paths, structure->texts);
    
    qint64 buildTime = timer.elapsed();
    out << "构建耗时: " << buildTime << " ms\n";
    
    // 查询测试
    QRectF viewport(-100, -100, 200, 200);  // 小视口
    timer.restart();
    
    QVector<ShapeProxy> shapes = quadTree.query(viewport);
    
    qint64 queryTime = timer.elapsed();
    out << "\n查询视口(" << viewport.left() << "," << viewport.top() 
        << " " << viewport.width() << "x" << viewport.height() << "):\n";
    out << "查询耗时: " << queryTime << " ms\n";
    out << "返回图形数: " << shapes.size() << "\n";
    
    // 全范围查询
    QRectF fullViewport(-100000, -100000, 200000, 200000);
    timer.restart();
    
    shapes = quadTree.query(fullViewport);
    
    queryTime = timer.elapsed();
    out << "\n全范围查询耗时: " << queryTime << " ms\n";
    out << "返回图形数: " << shapes.size() << "\n";
    
    return 0;
}