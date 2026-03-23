/**
 * @file test_ref_detail.cpp
 * @brief 检查引用解析细节
 */

#include <iostream>
#include <iomanip>
#include "GDSParser.h"

int main()
{
    QString filePath = "D:/testFile/20250530/Topcell_all_layout.gds";
    
    QLayoutEDA::GDSParser parser;
    if (!parser.parse(filePath)) {
        std::cout << "Parse failed: " << parser.getLastError().toStdString() << std::endl;
        return 1;
    }
    
    auto data = parser.getData();
    
    // 找一个有引用的结构
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        auto s = it.value();
        if (s->references.size() > 0 || s->arrays.size() > 0) {
            std::cout << "\n=== Structure with References ===" << std::endl;
            std::cout << "Name: " << s->name.toStdString() << std::endl;
            std::cout << "SREF count: " << s->references.size() << std::endl;
            std::cout << "AREF count: " << s->arrays.size() << std::endl;
            
            // 打印前5个 SREF
            std::cout << "\nFirst 5 SREF details:" << std::endl;
            int count = 0;
            for (const auto& ref : s->references) {
                if (count++ >= 5) break;
                std::cout << "  SREF[" << count << "]:" << std::endl;
                std::cout << "    Target: " << ref.structureName.toStdString() << std::endl;
                std::cout << "    Position: (" << ref.position.x << ", " << ref.position.y << ")" << std::endl;
                std::cout << "    Angle: " << ref.angle << std::endl;
                std::cout << "    Mag: " << ref.magnification << std::endl;
                std::cout << "    Reflected: " << (ref.reflected ? "YES" : "NO") << std::endl;
                
                // 检查目标是否存在
                bool exists = data->structures.contains(ref.structureName);
                std::cout << "    Target exists: " << (exists ? "YES" : "NO") << std::endl;
            }
            
            // 打印前5个 AREF
            if (!s->arrays.isEmpty()) {
                std::cout << "\nFirst 5 AREF details:" << std::endl;
                count = 0;
                for (const auto& aref : s->arrays) {
                    if (count++ >= 5) break;
                    std::cout << "  AREF[" << count << "]:" << std::endl;
                    std::cout << "    Target: " << aref.structureName.toStdString() << std::endl;
                    std::cout << "    Position: (" << aref.position.x << ", " << aref.position.y << ")" << std::endl;
                    std::cout << "    Columns: " << aref.columns << ", Rows: " << aref.rows << std::endl;
                    
                    bool exists = data->structures.contains(aref.structureName);
                    std::cout << "    Target exists: " << (exists ? "YES" : "NO") << std::endl;
                }
            }
            
            break;  // 只显示第一个有引用的结构
        }
    }
    
    return 0;
}