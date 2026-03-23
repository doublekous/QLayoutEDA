/**
 * @file test_cell_hierarchy.cpp
 * @brief 测试 Cell 层级关系分析
 */

#include <QCoreApplication>
#include <QDebug>
#include "../parser/GDSParser.h"

using namespace QLayoutEDA;

void analyzeCellHierarchy(GDSDataPtr data)
{
    if (!data) return;
    
    qDebug() << "\n=== Cell Hierarchy Analysis ===";
    qDebug() << "Total cells:" << data->structures.size();
    
    // 1. 收集所有被引用的 Cell
    QSet<QString> referencedCells;
    QHash<QString, QSet<QString>> parentMap;  // child -> parents
    QHash<QString, QSet<QString>> childMap;   // parent -> children
    
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        auto structure = it.value();
        if (!structure) continue;
        
        for (const auto& ref : structure->references) {
            referencedCells.insert(ref.structureName);
            parentMap[ref.structureName].insert(it.key());
            childMap[it.key()].insert(ref.structureName);
        }
        
        for (const auto& arr : structure->arrays) {
            referencedCells.insert(arr.structureName);
            parentMap[arr.structureName].insert(it.key());
            childMap[it.key()].insert(arr.structureName);
        }
    }
    
    // 2. 找出顶层 Cell（不被任何 Cell 引用）
    QSet<QString> topLevelCells;
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        if (!referencedCells.contains(it.key())) {
            topLevelCells.insert(it.key());
        }
    }
    
    qDebug() << "\n=== Top-level Cells (" << topLevelCells.size() << ") ===";
    for (const QString& name : topLevelCells) {
        qDebug() << "  " << name;
    }
    
    // 3. 特别检查用户关注的四个 Cell
    QStringList focusCells = {
        "Res_bansui_1_ab_Flip_$1",
        "Singlebit_V8_2_GRID_ARRAY(1)$1", 
        "TiNPad&Inpillar(3)",
        "topcell000111"
    };
    
    qDebug() << "\n=== Focus Cells Analysis ===";
    for (const QString& name : focusCells) {
        qDebug() << "\nCell:" << name;
        
        if (!data->structures.contains(name)) {
            qDebug() << "  (NOT FOUND IN GDS)";
            continue;
        }
        
        bool isReferenced = referencedCells.contains(name);
        qDebug() << "  Is referenced:" << isReferenced;
        qDebug() << "  Is top-level:" << !isReferenced;
        
        if (parentMap.contains(name)) {
            qDebug() << "  Referenced by (parents):" << parentMap[name].values();
        }
        
        if (childMap.contains(name)) {
            qDebug() << "  Contains (children):" << childMap[name].values().size() << "cells";
        }
    }
    
    qDebug() << "\n================================";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString testFile = "D:\\testFile\\20250530\\Topcell_all_layout.gds";
    if (argc > 1) testFile = argv[1];
    
    qDebug() << "=== Cell Hierarchy Test ===";
    qDebug() << "File:" << testFile;
    
    GDSParser parser;
    if (!parser.parse(testFile)) {
        qCritical() << "Parse failed:" << parser.getLastError();
        return 1;
    }
    
    analyzeCellHierarchy(parser.getData());
    
    return 0;
}