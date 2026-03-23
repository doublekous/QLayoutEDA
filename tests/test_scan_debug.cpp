/**
 * @file test_scan_debug.cpp
 * @brief 调试流式扫描
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
}

int main()
{
    QString filePath = "D:/testFile/20250530/test22.gds";
    
    std::cout << "=== Scan Debug ===" << std::endl;
    std::cout << "File: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "Cannot open file" << std::endl;
        return 1;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    int recordCount = 0;
    int bgnstrCount = 0;
    int strnameCount = 0;
    int endstrCount = 0;
    int foundCells = 0;
    
    while (!stream.atEnd() && recordCount < 100) {
        quint16 recordLength = 0, recordType = 0;
        stream >> recordLength >> recordType;
        
        int dataSize = recordLength - 4;
        
        const char* name = "OTHER";
        switch (recordType) {
            case GDSRecord::BGNLIB: name = "BGNLIB"; break;
            case GDSRecord::LIBNAME: name = "LIBNAME"; break;
            case GDSRecord::UNITS: name = "UNITS"; break;
            case GDSRecord::BGNSTR: name = "BGNSTR"; bgnstrCount++; break;
            case GDSRecord::STRNAME: name = "STRNAME"; strnameCount++; break;
            case GDSRecord::ENDSTR: name = "ENDSTR"; endstrCount++; break;
            case GDSRecord::ENDLIB: name = "ENDLIB"; break;
            case GDSRecord::BOUNDARY: name = "BOUNDARY"; break;
        }
        
        std::cout << "Rec " << std::setw(3) << recordCount 
                  << ": " << std::setw(10) << name 
                  << " len=" << std::setw(4) << recordLength;
        
        if (recordType == GDSRecord::STRNAME && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            std::cout << " name=\"" << data.toStdString() << "\"";
            foundCells++;
            dataSize = 0;
        } else if (recordType == GDSRecord::LIBNAME && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            std::cout << " lib=\"" << data.toStdString() << "\"";
            dataSize = 0;
        }
        
        if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        std::cout << std::endl;
        recordCount++;
    }
    
    std::cout << "\nSummary:" << std::endl;
    std::cout << "BGNSTR: " << bgnstrCount << std::endl;
    std::cout << "STRNAME: " << strnameCount << std::endl;
    std::cout << "ENDSTR: " << endstrCount << std::endl;
    std::cout << "Found cells: " << foundCells << std::endl;
    
    return 0;
}