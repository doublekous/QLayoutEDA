/**
 * @file create_multi_cell_gds.cpp
 * @brief 创建多 Cell 测试文件
 */

#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>

QTextStream out(stdout);

void writeGDSRecord(QDataStream& ds, quint16 type, const QByteArray& data = QByteArray())
{
    quint16 len = static_cast<quint16>(4 + data.size());
    ds << len;
    ds << type;
    if (!data.isEmpty()) {
        ds.writeRawData(data.constData(), data.size());
    }
}

void writeGDSReal8(QByteArray& ba, double value)
{
    // GDS Real8 格式
    int sign = value < 0 ? 1 : 0;
    value = qAbs(value);
    
    if (value == 0) {
        ba.append(8, 0);
        return;
    }
    
    int exp = 0;
    while (value >= 1.0) { value /= 16.0; exp++; }
    while (value < 0.0625) { value *= 16.0; exp--; }
    exp += 64;
    
    quint64 mantissa = static_cast<quint64>(value * (1ULL << 56));
    
    ba.append(static_cast<char>((sign << 7) | exp));
    for (int i = 6; i >= 0; i--) {
        ba.append(static_cast<char>((mantissa >> (i * 8)) & 0xFF));
    }
}

void createMultiCellGDS(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        out << "无法创建文件: " << filePath << "\n";
        return;
    }
    
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::BigEndian);
    
    // HEADER
    writeGDSRecord(ds, 0x0002, QByteArray(2, 0));
    
    // BGNLIB
    QByteArray bgnlib(28, 0);
    writeGDSRecord(ds, 0x0102, bgnlib);
    
    // LIBNAME
    QByteArray libName = "MULTI_CELL_TEST";
    libName.append('\0');
    writeGDSRecord(ds, 0x0206, libName);
    
    // UNITS
    QByteArray units;
    writeGDSReal8(units, 1e-6);  // user units per meter
    writeGDSReal8(units, 1e-9);  // db units per meter
    writeGDSRecord(ds, 0x0305, units);
    
    // ========== Cell 1: TOP ==========
    // BGNSTR
    writeGDSRecord(ds, 0x0502, bgnlib);
    // STRNAME
    QByteArray cell1Name = "TOP";
    cell1Name.append('\0');
    writeGDSRecord(ds, 0x0606, cell1Name);
    
    // BOUNDARY 1 - 红色矩形
    writeGDSRecord(ds, 0x0800);  // BOUNDARY
    QByteArray layer1; layer1.append('\0').append('\1');  // Layer 1
    writeGDSRecord(ds, 0x0D02, layer1);
    writeGDSRecord(ds, 0x0E02, QByteArray(2, 0));  // DATATYPE
    QByteArray xy1;
    QDataStream xy1Ds(&xy1, QIODevice::WriteOnly);
    xy1Ds.setByteOrder(QDataStream::BigEndian);
    qint32 coords1[] = {0, 0, 10000, 0, 10000, 5000, 0, 5000, 0, 0};
    for (int i = 0; i < 10; i++) xy1Ds << coords1[i];
    writeGDSRecord(ds, 0x1003, xy1);
    writeGDSRecord(ds, 0x1100);  // ENDEL
    
    // SREF to CELL_A
    writeGDSRecord(ds, 0x0A00);  // SREF
    QByteArray cellAName = "CELL_A";
    cellAName.append('\0');
    writeGDSRecord(ds, 0x1206, cellAName);
    QByteArray srefXY;
    QDataStream srefDs(&srefXY, QIODevice::WriteOnly);
    srefDs.setByteOrder(QDataStream::BigEndian);
    srefDs << (qint32)15000 << (qint32)0;
    writeGDSRecord(ds, 0x1003, srefXY);
    writeGDSRecord(ds, 0x1100);  // ENDEL
    
    // SREF to CELL_B
    writeGDSRecord(ds, 0x0A00);  // SREF
    QByteArray cellBName = "CELL_B";
    cellBName.append('\0');
    writeGDSRecord(ds, 0x1206, cellBName);
    QByteArray sref2XY;
    QDataStream sref2Ds(&sref2XY, QIODevice::WriteOnly);
    sref2Ds.setByteOrder(QDataStream::BigEndian);
    sref2Ds << (qint32)0 << (qint32)10000;
    writeGDSRecord(ds, 0x1003, sref2XY);
    writeGDSRecord(ds, 0x1100);  // ENDEL
    
    writeGDSRecord(ds, 0x0700);  // ENDSTR
    
    // ========== Cell 2: CELL_A ==========
    writeGDSRecord(ds, 0x0502, bgnlib);
    QByteArray cell2Name = "CELL_A";
    cell2Name.append('\0');
    writeGDSRecord(ds, 0x0606, cell2Name);
    
    // BOUNDARY - 蓝色矩形
    writeGDSRecord(ds, 0x0800);
    QByteArray layer2; layer2.append('\0').append('\2');  // Layer 2
    writeGDSRecord(ds, 0x0D02, layer2);
    writeGDSRecord(ds, 0x0E02, QByteArray(2, 0));
    QByteArray xy2;
    QDataStream xy2Ds(&xy2, QIODevice::WriteOnly);
    xy2Ds.setByteOrder(QDataStream::BigEndian);
    qint32 coords2[] = {0, 0, 5000, 0, 5000, 8000, 0, 8000, 0, 0};
    for (int i = 0; i < 10; i++) xy2Ds << coords2[i];
    writeGDSRecord(ds, 0x1003, xy2);
    writeGDSRecord(ds, 0x1100);
    
    writeGDSRecord(ds, 0x0700);  // ENDSTR
    
    // ========== Cell 3: CELL_B ==========
    writeGDSRecord(ds, 0x0502, bgnlib);
    QByteArray cell3Name = "CELL_B";
    cell3Name.append('\0');
    writeGDSRecord(ds, 0x0606, cell3Name);
    
    // BOUNDARY - 绿色矩形
    writeGDSRecord(ds, 0x0800);
    QByteArray layer3; layer3.append('\0').append('\3');  // Layer 3
    writeGDSRecord(ds, 0x0D02, layer3);
    writeGDSRecord(ds, 0x0E02, QByteArray(2, 0));
    QByteArray xy3;
    QDataStream xy3Ds(&xy3, QIODevice::WriteOnly);
    xy3Ds.setByteOrder(QDataStream::BigEndian);
    qint32 coords3[] = {0, 0, 6000, 0, 6000, 4000, 0, 4000, 0, 0};
    for (int i = 0; i < 10; i++) xy3Ds << coords3[i];
    writeGDSRecord(ds, 0x1003, xy3);
    writeGDSRecord(ds, 0x1100);
    
    // PATH
    writeGDSRecord(ds, 0x0900);  // PATH
    writeGDSRecord(ds, 0x0D02, layer3);
    QByteArray width; QDataStream wDs(&width, QIODevice::WriteOnly);
    wDs.setByteOrder(QDataStream::BigEndian);
    wDs << (qint32)500;
    writeGDSRecord(ds, 0x0F03, width);  // WIDTH
    QByteArray xyPath;
    QDataStream xyPathDs(&xyPath, QIODevice::WriteOnly);
    xyPathDs.setByteOrder(QDataStream::BigEndian);
    qint32 pathCoords[] = {1000, 1000, 5000, 1000, 5000, 3000};
    for (int i = 0; i < 6; i++) xyPathDs << pathCoords[i];
    writeGDSRecord(ds, 0x1003, xyPath);
    writeGDSRecord(ds, 0x1100);
    
    writeGDSRecord(ds, 0x0700);  // ENDSTR
    
    // ENDLIB
    writeGDSRecord(ds, 0x0400);
    
    file.close();
    out << "创建成功: " << filePath << "\n";
    out << "包含 3 个 Cell:\n";
    out << "  - TOP (顶层，包含对 CELL_A 和 CELL_B 的引用)\n";
    out << "  - CELL_A (蓝色矩形)\n";
    out << "  - CELL_B (绿色矩形 + 路径)\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = "D:\\multi_cell_test.gds";
    createMultiCellGDS(filePath);
    
    return 0;
}