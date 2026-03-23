/**
 * @file test_simple.cpp
 * @brief GDS 解析器测试
 */

#include <iostream>
#include <QElapsedTimer>
#include "GDSParser.h"

int main(int, char**)
{
    std::cout << "=== GDS Parser Test ===\n" << std::endl;
    
    QLayoutEDA::GDSParser parser;
    
    QStringList testFiles = {
        "D:/testFile/20250530/test22.gds",
        "D:/testFile/20250530/bit_cell.gds",
        "D:/testFile/20250530/qc2.gds"
    };
    
    for (const QString& file : testFiles) {
        std::cout << "File: " << file.toStdString() << std::endl;
        
        QElapsedTimer timer;
        timer.start();
        
        bool success = parser.parse(file);
        
        std::cout << "Parse time: " << timer.elapsed() << "ms" << std::endl;
        
        if (success) {
            auto data = parser.getData();
            std::cout << "Library: " << data->libraryName.toStdString() << std::endl;
            std::cout << "Structures: " << data->structures.size() << std::endl;
            
            for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
                auto s = it.value();
                std::cout << "\n  [" << s->name.toStdString() << "]" << std::endl;
                std::cout << "    Boundaries: " << s->boundaries.size() << std::endl;
                std::cout << "    Paths: " << s->paths.size() << std::endl;
                std::cout << "    Texts: " << s->texts.size() << std::endl;
                std::cout << "    References: " << s->references.size() << std::endl;
            }
        } else {
            std::cout << "Error: " << parser.getLastError().toStdString() << std::endl;
        }
        
        std::cout << "\n------------------------\n" << std::endl;
    }
    
    return 0;
}