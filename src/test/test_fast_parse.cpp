/**
 * @file test_fast_parse.cpp
 * @brief 快速 GDS 解析测试
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QElapsedTimer>
#include <QHash>

QTextStream out(stdout);

// GDS 记录类型
enum GDSRecord : quint16 {
    HEADER   = 0x0002, BGNLIB  = 0x0102, LIBNAME = 0x0206,
    UNITS    = 0x0305, ENDLIB  = 0x0400, BGNSTR  = 0x0502,
    STRNAME  = 0x0606, ENDSTR  = 0x0700, BOUNDARY= 0x0800,
    PATH     = 0x0900, SREF    = 0x0A00, AREF    = 0x0B00,
    TEXT     = 0x0C00, LAYER   = 0x0D02, DATATYPE= 0x0E02,
    WIDTH    = 0x0F03, XY      = 0x1003, ENDEL  = 0x1100,
    SNAME    = 0x1206, COLROW  = 0x1302, STRING  = 0x1906,
    BOX      = 0x2D00
};

struct Stats {
    qint64 boundaries = 0, paths = 0, texts = 0;
    qint64 srefs = 0, arefs = 0, structures = 0;
    qint64 totalPoints = 0;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = argc > 1 ? argv[1] : "D:\\CMA6-164-4.gds";
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        out << "无法打开文件: " << filePath << "\n";
        return 1;
    }
    
    qint64 fileSize = file.size();
    out << "\n=== 快速 GDS 解析 ===\n";
    out << "文件: " << filePath << "\n";
    out << "大小: " << fileSize / 1024.0 / 1024.0 << " MB\n\n";
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    Stats stats;
    QElapsedTimer timer;
    timer.start();
    
    int lastProgress = -1;
    qint64 boundaryPoints = 0;
    qint64 pathPoints = 0;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen;
        
        if (stream.status() != QDataStream::Ok) break;
        
        quint8 typeHi, typeLo;
        stream.readRawData(reinterpret_cast<char*>(&typeHi), 1);
        stream.readRawData(reinterpret_cast<char*>(&typeLo), 1);
        recType = (static_cast<quint16>(typeHi) << 8) | typeLo;
        
        int dataSize = recLen - 4;
        if (dataSize < 0 || recLen < 4 || recLen > 65532) {
            out << "无效记录在位置 " << file.pos() << "\n";
            break;
        }
        
        switch (recType) {
            case BGNSTR: stats.structures++; break;
            case BOUNDARY: stats.boundaries++; break;
            case PATH: stats.paths++; break;
            case TEXT: stats.texts++; break;
            case SREF: stats.srefs++; break;
            case AREF: stats.arefs++; break;
            case XY: stats.totalPoints += dataSize / 8; break;
        }
        
        if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        int progress = static_cast<int>(file.pos() * 100 / fileSize);
        if (progress != lastProgress && progress % 20 == 0) {
            out << "进度: " << progress << "% - B:" << stats.boundaries 
                << " P:" << stats.paths << " S:" << stats.structures << "\n";
            out.flush();
            lastProgress = progress;
        }
    }
    
    file.close();
    
    qint64 parseTime = timer.elapsed();
    
    out << "\n=== 解析结果 ===\n";
    out << "解析耗时: " << parseTime << " ms\n";
    out << "结构数: " << stats.structures << "\n";
    out << "BOUNDARY: " << stats.boundaries << "\n";
    out << "PATH: " << stats.paths << "\n";
    out << "TEXT: " << stats.texts << "\n";
    out << "SREF: " << stats.srefs << "\n";
    out << "AREF: " << stats.arefs << "\n";
    out << "总顶点数: " << stats.totalPoints << "\n";
    
    return 0;
}