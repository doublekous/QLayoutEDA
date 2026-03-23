/**
 * @file diagnose_large.cpp
 * @brief 诊断大文件 GDS 解析问题
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>

namespace GDSRecord {
    constexpr quint16 HEADER     = 0x0002;
    constexpr quint16 BGNLIB     = 0x0102;
    constexpr quint16 LIBNAME    = 0x0206;
    constexpr quint16 UNITS      = 0x0305;
    constexpr quint16 ENDLIB     = 0x0400;
    constexpr quint16 BGNSTR     = 0x0502;
    constexpr quint16 STRNAME    = 0x0606;
    constexpr quint16 ENDSTR     = 0x0700;
    constexpr quint16 BOUNDARY   = 0x0800;
    constexpr quint16 PATH       = 0x0900;
    constexpr quint16 SREF       = 0x0A00;
    constexpr quint16 AREF       = 0x0B00;
    constexpr quint16 TEXT       = 0x0C00;
}

const char* getRecordName(quint16 type) {
    switch (type) {
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
        default: return "OTHER";
    }
}

int main(int argc, char* argv[])
{
    QString filePath = argc > 1 ? argv[1] : "D:/CMA6-164-4.gds";
    
    std::cout << "=== Large GDS Diagnosis ===" << std::endl;
    std::cout << "File: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "ERROR: Cannot open file" << std::endl;
        return 1;
    }
    
    qint64 fileSize = file.size();
    std::cout << "Size: " << (fileSize / 1024.0 / 1024.0) << " MB" << std::endl;
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    qint64 recordCount = 0;
    qint64 errorCount = 0;
    qint64 structureCount = 0;
    qint64 boundaryCount = 0;
    qint64 pathCount = 0;
    qint64 srefCount = 0;
    qint64 arefCount = 0;
    qint64 textCount = 0;
    
    QString currentStructure;
    QElapsedTimer timer;
    timer.start();
    qint64 lastReportTime = 0;
    
    while (!stream.atEnd()) {
        quint16 recordLength = 0, recordType = 0;
        qint64 pos = file.pos();
        
        stream >> recordLength >> recordType;
        
        if (stream.status() != QDataStream::Ok) {
            std::cout << "\nERROR at position " << pos << ": Read failed" << std::endl;
            break;
        }
        
        int dataSize = recordLength - 4;
        
        // 检查无效记录
        if (dataSize < 0 || recordLength > 65535 || recordLength < 4) {
            std::cout << "\nERROR at record " << recordCount 
                      << " (pos " << pos << "): Invalid length " << recordLength 
                      << ", type 0x" << std::hex << recordType << std::dec << std::endl;
            errorCount++;
            if (errorCount > 10) {
                std::cout << "Too many errors, stopping" << std::endl;
                break;
            }
            // 尝试恢复：跳过一个字节后重试
            file.seek(pos + 1);
            continue;
        }
        
        // 统计记录类型
        switch (recordType) {
            case GDSRecord::BGNSTR:
                structureCount++;
                currentStructure = QString("structure_%1").arg(structureCount);
                break;
            case GDSRecord::STRNAME:
                if (dataSize > 0) {
                    QByteArray data(dataSize, 0);
                    stream.readRawData(data.data(), dataSize);
                    while (data.endsWith('\0')) data.chop(1);
                    currentStructure = QString::fromLatin1(data);
                    dataSize = 0;  // 已读取
                }
                break;
            case GDSRecord::ENDSTR:
                currentStructure.clear();
                break;
            case GDSRecord::BOUNDARY:
                boundaryCount++;
                break;
            case GDSRecord::PATH:
                pathCount++;
                break;
            case GDSRecord::SREF:
                srefCount++;
                break;
            case GDSRecord::AREF:
                arefCount++;
                break;
            case GDSRecord::TEXT:
                textCount++;
                break;
        }
        
        // 跳过数据
        if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        recordCount++;
        
        // 进度报告
        qint64 elapsed = timer.elapsed();
        if (elapsed - lastReportTime > 5000) {  // 每5秒报告一次
            qint64 progress = pos * 100 / fileSize;
            std::cout << "Progress: " << progress << "%, Records: " << recordCount 
                      << ", Structures: " << structureCount << std::endl;
            lastReportTime = elapsed;
        }
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Parse time: " << timer.elapsed() << " ms" << std::endl;
    std::cout << "Total records: " << recordCount << std::endl;
    std::cout << "Errors: " << errorCount << std::endl;
    std::cout << "\n--- Structure Summary ---" << std::endl;
    std::cout << "Structures (BGNSTR): " << structureCount << std::endl;
    std::cout << "Boundaries: " << boundaryCount << std::endl;
    std::cout << "Paths: " << pathCount << std::endl;
    std::cout << "SREFs: " << srefCount << std::endl;
    std::cout << "AREFs: " << arefCount << std::endl;
    std::cout << "TEXTs: " << textCount << std::endl;
    
    return 0;
}