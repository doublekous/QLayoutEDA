#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QSet>
#include <QHash>

QTextStream out(stdout);

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QString file = argc > 1 ? argv[1] : "D:\\testFile\\20250530\\Topcell_all_layout.gds";
    
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) { out << "Error\n"; return 1; }
    
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::BigEndian);
    
    QSet<QString> allCells, referencedCells;
    QHash<QString, QSet<QString>> cellRefs, cellRefdBy;
    QString currentCell;
    bool inStr = false;
    
    while (!ds.atEnd()) {
        quint16 len; ds >> len;
        quint8 hi, lo; ds.readRawData((char*)&hi,1); ds.readRawData((char*)&lo,1);
        quint16 type = (hi<<8)|lo;
        int data = len - 4;
        if (data < 0 || len > 65532) break;
        
        if (type == 0x0502) { inStr = true; currentCell.clear(); }
        else if (type == 0x0606 && inStr && data > 0) {
            QByteArray n(data, 0); ds.readRawData(n.data(), data);
            while (n.endsWith('\0')) n.chop(1);
            currentCell = QString::fromLatin1(n);
            allCells.insert(currentCell);
        }
        else if (type == 0x0700) { inStr = false; currentCell.clear(); }
        else if (type == 0x1206 && !currentCell.isEmpty() && data > 0) {
            QByteArray n(data, 0); ds.readRawData(n.data(), data);
            while (n.endsWith('\0')) n.chop(1);
            QString ref = QString::fromLatin1(n);
            cellRefs[currentCell].insert(ref);
            cellRefdBy[ref].insert(currentCell);
            referencedCells.insert(ref);
        }
        else if (data > 0) ds.skipRawData(data);
    }
    
    QSet<QString> topCells = allCells - referencedCells;
    
    out << "\n=== Cell Hierarchy ===\n";
    out << "Total: " << allCells.size() << ", Top-level: " << topCells.size() << "\n\n";
    
    QStringList check = {"Res_bansui_1_ab_Flip_$1", "Singlebit_V8_2_GRID_ARRAY(1)$1", "TiNPad&Inpillar(3)", "topcell000111"};
    
    for (const QString& c : check) {
        out << c << "\n";
        out << "  Exists: " << (allCells.contains(c) ? "YES" : "NO");
        out << ", TopLevel: " << (topCells.contains(c) ? "YES" : "NO") << "\n";
        if (cellRefdBy.contains(c)) {
            out << "  Referenced by: " << QStringList(cellRefdBy[c].values()).join(", ") << "\n";
        }
    }
    
    out << "\n=== All Top-Level Cells ===\n";
    for (const QString& c : topCells) {
        out << "  " << c << "\n";
    }
    
    return 0;
}