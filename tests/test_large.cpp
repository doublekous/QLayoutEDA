/**
 * @file test_large.cpp
 * @brief 大文件 GDS 解析测试
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QElapsedTimer>
#include "GDSParser.h"

int main()
{
    QString filePath = "D:/testFile/20250530/Topcell_all_layout.gds";
    
    std::cout << "=== Large File GDS Parser Test ===" << std::endl;
    std::cout << "\nFile: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.exists()) {
        std::cout << "ERROR: File not found!" << std::endl;
        return 1;
    }
    
    double sizeMB = file.size() / 1024.0 / 1024.0;
    std::cout << "Size: " << std::fixed << std::setprecision(2) << sizeMB << " MB" << std::endl;
    
    QLayoutEDA::GDSParser parser;
    
    // 设置进度回调
    parser.setProgressCallback([](int progress) {
        if (progress % 10 == 0) {
            std::cout << "Progress: " << progress << "%" << std::endl;
        }
    });
    
    std::cout << "\nStarting parse..." << std::endl;
    
    QElapsedTimer timer;
    timer.start();
    
    bool success = parser.parse(filePath);
    
    qint64 elapsed = timer.elapsed();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Parse time: " << elapsed << " ms (" << (elapsed / 1000.0) << " seconds)" << std::endl;
    std::cout << "Success: " << (success ? "YES" : "NO") << std::endl;
    
    if (success) {
        auto data = parser.getData();
        std::cout << "\n--- Library Info ---" << std::endl;
        std::cout << "Library name: " << data->libraryName.toStdString() << std::endl;
        std::cout << "Structures: " << data->structures.size() << std::endl;
        std::cout << "Top structure: " << data->topStructure.toStdString() << std::endl;
        
        int totalBoundaries = 0, totalPaths = 0, totalTexts = 0, totalRefs = 0;
        
        for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
            auto s = it.value();
            totalBoundaries += s->boundaries.size();
            totalPaths += s->paths.size();
            totalTexts += s->texts.size();
            totalRefs += s->references.size() + s->arrays.size();
        }
        
        std::cout << "\n--- Totals ---" << std::endl;
        std::cout << "Boundaries: " << totalBoundaries << std::endl;
        std::cout << "Paths: " << totalPaths << std::endl;
        std::cout << "Texts: " << totalTexts << std::endl;
        std::cout << "References: " << totalRefs << std::endl;
        
        // 检查性能目标
        std::cout << "\n--- Performance Check ---" << std::endl;
        std::cout << "Parse time < 10s: " << (elapsed < 10000 ? "PASS" : "FAIL") << std::endl;
    } else {
        std::cout << "Error: " << parser.getLastError().toStdString() << std::endl;
    }
    
    return 0;
}