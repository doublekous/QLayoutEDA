/**
 * @file test_real_gds.cpp
 * @brief 使用真实 GDS 文件测试解析器
 */

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include "GDSParser.h"

using namespace QLayoutEDA;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "Starting GDS Parser Test...";
    
    GDSParser parser;
    
    // 测试文件
    QString filePath = "D:/testFile/20250530/test22.gds";
    
    qDebug() << "Testing file:" << filePath;
    
    // 检查文件是否存在
    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "File does not exist!";
        return 1;
    }
    qDebug() << "File exists, size:" << file.size() << "bytes";
    
    QElapsedTimer timer;
    timer.start();
    
    // 解析文件
    bool success = parser.parse(filePath);
    
    qint64 elapsed = timer.elapsed();
    
    qDebug() << "Parse time:" << elapsed << "ms";
    qDebug() << "Success:" << success;
    
    if (success) {
        GDSDataPtr data = parser.getData();
        qDebug() << "Library name:" << data->libraryName;
        qDebug() << "Structures count:" << data->structures.size();
    } else {
        qDebug() << "Error:" << parser.getLastError();
    }
    
    qDebug() << "Test completed.";
    
    return 0;
}