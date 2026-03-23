/**
 * @file test_debug2.cpp
 * @brief 调试解析过程
 */

#include <iostream>
#include <iomanip>
#include <QFile>
#include <QDataStream>

// 从 GDSFormat.h 复制的记录类型
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
    constexpr quint16 LAYER      = 0x0D02;
    constexpr quint16 DATATYPE   = 0x0E02;
    constexpr quint16 XY         = 0x1003;
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
        case GDSRecord::LAYER: return "LAYER";
        case GDSRecord::DATATYPE: return "DATATYPE";
        case GDSRecord::XY: return "XY";
        case GDSRecord::ENDEL: return "ENDEL";
        default: return "UNKNOWN";
    }
}

int main()
{
    QString filePath = "D:/testFile/20250530/test22.gds";
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "Cannot open file" << std::endl;
        return 1;
    }
    
    std::cout << "File size: " << file.size() << " bytes\n" << std::endl;
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    int recordCount = 0;
    int boundaries = 0;
    int structures = 0;
    QString currentStruct;
    
    while (!stream.atEnd()) {
        quint16 recordLength = 0;
        quint16 recordType = 0;
        
        stream >> recordLength;
        stream >> recordType;
        
        int dataSize = recordLength - 4;
        
        const char* name = getRecordName(recordType);
        
        std::cout << "Rec " << std::setw(3) << recordCount 
                  << ": " << std::setw(10) << name 
                  << " (0x" << std::hex << std::setw(4) << std::setfill('0') << recordType << std::dec << ")";
        
        if (recordType == GDSRecord::STRNAME && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            currentStruct = QString::fromLatin1(data);
            std::cout << " -> \"" << currentStruct.toStdString() << "\"";
            structures++;
        } else if (recordType == GDSRecord::BOUNDARY) {
            boundaries++;
            std::cout << " [boundary #" << boundaries << "]";
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        std::cout << std::endl;
        recordCount++;
        
        if (recordCount > 30) break;  // 只显示前30条
    }
    
    file.close();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Total records: " << recordCount << std::endl;
    std::cout << "Structures: " << structures << std::endl;
    std::cout << "Boundaries: " << boundaries << std::endl;
    
    return 0;
}