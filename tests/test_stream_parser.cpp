/**
 * @file test_stream_parser.cpp
 * @brief 流式解析器性能测试
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QElapsedTimer>
#include "GDSStreamParser.h"

void testStreamParser(const QString& filePath) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "File: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.exists()) {
        std::cout << "ERROR: File not found!" << std::endl;
        return;
    }
    
    double sizeMB = file.size() / 1024.0 / 1024.0;
    std::cout << "Size: " << std::fixed << std::setprecision(2) << sizeMB << " MB" << std::endl;
    
    QLayoutEDA::GDSStreamParser parser;
    
    // ============ 阶段1：快速扫描 ============
    std::cout << "\n--- Phase 1: Metadata Scan ---" << std::endl;
    
    QElapsedTimer timer;
    timer.start();
    
    bool success = parser.scanMetadata(filePath);
    
    qint64 scanTime = timer.elapsed();
    
    std::cout << "Scan time: " << scanTime << " ms" << std::endl;
    std::cout << "Success: " << (success ? "YES" : "NO") << std::endl;
    
    if (!success) {
        std::cout << "Error: " << parser.getLastError().toStdString() << std::endl;
        return;
    }
    
    // 显示扫描结果
    QStringList cellNames = parser.getCellNames();
    std::cout << "Cell count: " << cellNames.size() << std::endl;
    std::cout << "Top cell: " << parser.getTopCellName().toStdString() << std::endl;
    
    // 检查性能目标
    bool scanPass = false;
    if (sizeMB < 50) scanPass = scanTime < 500;
    else if (sizeMB < 200) scanPass = scanTime < 2000;
    else scanPass = scanTime < 10000;
    
    std::cout << "\nScan performance: " << (scanPass ? "PASS" : "FAIL") << std::endl;
    
    // ============ 阶段2：按需加载 ============
    std::cout << "\n--- Phase 2: On-Demand Loading ---" << std::endl;
    
    QString topCell = parser.getTopCellName();
    if (!topCell.isEmpty()) {
        timer.restart();
        
        auto cell = parser.loadCell(topCell);
        
        qint64 loadTime = timer.elapsed();
        
        std::cout << "Load cell: " << topCell.toStdString() << std::endl;
        std::cout << "Load time: " << loadTime << " ms" << std::endl;
        std::cout << "Load < 100ms: " << (loadTime < 100 ? "PASS" : "FAIL") << std::endl;
        
        if (cell) {
            std::cout << "Boundaries: " << cell->boundaries.size() << std::endl;
            std::cout << "Paths: " << cell->paths.size() << std::endl;
            std::cout << "References: " << cell->references.size() << std::endl;
            std::cout << "Arrays: " << cell->arrays.size() << std::endl;
        }
    }
    
    // ============ 阶段3：缓存测试 ============
    std::cout << "\n--- Phase 3: Cache Test ---" << std::endl;
    
    std::cout << "Cached cells: " << parser.getCachedCellCount() << std::endl;
    
    if (!topCell.isEmpty()) {
        timer.restart();
        auto cachedCell = parser.loadCell(topCell);  // 第二次加载应该从缓存读取
        qint64 cacheTime = timer.elapsed();
        
        std::cout << "Cache load time: " << cacheTime << " ms" << std::endl;
        std::cout << "Cache hit (< 1ms): " << (cacheTime < 1 ? "PASS" : "FAIL") << std::endl;
    }
    
    // ============ 总结 ============
    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "File size: " << sizeMB << " MB" << std::endl;
    std::cout << "Scan time: " << scanTime << " ms" << std::endl;
    std::cout << "Scan performance: " << (scanPass ? "PASS" : "FAIL") << std::endl;
}

int main()
{
    std::cout << "=== GDS Stream Parser Performance Test ===" << std::endl;
    
    QStringList testFiles = {
        "D:/testFile/20250530/test22.gds",
        "D:/testFile/20250530/bit_cell.gds",
        "D:/testFile/20250530/qc2.gds",
        "D:/testFile/20250530/flip_chip.gds",
        "D:/testFile/20250530/lvboqi3.13_origin_file.gds",
        "D:/testFile/20250530/Topcell_all_layout.gds"
    };
    
    for (const QString& file : testFiles) {
        testStreamParser(file);
    }
    
    return 0;
}