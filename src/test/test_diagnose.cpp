/**
 * @file test_diagnose.cpp
 * @brief GDS 文件诊断工具 - 完整分析
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>

QTextStream out(stdout);

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
    out << "\n=== GDS 文件深度诊断 ===\n";
    out << "文件: " << filePath << "\n";
    out << "大小: " << fileSize << " bytes (" << QString::number(fileSize / 1024.0 / 1024.0, 'f', 2) << " MB)\n\n";
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    qint64 boundaryCount = 0;
    qint64 pathCount = 0;
    qint64 textCount = 0;
    qint64 srefCount = 0;
    qint64 arefCount = 0;
    qint64 endelCount = 0;
    qint64 bgnstrCount = 0;
    qint64 xyCount = 0;
    qint64 totalPoints = 0;
    int maxPoints = 0;
    
    // 边界框
    qint64 minX = LLONG_MAX, maxX = LLONG_MIN;
    qint64 minY = LLONG_MAX, maxY = LLONG_MIN;
    bool hasBounds = false;
    
    int lastProgress = -1;
    
    while (!stream.atEnd()) {
        quint16 recLen;
        stream >> recLen;
        if (stream.status() != QDataStream::Ok) break;
        
        quint8 typeHi, typeLo;
        stream.readRawData(reinterpret_cast<char*>(&typeHi), 1);
        stream.readRawData(reinterpret_cast<char*>(&typeLo), 1);
        if (stream.status() != QDataStream::Ok) break;
        
        quint16 recType = (static_cast<quint16>(typeHi) << 8) | typeLo;
        
        int dataSize = recLen - 4;
        if (dataSize < 0) break;
        
        // 统计记录类型
        switch (recType) {
            case 0x0800: boundaryCount++; break;
            case 0x0900: pathCount++; break;
            case 0x0C00: textCount++; break;
            case 0x0A00: srefCount++; break;
            case 0x0B00: arefCount++; break;
            case 0x1100: endelCount++; break;
            case 0x0502: bgnstrCount++; break;
            case 0x0700: break; // ENDSTR
            case 0x1003: { // XY 记录
                int pointCount = dataSize / 8;
                if (maxPoints < pointCount) maxPoints = pointCount;
                totalPoints += pointCount;
                xyCount++;
                
                // 读取坐标更新边界框
                int numCoords = dataSize / 4;
                for (int i = 0; i < numCoords; i += 2) {
                    qint32 x, y;
                    stream >> x >> y;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                    hasBounds = true;
                }
                continue; // 已读取数据
            }
        }
        
        // 跳过数据
        if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
        
        // 进度
        int progress = static_cast<int>(file.pos() * 100 / fileSize);
        if (progress != lastProgress && progress % 20 == 0) {
            out << "进度: " << progress << "%\n";
            out.flush();
            lastProgress = progress;
        }
    }
    
    file.close();
    
    out << "\n=== 统计结果 ===\n";
    out << "结构数: " << bgnstrCount << "\n";
    out << "BOUNDARY: " << boundaryCount << "\n";
    out << "PATH: " << pathCount << "\n";
    out << "TEXT: " << textCount << "\n";
    out << "SREF: " << srefCount << "\n";
    out << "AREF: " << arefCount << "\n";
    out << "ENDEL: " << endelCount << "\n";
    out << "XY 记录数: " << xyCount << "\n";
    out << "总顶点数: " << totalPoints << "\n";
    out << "最大单图顶点数: " << maxPoints << "\n";
    
    if (boundaryCount > 0) {
        out << "\n=== 分析 ===\n";
        out << "平均每 BOUNDARY 顶点数: " << QString::number(static_cast<double>(totalPoints) / boundaryCount, 'f', 1) << "\n";
        out << "平均每 BOUNDARY 大小: " << QString::number(static_cast<double>(fileSize) / boundaryCount / 1024.0, 'f', 2) << " KB\n";
    }
    
    if (hasBounds) {
        out << "\n=== 边界框 ===\n";
        out << "X 范围: " << minX << " ~ " << maxX << " (" << (maxX - minX) << " 数据库单位)\n";
        out << "Y 范围: " << minY << " ~ " << maxY << " (" << (maxY - minY) << " 数据库单位)\n";
        double widthUm = (maxX - minX) / 1000.0;
        double heightUm = (maxY - minY) / 1000.0;
        out << "尺寸: " << QString::number(widthUm / 1000.0, 'f', 3) << " x " 
            << QString::number(heightUm / 1000.0, 'f', 3) << " mm\n";
    }
    
    // 结论
    out << "\n=== 问题诊断 ===\n";
    if (maxPoints > 1000) {
        out << "[!] 警告: 单个图形顶点数过多 (" << maxPoints << ")\n";
        out << "    这会导致渲染性能严重下降\n";
        out << "    建议: 实现顶点简化或 LOD 渲染\n";
    }
    
    if (totalPoints > 100000) {
        out << "[!] 总顶点数: " << totalPoints << " (超过10万)\n";
        out << "    需要空间索引和视口裁剪优化\n";
    }
    
    return 0;
}