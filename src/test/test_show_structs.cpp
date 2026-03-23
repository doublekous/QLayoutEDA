/**
 * @file test_show_structs.cpp
 * @brief 显示 GDS 文件结构信息
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include "../include/core/Types.h"
#include "../src/parser/GDSParser.cpp"
#include "../src/parser/GDSFormat.h"

using namespace QLayoutEDA;

QTextStream out(stdout);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = argc > 1 ? argv[1] : "D:\\CMA6-164-4.gds";
    
    out << "\n=== GDS 结构分析 ===\n";
    out << "文件: " << filePath << "\n\n";
    
    GDSParser parser;
    if (!parser.parse(filePath)) {
        out << "解析失败: " << parser.getLastError() << "\n";
        return 1;
    }
    
    auto data = parser.getData();
    if (!data) {
        out << "无数据\n";
        return 1;
    }
    
    out << "库名: " << data->libraryName << "\n";
    out << "顶层结构(topStructure): " << data->topStructure << "\n";
    out << "结构数量: " << data->structures.size() << "\n\n";
    
    out << "结构列表:\n";
    out << "----------------------------------------\n";
    
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        const QString& name = it.key();
        auto structure = it.value();
        
        out << "结构名: " << name << "\n";
        out << "  BOUNDARY: " << structure->boundaries.size() << "\n";
        out << "  PATH: " << structure->paths.size() << "\n";
        out << "  TEXT: " << structure->texts.size() << "\n";
        out << "  SREF: " << structure->references.size() << "\n";
        out << "  AREF: " << structure->arrays.size() << "\n";
        
        // 计算边界框
        if (!structure->boundaries.isEmpty()) {
            qint64 minX = LLONG_MAX, maxX = LLONG_MIN;
            qint64 minY = LLONG_MAX, maxY = LLONG_MIN;
            
            for (const auto& b : structure->boundaries) {
                for (const auto& p : b.points) {
                    minX = qMin(minX, p.x);
                    maxX = qMax(maxX, p.x);
                    minY = qMin(minY, p.y);
                    maxY = qMax(maxY, p.y);
                }
            }
            
            out << "  边界框 X: " << minX << " ~ " << maxX << "\n";
            out << "  边界框 Y: " << minY << " ~ " << maxY << "\n";
            out << "  尺寸: " << (maxX - minX) / 1000.0 << " x " << (maxY - minY) / 1000.0 << " μm\n";
        }
        out << "\n";
    }
    
    return 0;
}