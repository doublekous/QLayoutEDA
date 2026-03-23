/**
 * @file test_deep_diagnose.cpp
 * @brief GDS 深度诊断工具 - 详细分析解析过程
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QElapsedTimer>
#include <QSet>
#include <iostream>
#include <fstream>

#include "../../include/core/Types.h"
#include "../../src/parser/GDSFormat.h"

// 输出到文件和控制台
std::ofstream logFile;
void log(const QString& msg) {
    std::cout << msg.toStdString() << std::endl;
    if (logFile.is_open()) {
        logFile << msg.toStdString() << std::endl;
    }
}

using namespace QLayoutEDA;

// GDS 记录类型名称映射
QString recordTypeName(quint16 recType) {
    switch (recType) {
        case GDSRecord::HEADER: return "HEADER";
        case GDSRecord::BGNLIB: return "BGNLIB";
        case GDSRecord::LIBNAME: return "LIBNAME";
        case GDSRecord::UNITS: return "UNITS";
        case GDSRecord::ENDLIB: return "ENDLIB";
        case GDSRecord::BGNSTR: return "BGNSTR";
        case GDSRecord::STRNAME: return "STRNAME";
        case GDSRecord::ENDSTR: return "ENDSTR";
        case GDSRecord::BOUNDARY: return "BOUNDARY";
        case GDSRecord::PATH: return "PATH";
        case GDSRecord::SREF: return "SREF";
        case GDSRecord::AREF: return "AREF";
        case GDSRecord::TEXT: return "TEXT";
        case GDSRecord::BOX: return "BOX";
        case GDSRecord::NODE: return "NODE";
        case GDSRecord::LAYER: return "LAYER";
        case GDSRecord::DATATYPE: return "DATATYPE";
        case GDSRecord::WIDTH: return "WIDTH";
        case GDSRecord::XY: return "XY";
        case GDSRecord::ENDEL: return "ENDEL";
        case GDSRecord::SNAME: return "SNAME";
        case GDSRecord::STRANS: return "STRANS";
        case GDSRecord::MAG: return "MAG";
        case GDSRecord::ANGLE: return "ANGLE";
        case GDSRecord::COLROW: return "COLROW";
        case GDSRecord::TEXTTYPE: return "TEXTTYPE";
        case GDSRecord::PRESENTATION: return "PRESENTATION";
        case GDSRecord::STRING: return "STRING";
        case GDSRecord::PATHTYPE: return "PATHTYPE";
        case GDSRecord::PROPATTR: return "PROPATTR";
        case GDSRecord::PROPVALUE: return "PROPVALUE";
        default: return QString("UNKNOWN(0x%1)").arg(recType, 4, 16, QChar('0'));
    }
}

/**
 * @brief 深度分析 GDS 文件
 */
void deepAnalyzeGDS(const QString& filePath, bool verbose = false) {
    qDebug() << "\n========================================";
    qDebug() << "=== GDS 深度诊断分析 ===";
    qDebug() << "========================================\n";
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件:" << filePath;
        return;
    }
    
    qint64 fileSize = file.size();
    qDebug() << "文件路径:" << filePath;
    qDebug() << "文件大小:" << fileSize << "bytes (" 
             << QString::number(fileSize / 1024.0 / 1024.0, 'f', 2) << "MB)";
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    // 统计
    QHash<quint16, qint64> recordCounts;
    QHash<QString, qint64> structureStats;
    QString currentStructure;
    qint64 recordCount = 0;
    qint64 errorCount = 0;
    qint64 lastProgress = 0;
    
    QElapsedTimer timer;
    timer.start();
    
    qDebug() << "\n--- 开始逐记录分析 ---\n";
    
    while (!stream.atEnd()) {
        qint64 currentPos = file.pos();
        
        // 进度报告
        qint64 progress = currentPos * 100 / fileSize;
        if (progress != lastProgress && progress % 10 == 0) {
            qDebug() << QString("进度: %1% (%2/%3 MB)")
                        .arg(progress)
                        .arg(currentPos / 1024 / 1024)
                        .arg(fileSize / 1024 / 1024);
            lastProgress = progress;
        }
        
        quint16 recordLength = 0, recordType = 0;
        stream >> recordLength >> recordType;
        
        if (stream.status() != QDataStream::Ok) {
            if (stream.atEnd()) {
                qDebug() << "\n到达文件末尾";
                break;
            }
            qWarning() << QString("读取错误在位置 %1").arg(currentPos);
            errorCount++;
            break;
        }
        
        int dataSize = recordLength - 4;
        
        // 检查无效记录长度
        if (dataSize < 0) {
            qWarning() << QString("\n❌ 无效记录长度: %1 在位置 %2")
                          .arg(recordLength).arg(currentPos);
            qWarning() << QString("   记录类型: 0x%1 (%2)")
                          .arg(recordType, 4, 16, QChar('0'))
                          .arg(recordTypeName(recordType));
            errorCount++;
            break;
        }
        
        // 检查超大记录
        if (dataSize > 65532) {
            qWarning() << QString("\n⚠️ 异常大记录: %1 bytes 在位置 %2")
                          .arg(dataSize).arg(currentPos);
        }
        
        // 记录统计
        recordCounts[recordType]++;
        recordCount++;
        
        // 详细输出（前100条和关键记录）
        if (verbose || recordCount <= 100 || 
            recordType == GDSRecord::BGNSTR || 
            recordType == GDSRecord::ENDSTR ||
            recordType == GDSRecord::ENDLIB) {
            
            QString typeName = recordTypeName(recordType);
            qDebug() << QString("  [%1] %2: len=%3, data=%4")
                        .arg(recordCount, 8)
                        .arg(typeName, -12)
                        .arg(recordLength)
                        .arg(dataSize);
        }
        
        // 处理特定记录
        switch (recordType) {
            case GDSRecord::BGNSTR:
                // 结构开始
                break;
                
            case GDSRecord::STRNAME: {
                QByteArray data(dataSize, 0);
                stream.readRawData(data.data(), dataSize);
                while (data.endsWith('\0')) data.chop(1);
                currentStructure = QString::fromLatin1(data);
                structureStats[currentStructure] = 0;
                qDebug() << QString("\n📌 结构: %1").arg(currentStructure);
                break;
            }
            
            case GDSRecord::ENDSTR: {
                qDebug() << QString("   结构 %1 包含 %2 个元素")
                            .arg(currentStructure)
                            .arg(structureStats[currentStructure]);
                currentStructure.clear();
                break;
            }
            
            case GDSRecord::BOUNDARY:
            case GDSRecord::PATH:
            case GDSRecord::SREF:
            case GDSRecord::AREF:
            case GDSRecord::TEXT:
                if (!currentStructure.isEmpty()) {
                    structureStats[currentStructure]++;
                }
                break;
            
            case GDSRecord::ENDLIB:
                qDebug() << "\n✅ 到达 ENDLIB";
                break;
                
            default:
                // 跳过数据
                if (dataSize > 0) {
                    stream.skipRawData(dataSize);
                }
                break;
        }
        
        // 安全检查：防止无限循环
        if (recordCount > 100000000) {
            qWarning() << "\n⚠️ 记录数超过 1 亿，可能文件损坏";
            break;
        }
    }
    
    file.close();
    
    qint64 elapsed = timer.elapsed();
    
    // 输出统计结果
    qDebug() << "\n========================================";
    qDebug() << "=== 分析统计 ===";
    qDebug() << "========================================";
    
    qDebug() << "\n解析耗时:" << elapsed << "ms";
    qDebug() << "总记录数:" << recordCount;
    qDebug() << "错误数:" << errorCount;
    
    qDebug() << "\n记录类型分布:";
    QList<quint16> types = recordCounts.keys();
    std::sort(types.begin(), types.end());
    for (quint16 type : types) {
        qDebug() << QString("  %1: %2")
                    .arg(recordTypeName(type), -12)
                    .arg(recordCounts[type]);
    }
    
    qDebug() << "\n结构元素统计:";
    QList<QString> structs = structureStats.keys();
    std::sort(structs.begin(), structs.end());
    for (const QString& s : structs) {
        qDebug() << QString("  %1: %2 个元素").arg(s).arg(structureStats[s]);
    }
    
    if (errorCount > 0) {
        qDebug() << "\n❌ 发现解析错误，请检查文件完整性";
    } else {
        qDebug() << "\n✅ 文件解析正常";
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        QString arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            filePath = arg;
        }
    }
    
    if (filePath.isEmpty()) {
        filePath = "D:\\CMA6-164-4.gds";
    }
    
    deepAnalyzeGDS(filePath, verbose);
    
    return 0;
}