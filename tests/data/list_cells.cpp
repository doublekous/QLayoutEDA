/**
 * @file list_cells.cpp
 * @brief 列出 GDS 文件中的所有 Cell/结构名称
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QByteArray>

// GDS 记录类型 (两字节: recType + dataType)
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
    constexpr quint16 LAYER      = 0x0D02;
    constexpr quint16 DATATYPE   = 0x0E02;
    constexpr quint16 WIDTH      = 0x0F03;
    constexpr quint16 XY         = 0x1003;
    constexpr quint16 ENDEL      = 0x1100;
    constexpr quint16 SNAME      = 0x1206;
}

QString readString(QDataStream& stream, int size) {
    QByteArray data(size, 0);
    stream.readRawData(data.data(), size);
    while (data.endsWith('\0')) data.chop(1);
    return QString::fromLatin1(data);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        QTextStream(stderr) << "Usage: list_cells <gds_file>" << Qt::endl;
        return 1;
    }
    
    QString filePath = argv[1];
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Cannot open file:" << filePath << Qt::endl;
        return 1;
    }
    
    QTextStream out(stdout);
    out << "=== GDS Cell List ===" << Qt::endl;
    out << "File:" << filePath << Qt::endl;
    out << "Size:" << file.size() << "bytes" << Qt::endl;
    out << Qt::endl;
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    QStringList cellNames;
    QString currentCell;
    int boundaryCount = 0;
    int pathCount = 0;
    int textCount = 0;
    int srefCount = 0;
    int arefCount = 0;
    int totalCells = 0;
    
    while (!stream.atEnd()) {
        // 读取记录长度 (2字节)
        quint16 recordLength = 0;
        stream >> recordLength;
        
        if (stream.status() != QDataStream::Ok) break;
        
        // 读取记录类型 (2字节: recType + dataType)
        quint16 recType = 0;
        stream >> recType;
        
        int dataSize = recordLength - 4;
        if (dataSize < 0) break;
        
        switch (recType) {
            case GDSRecord::BGNSTR:
                // 新结构开始
                currentCell.clear();
                boundaryCount = 0;
                pathCount = 0;
                textCount = 0;
                srefCount = 0;
                arefCount = 0;
                stream.skipRawData(dataSize);
                break;
                
            case GDSRecord::STRNAME:
                // 结构名称
                currentCell = readString(stream, dataSize);
                cellNames.append(currentCell);
                totalCells++;
                break;
                
            case GDSRecord::ENDSTR:
                // 结构结束
                out << QString("  [%1] Cell: %2").arg(totalCells).arg(currentCell) << Qt::endl;
                if (boundaryCount > 0) out << QString("    - Boundaries: %1").arg(boundaryCount) << Qt::endl;
                if (pathCount > 0) out << QString("    - Paths: %1").arg(pathCount) << Qt::endl;
                if (textCount > 0) out << QString("    - Texts: %1").arg(textCount) << Qt::endl;
                if (srefCount > 0) out << QString("    - SREFs: %1").arg(srefCount) << Qt::endl;
                if (arefCount > 0) out << QString("    - AREFs: %1").arg(arefCount) << Qt::endl;
                break;
                
            case GDSRecord::BOUNDARY:
                boundaryCount++;
                break;
                
            case GDSRecord::PATH:
                pathCount++;
                break;
                
            case GDSRecord::TEXT:
                textCount++;
                break;
                
            case GDSRecord::SREF:
                srefCount++;
                break;
                
            case GDSRecord::AREF:
                arefCount++;
                break;
                
            case GDSRecord::ENDLIB:
                out << Qt::endl;
                out << "=== Summary ===" << Qt::endl;
                out << "Total Cells:" << totalCells << Qt::endl;
                out << "Cell names:" << cellNames.join(", ") << Qt::endl;
                file.close();
                return 0;
                
            default:
                if (dataSize > 0) stream.skipRawData(dataSize);
                break;
        }
    }
    
    file.close();
    
    out << Qt::endl;
    out << "=== Summary ===" << Qt::endl;
    out << "Total Cells:" << totalCells << Qt::endl;
    out << "Cell names:" << cellNames.join(", ") << Qt::endl;
    
    return 0;
}