/**
 * @file analyze_hierarchy.cpp
 * @brief 分析 Cell 层级引用关系
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QSet>
#include <QMap>
#include "GDSParser.h"

using namespace QLayoutEDA;

struct CellHierarchy {
    QString name;
    QSet<QString> references;      // 该 Cell 引用的其他 Cell
    QSet<QString> referencedBy;     // 引用该 Cell 的其他 Cell
    int boundaryCount = 0;
    int pathCount = 0;
    int textCount = 0;
};

int main()
{
    QString filePath = "D:/testFile/20250530/Topcell_all_layout.gds";
    
    std::cout << "=== Cell Hierarchy Analysis ===" << std::endl;
    std::cout << "File: " << filePath.toStdString() << "\n" << std::endl;
    
    GDSParser parser;
    if (!parser.parse(filePath)) {
        std::cout << "Parse failed: " << parser.getLastError().toStdString() << std::endl;
        return 1;
    }
    
    auto data = parser.getData();
    
    // 构建层级关系
    QMap<QString, CellHierarchy> hierarchy;
    
    // 收集所有 Cell
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        QString name = it.key();
        auto s = it.value();
        
        CellHierarchy cell;
        cell.name = name;
        cell.boundaryCount = s->boundaries.size();
        cell.pathCount = s->paths.size();
        cell.textCount = s->texts.size();
        
        // 收集引用
        for (const auto& ref : s->references) {
            if (!ref.structureName.isEmpty()) {
                cell.references.insert(ref.structureName);
            }
        }
        for (const auto& aref : s->arrays) {
            if (!aref.structureName.isEmpty()) {
                cell.references.insert(aref.structureName);
            }
        }
        
        hierarchy[name] = cell;
    }
    
    // 构建反向引用关系
    for (auto& cell : hierarchy) {
        for (const QString& ref : cell.references) {
            if (hierarchy.contains(ref)) {
                hierarchy[ref].referencedBy.insert(cell.name);
            }
        }
    }
    
    // 找出顶层 Cell（不被任何 Cell 引用）
    QStringList topCells;
    for (auto it = hierarchy.begin(); it != hierarchy.end(); ++it) {
        if (it->referencedBy.isEmpty()) {
            topCells.append(it.key());
        }
    }
    
    // 输出结果
    std::cout << "=== Top-Level Cells (not referenced by others) ===" << std::endl;
    std::cout << "Count: " << topCells.size() << "\n" << std::endl;
    
    for (const QString& name : topCells) {
        auto& cell = hierarchy[name];
        std::cout << "  [" << name.toStdString() << "]" << std::endl;
        std::cout << "    Boundaries: " << cell.boundaryCount << std::endl;
        std::cout << "    Paths: " << cell.pathCount << std::endl;
        std::cout << "    References: " << cell.references.size() << std::endl;
        
        if (!cell.references.isEmpty()) {
            std::cout << "    References ->" << std::endl;
            for (const QString& ref : cell.references) {
                std::cout << "      - " << ref.toStdString() << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    // 分析用户关心的四个 Cell
    QStringList userCells = {
        "Res_bansui_1_ab_Flip_$1",
        "Singlebit_V8_2_GRID_ARRAY(1)$1",
        "TiNPad&Inpillar(3)",
        "topcell000111"
    };
    
    std::cout << "\n=== User-Specified Cells Analysis ===" << std::endl;
    
    for (const QString& name : userCells) {
        std::cout << "\n[" << name.toStdString() << "]" << std::endl;
        
        if (!hierarchy.contains(name)) {
            std::cout << "  STATUS: NOT FOUND IN GDS!" << std::endl;
            continue;
        }
        
        auto& cell = hierarchy[name];
        
        // 是否是顶层？
        bool isTop = cell.referencedBy.isEmpty();
        std::cout << "  Is top-level: " << (isTop ? "YES" : "NO") << std::endl;
        
        std::cout << "  Boundaries: " << cell.boundaryCount << std::endl;
        std::cout << "  Paths: " << cell.pathCount << std::endl;
        std::cout << "  References: " << cell.references.size() << std::endl;
        
        if (!cell.referencedBy.isEmpty()) {
            std::cout << "  Referenced by:" << std::endl;
            for (const QString& ref : cell.referencedBy) {
                std::cout << "    <- " << ref.toStdString() << std::endl;
            }
        }
        
        if (!cell.references.isEmpty()) {
            std::cout << "  References:" << std::endl;
            for (const QString& ref : cell.references) {
                std::cout << "    -> " << ref.toStdString() << std::endl;
            }
        }
    }
    
    // 分析 top_cell2
    std::cout << "\n=== Current Top Structure: top_cell2 ===" << std::endl;
    if (hierarchy.contains("top_cell2")) {
        auto& cell = hierarchy["top_cell2"];
        std::cout << "  Boundaries: " << cell.boundaryCount << std::endl;
        std::cout << "  References: " << cell.references.size() << std::endl;
        std::cout << "  Referenced by: " << cell.referencedBy.size() << std::endl;
        
        if (!cell.referencedBy.isEmpty()) {
            std::cout << "  Referenced by:" << std::endl;
            for (const QString& ref : cell.referencedBy) {
                std::cout << "    <- " << ref.toStdString() << std::endl;
            }
        }
    }
    
    // 输出完整的引用链
    std::cout << "\n=== Complete Reference Chain ===" << std::endl;
    for (const QString& topCell : topCells) {
        std::cout << "\nFrom [" << topCell.toStdString() << "]:" << std::endl;
        
        // BFS 遍历引用
        QSet<QString> visited;
        QList<QPair<QString, int>> queue;
        queue.append({topCell, 0});
        
        while (!queue.isEmpty()) {
            auto [name, depth] = queue.takeFirst();
            if (visited.contains(name) || !hierarchy.contains(name)) continue;
            visited.insert(name);
            
            QString indent(depth * 2, ' ');
            std::cout << indent.toStdString() << "-> " << name.toStdString() 
                      << " (B:" << hierarchy[name].boundaryCount 
                      << ", R:" << hierarchy[name].references.size() << ")" << std::endl;
            
            for (const QString& ref : hierarchy[name].references) {
                if (!visited.contains(ref)) {
                    queue.append({ref, depth + 1});
                }
            }
        }
    }
    
    return 0;
}