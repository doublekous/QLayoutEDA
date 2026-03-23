/**
 * @file test_refs.cpp
 * @brief 检查引用层级解析
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QSet>
#include "GDSParser.h"

int main()
{
    QString filePath = "D:/testFile/20250530/Topcell_all_layout.gds";
    
    std::cout << "=== Reference Hierarchy Check ===" << std::endl;
    std::cout << "\nFile: " << filePath.toStdString() << std::endl;
    
    QLayoutEDA::GDSParser parser;
    if (!parser.parse(filePath)) {
        std::cout << "Parse failed: " << parser.getLastError().toStdString() << std::endl;
        return 1;
    }
    
    auto data = parser.getData();
    
    std::cout << "\n=== Library Info ===" << std::endl;
    std::cout << "Library name: " << data->libraryName.toStdString() << std::endl;
    std::cout << "Total structures: " << data->structures.size() << std::endl;
    std::cout << "Top structure: " << data->topStructure.toStdString() << std::endl;
    
    // 检查顶层结构
    auto topStruct = data->getStructure(data->topStructure);
    if (!topStruct) {
        std::cout << "\nERROR: Top structure not found!" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Top Structure Details ===" << std::endl;
    std::cout << "Name: " << topStruct->name.toStdString() << std::endl;
    std::cout << "Boundaries: " << topStruct->boundaries.size() << std::endl;
    std::cout << "Paths: " << topStruct->paths.size() << std::endl;
    std::cout << "Texts: " << topStruct->texts.size() << std::endl;
    std::cout << "References (SREF): " << topStruct->references.size() << std::endl;
    std::cout << "Arrays (AREF): " << topStruct->arrays.size() << std::endl;
    
    // 统计顶层引用的目标结构
    std::cout << "\n=== Referenced Structures from Top ===" << std::endl;
    QSet<QString> referencedNames;
    
    for (const auto& ref : topStruct->references) {
        referencedNames.insert(ref.structureName);
    }
    for (const auto& aref : topStruct->arrays) {
        referencedNames.insert(aref.structureName);
    }
    
    std::cout << "Unique referenced structures: " << referencedNames.size() << std::endl;
    
    // 检查每个引用的目标结构是否存在
    int missingCount = 0;
    std::cout << "\nReferenced structure check:" << std::endl;
    
    for (const QString& name : referencedNames) {
        bool exists = data->structures.contains(name);
        std::cout << "  " << name.toStdString() << ": " << (exists ? "EXISTS" : "MISSING!") << std::endl;
        if (!exists) missingCount++;
    }
    
    // 打印所有结构名称
    std::cout << "\n=== All Structures in Library ===" << std::endl;
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        auto s = it.value();
        std::cout << "  [" << s->name.toStdString() << "] "
                  << "B:" << s->boundaries.size() << " "
                  << "P:" << s->paths.size() << " "
                  << "T:" << s->texts.size() << " "
                  << "R:" << s->references.size() << " "
                  << "A:" << s->arrays.size() << std::endl;
    }
    
    // 总结
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Top structure: " << data->topStructure.toStdString() << std::endl;
    std::cout << "Direct references from top: " << (topStruct->references.size() + topStruct->arrays.size()) << std::endl;
    std::cout << "Unique referenced structures: " << referencedNames.size() << std::endl;
    std::cout << "Missing structures: " << missingCount << std::endl;
    
    if (missingCount > 0) {
        std::cout << "\nWARNING: Some referenced structures are missing!" << std::endl;
    }
    
    return 0;
}