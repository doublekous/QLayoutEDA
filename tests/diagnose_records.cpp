/**
 * @file diagnose_records.cpp
 * @brief 详细诊断记录序列
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QDataStream>

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
    constexpr quint16 ENDEL      = 0x1100;
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
        case GDSRecord::ENDEL: return "ENDEL";
        default: return nullptr;
    }
}

int main(int argc, char* argv[])
{
    QString filePath = argc > 1 ? argv[1] : "D:/CMA6-164-4.gds";
    
    std::cout << "=== Record Sequence Diagnosis ===" << std::endl;
    std::cout << "File: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "ERROR: Cannot open file" << std::endl;
        return 1;
    }
    
    std::cout << "Size: " << (file.size() / 1024.0 / 1024.0) << " MB" << std::endl << std::endl;
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    qint64 recordCount = 0;
    qint64 bgnstrCount = 0;
    qint64 endstrCount = 0;
    qint64 endlibFound = false;
    
    // 只显示关键记录
    std::cout << "Key records (BGNSTR, STRNAME, ENDSTR, ENDLIB):" << std::endl;
    std::cout << std::setw(10) << "Rec#" << " | "
              << std::setw(12) << "Type" << " | "
              << std::setw(8) << "Data" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    while (!stream.atEnd() && recordCount < 100000) {
        quint16 recordLength = 0, recordType = 0;
        stream >> recordLength >> recordType;
        
        if (stream.status() != QDataStream::Ok) break;
        
        int dataSize = recordLength - 4;
        if (dataSize < 0) {
            std::cout << "ERROR: Invalid length at record " << recordCount << std::endl;
            break;
        }
        
        // 显示关键记录
        const char* name = getRecordName(recordType);
        if (name) {
            std::cout << std::setw(10) << recordCount << " | "
                      << std::setw(12) << name;
            
            if (recordType == GDSRecord::BGNSTR) {
                bgnstrCount++;
                std::cout << " | #" << bgnstrCount;
            } else if (recordType == GDSRecord::ENDSTR) {
                endstrCount++;
                std::cout << " | #" << endstrCount;
            } else if (recordType == GDSRecord::ENDLIB) {
                endlibFound = true;
                std::cout << " | FOUND!";
            } else if (recordType == GDSRecord::STRNAME && dataSize > 0) {
                QByteArray data(dataSize, 0);
                stream.readRawData(data.data(), dataSize);
                while (data.endsWith('\0')) data.chop(1);
                std::cout << " | \"" << data.toStdString() << "\"";
                dataSize = 0;
            } else {
                std::cout << " |";
            }
            
            std::cout << std::endl;
        }
        
        // 跳过数据
        if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        recordCount++;
    }
    
    std::cout << std::string(40, '=') << std::endl;
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Total records: " << recordCount << std::endl;
    std::cout << "  BGNSTR count: " << bgnstrCount << std::endl;
    std::cout << "  ENDSTR count: " << endstrCount << std::endl;
    std::cout << "  ENDLIB found: " << (endlibFound ? "YES" : "NO") << std::endl;
    
    if (bgnstrCount != endstrCount) {
        std::cout << "\nWARNING: BGNSTR/ENDSTR mismatch!" << std::endl;
    }
    
    if (!endlibFound) {
        std::cout << "\nWARNING: ENDLIB not found before EOF!" << std::endl;
    }
    
    return 0;
}