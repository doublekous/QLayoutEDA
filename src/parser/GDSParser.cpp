/**
 * @file GDSParser.cpp
 * @brief GDSII 解析器完整实现
 */

#include "GDSParser.h"
#include "GDSFormat.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QtMath>

namespace QLayoutEDA {

struct GDSParser::Impl {
    GDSDataPtr data;
    QString lastError;
    int progress = 0;
    bool cancelled = false;
    qint64 totalBytes = 0;
    qint64 bytesRead = 0;
    std::function<void(int)> progressCallback;
    GDSStructurePtr currentStructure;
    
    void updateProgress(qint64 pos) {
        bytesRead = pos;
        if (totalBytes > 0) {
            int newProgress = static_cast<int>(bytesRead * 100 / totalBytes);
            if (newProgress != progress) {
                progress = newProgress;
                if (progressCallback) progressCallback(progress);
            }
        }
    }
    
    void reset() {
        data = std::make_shared<GDSData>();
        lastError.clear();
        progress = 0;
        cancelled = false;
        currentStructure.reset();
    }
};

GDSParser::GDSParser(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
}

GDSParser::~GDSParser() = default;

bool GDSParser::parse(const QString& filePath)
{
    d->reset();
    d->data->libraryPath = filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        d->lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }
    
    d->totalBytes = file.size();
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    bool success = true;
    while (!stream.atEnd() && !d->cancelled) {
        if (!parseRecord(stream)) {
            success = false;
            break;
        }
        d->updateProgress(file.pos());
    }
    
    file.close();
    
    if (d->cancelled) {
        d->lastError = "Parsing cancelled";
        return false;
    }
    
    if (success) {
        d->progress = 100;
        if (d->progressCallback) d->progressCallback(100);
        
        // ========== 诊断日志：解析统计 ==========
        qDebug() << "========================================";
        qDebug() << "=== GDS 解析统计 ===";
        qDebug() << "文件:" << filePath;
        qDebug() << "文件大小:" << d->totalBytes << "bytes (" << d->totalBytes / 1024.0 / 1024.0 << "MB)";
        qDebug() << "结构数量:" << d->data->structures.size();
        qDebug() << "顶层结构:" << d->data->topStructure;
        
        qint64 totalBoundaries = 0;
        qint64 totalPaths = 0;
        qint64 totalTexts = 0;
        qint64 totalSRefs = 0;
        qint64 totalARefs = 0;
        qint64 totalARefInstances = 0;
        
        for (auto it = d->data->structures.begin(); it != d->data->structures.end(); ++it) {
            const auto& structName = it.key();
            const auto& structure = it.value();
            totalBoundaries += structure->boundaries.size();
            totalPaths += structure->paths.size();
            totalTexts += structure->texts.size();
            totalSRefs += structure->references.size();
            totalARefs += structure->arrays.size();
            
            // 计算 AREF 展开后的实例数
            for (const auto& aref : structure->arrays) {
                totalARefInstances += static_cast<qint64>(aref.columns) * aref.rows;
            }
            
            qDebug() << "  结构:" << structName 
                     << "| BOUNDARY:" << structure->boundaries.size()
                     << "| PATH:" << structure->paths.size()
                     << "| TEXT:" << structure->texts.size()
                     << "| SREF:" << structure->references.size()
                     << "| AREF:" << structure->arrays.size();
        }
        
        qDebug() << "---";
        qDebug() << "总计 BOUNDARY:" << totalBoundaries;
        qDebug() << "总计 PATH:" << totalPaths;
        qDebug() << "总计 TEXT:" << totalTexts;
        qDebug() << "总计 SREF:" << totalSRefs;
        qDebug() << "总计 AREF:" << totalARefs << "(展开后实例:" << totalARefInstances << ")";
        qDebug() << "总计基础图元:" << (totalBoundaries + totalPaths + totalTexts);
        qDebug() << "========================================";
    }
    
    return success;
}

GDSDataPtr GDSParser::getData() const { return d->data; }
int GDSParser::getProgress() const { return d->progress; }
QString GDSParser::getLastError() const { return d->lastError; }
void GDSParser::cancel() { d->cancelled = true; }
void GDSParser::setProgressCallback(std::function<void(int)> cb) { d->progressCallback = cb; }

bool GDSParser::parseRecord(QDataStream& stream)
{
    quint16 recordLength = 0, recordType = 0;
    stream >> recordLength >> recordType;
    
    if (stream.status() != QDataStream::Ok) {
        return stream.atEnd();
    }
    
    int dataSize = recordLength - 4;
    if (dataSize < 0 || recordLength < 4 || recordLength > 65532) {
        // 无效记录，尝试跳过恢复
        stream.setStatus(QDataStream::Ok);
        char dummy;
        stream.readRawData(&dummy, 1);
        return true;
    }
    
    switch (recordType) {
        case GDSRecord::HEADER: {
            quint16 ver;
            stream >> ver;
            d->data->version = QString::number(ver);
            break;
        }
        case GDSRecord::BGNLIB:
            stream.skipRawData(dataSize);
            break;
        case GDSRecord::LIBNAME: {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            d->data->libraryName = QString::fromLatin1(data);
            break;
        }
        case GDSRecord::UNITS: {
            if (dataSize >= 16) {
                QByteArray real8(8, 0);
                stream.readRawData(real8.data(), 8);
                d->data->userUnitsPerMeter = readGDSReal8(real8);
                stream.readRawData(real8.data(), 8);
                d->data->dbUnitsPerMeter = readGDSReal8(real8);
            }
            break;
        }
        case GDSRecord::ENDLIB:
            return true;
        case GDSRecord::BGNSTR:
            d->currentStructure = std::make_shared<GDSStructure>();
            stream.skipRawData(dataSize);
            break;
        case GDSRecord::STRNAME: {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            if (d->currentStructure) {
                d->currentStructure->name = QString::fromLatin1(data);
            }
            break;
        }
        case GDSRecord::ENDSTR: {
            if (d->currentStructure) {
                d->data->structures[d->currentStructure->name] = d->currentStructure;
                if (d->data->topStructure.isEmpty()) {
                    d->data->topStructure = d->currentStructure->name;
                }
                d->currentStructure.reset();
            }
            break;
        }
        case GDSRecord::BOUNDARY:
            parseBoundary(stream);
            break;
        case GDSRecord::PATH:
            parsePath(stream);
            break;
        case GDSRecord::SREF:
            parseSRef(stream);
            break;
        case GDSRecord::AREF:
            parseARef(stream);
            break;
        case GDSRecord::TEXT:
            parseText(stream);
            break;
        case GDSRecord::BOX:
            parseBox(stream);
            break;
        default:
            if (dataSize > 0) stream.skipRawData(dataSize);
            break;
    }
    
    return stream.status() == QDataStream::Ok;
}

bool GDSParser::parseBoundary(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSBoundary boundary;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::LAYER && dataSize >= 2) {
            qint16 val;
            stream >> val;
            boundary.layer = val;
        } else if (recType == GDSRecord::DATATYPE && dataSize >= 2) {
            qint16 val;
            stream >> val;
            boundary.dataType = val;
        } else if (recType == GDSRecord::XY && dataSize >= 8) {
            int count = dataSize / 8;
            for (int i = 0; i < count; ++i) {
                qint32 x, y;
                stream >> x >> y;
                boundary.points.append(DbPoint(x, y));
            }
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->boundaries.append(boundary);
    return true;
}

bool GDSParser::parsePath(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSPath path;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::LAYER && dataSize >= 2) {
            qint16 val;
            stream >> val;
            path.layer = val;
        } else if (recType == GDSRecord::WIDTH && dataSize >= 4) {
            qint32 val;
            stream >> val;
            path.width = val;
        } else if (recType == GDSRecord::XY && dataSize >= 8) {
            int count = dataSize / 8;
            for (int i = 0; i < count; ++i) {
                qint32 x, y;
                stream >> x >> y;
                path.points.append(DbPoint(x, y));
            }
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->paths.append(path);
    return true;
}

bool GDSParser::parseSRef(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSReference ref;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::SNAME && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            ref.structureName = QString::fromLatin1(data);
        } else if (recType == GDSRecord::STRANS && dataSize >= 2) {
            quint16 flags;
            stream >> flags;
            ref.reflected = (flags & 0x8000) != 0;
        } else if (recType == GDSRecord::ANGLE && dataSize >= 8) {
            QByteArray real8(8, 0);
            stream.readRawData(real8.data(), 8);
            ref.angle = readGDSReal8(real8);
        } else if (recType == GDSRecord::MAG && dataSize >= 8) {
            QByteArray real8(8, 0);
            stream.readRawData(real8.data(), 8);
            ref.magnification = readGDSReal8(real8);
        } else if (recType == GDSRecord::XY && dataSize >= 8) {
            qint32 x, y;
            stream >> x >> y;
            ref.position = DbPoint(x, y);
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->references.append(ref);
    return true;
}

bool GDSParser::parseARef(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSArrayReference aref;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::SNAME && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            aref.structureName = QString::fromLatin1(data);
        } else if (recType == GDSRecord::COLROW && dataSize >= 4) {
            quint16 cols, rows;
            stream >> cols >> rows;
            aref.columns = cols;
            aref.rows = rows;
        } else if (recType == GDSRecord::XY && dataSize >= 24) {
            qint32 x1, y1, x2, y2, x3, y3;
            stream >> x1 >> y1 >> x2 >> y2 >> x3 >> y3;
            aref.position = DbPoint(x1, y1);
            aref.columnVector = DbPoint(x2 - x1, y2 - y1);
            aref.rowVector = DbPoint(x3 - x1, y3 - y1);
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->arrays.append(aref);
    return true;
}

bool GDSParser::parseText(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSText text;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::LAYER && dataSize >= 2) {
            qint16 val;
            stream >> val;
            text.layer = val;
        } else if (recType == GDSRecord::STRING && dataSize > 0) {
            QByteArray data(dataSize, 0);
            stream.readRawData(data.data(), dataSize);
            while (data.endsWith('\0')) data.chop(1);
            text.text = QString::fromLatin1(data);
        } else if (recType == GDSRecord::XY && dataSize >= 8) {
            qint32 x, y;
            stream >> x >> y;
            text.position = DbPoint(x, y);
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->texts.append(text);
    return true;
}

bool GDSParser::parseBox(QDataStream& stream)
{
    if (!d->currentStructure) return false;
    
    GDSBoundary box;
    
    while (!stream.atEnd()) {
        quint16 recLen, recType;
        stream >> recLen >> recType;
        int dataSize = recLen - 4;
        
        if (recType == GDSRecord::ENDEL) break;
        
        if (recType == GDSRecord::LAYER && dataSize >= 2) {
            qint16 val;
            stream >> val;
            box.layer = val;
        } else if (recType == GDSRecord::XY && dataSize >= 8) {
            int count = dataSize / 8;
            for (int i = 0; i < count; ++i) {
                qint32 x, y;
                stream >> x >> y;
                box.points.append(DbPoint(x, y));
            }
        } else if (dataSize > 0) {
            stream.skipRawData(dataSize);
        }
    }
    
    d->currentStructure->boundaries.append(box);
    return true;
}

} // namespace QLayoutEDA