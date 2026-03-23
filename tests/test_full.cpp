/**
 * @file test_full.cpp
 * @brief 完整 GDS 解析测试
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QElapsedTimer>
#include "GDSParser.h"

void testFile(const QString& filePath) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "File: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.exists()) {
        std::cout << "ERROR: File not found!" << std::endl;
        return;
    }
    
    std::cout << "Size: " << (file.size() / 1024.0 / 1024.0) << " MB" << std::endl;
    
    QLayoutEDA::GDSParser parser;
    QElapsedTimer timer;
    timer.start();
    
    bool success = parser.parse(filePath);
    
    std::cout << "Parse time: " << timer.elapsed() << " ms" << std::endl;
    std::cout << "Success: " << (success ? "YES" : "NO") << std::endl;
    
    if (success) {
        auto data = parser.getData();
        std::cout << "\n--- Library Info ---" << std::endl;
        std::cout << "Library name: " << data->libraryName.toStdString() << std::endl;
        std::cout << "DB units/meter: " << data->dbUnitsPerMeter << std::endl;
        std::cout << "User units/meter: " << data->userUnitsPerMeter << std::endl;
        std::cout << "Top structure: " << data->topStructure.toStdString() << std::endl;
        
        std::cout << "\n--- Structures (" << data->structures.size() << ") ---" << std::endl;
        
        int totalBoundaries = 0, totalPaths = 0, totalTexts = 0, totalRefs = 0;
        
        for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
            auto s = it.value();
            totalBoundaries += s->boundaries.size();
            totalPaths += s->paths.size();
            totalTexts += s->texts.size();
            totalRefs += s->references.size() + s->arrays.size();
            
            std::cout << "  [" << s->name.toStdString() << "] "
                      << "B:" << s->boundaries.size() << " "
                      << "P:" << s->paths.size() << " "
                      << "T:" << s->texts.size() << " "
                      << "R:" << s->references.size() << " "
                      << "A:" << s->arrays.size() << std::endl;
        }
        
        std::cout << "\n--- Totals ---" << std::endl;
        std::cout << "Boundaries: " << totalBoundaries << std::endl;
        std::cout << "Paths: " << totalPaths << std::endl;
        std::cout << "Texts: " << totalTexts << std::endl;
        std::cout << "References: " << totalRefs << std::endl;
    } else {
        std::cout << "Error: " << parser.getLastError().toStdString() << std::endl;
    }
}

int main()
{
    std::cout << "=== GDS Parser Full Test ===" << std::endl;
    
    QStringList testFiles = {
        "D:/testFile/20250530/test22.gds",
        "D:/testFile/20250530/bit_cell.gds",
        "D:/testFile/20250530/qc2.gds",
        "D:/testFile/20250530/flip_chip.gds",
        "D:/testFile/20250530/lvboqi3.13_origin_file.gds",
        "D:/CMA6-164-4.gds"
    };
    
    for (const QString& file : testFiles) {
        testFile(file);
    }
    
    return 0;
}