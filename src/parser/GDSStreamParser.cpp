/**
 * @file GDSStreamParser.cpp
 * @brief GDSII 流式解析器实现
 */

#include "GDSStreamParser.h"
#include "GDSFormat.h"
#include <QDataStream>
#include <QDebug>
#include <QElapsedTimer>

namespace QLayoutEDA {

struct GDSStreamParser::Impl {
    // 文件信息
    QString filePath;
    QString lastError;
    int progress = 0;
    bool cancelled = false;
    
    // 库信息
    QString libraryName;
    double dbUnitsPerMeter = 1e-9;
    double userUnitsPerMeter = 1e-3;
    QString topCellName;
    
    // Cell 位置索引
    QHash<QString, CellLocation> cellLocations;
    
    // Cell 缓存 (LRU)
    QCache<QString, GDSStructurePtr> cellCache;
    int maxCacheSize = 50;  // 默认缓存 50 个 Cell
    
    // 已加载的完整数据
    GDSDataPtr data;
    
    // 进度回调
    std::function<void(int)> progressCallback;
    
    // 线程安全
    mutable QMutex mutex;
    
    void updateProgress(int p) {
        progress = p;
        if (progressCallback) {
            progressCallback(p);
        }
    }
    
    void setError(const QString& err) {
        lastError = err;
        qWarning() << "GDSStreamParser error:" << err;
    }
};

GDSStreamParser::GDSStreamParser(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
    d->cellCache.setMaxCost(d->maxCacheSize);
}

GDSStreamParser::~GDSStreamParser() = default;

//=============================================================================
// IGDSParser 接口实现
//=============================================================================

bool GDSStreamParser::parse(const QString& filePath)
{
    return scanMetadata(filePath);
}

GDSDataPtr GDSStreamParser::getData() const
{
    QMutexLocker locker(&d->mutex);
    
    if (!d->data) {
        d->data = std::make_shared<GDSData>();
        d->data->libraryName = d->libraryName;
        d->data->dbUnitsPerMeter = d->dbUnitsPerMeter;
        d->data->userUnitsPerMeter = d->userUnitsPerMeter;
        d->data->topStructure = d->topCellName;
    }
    
    return d->data;
}

int GDSStreamParser::getProgress() const
{
    return d->progress;
}

QString GDSStreamParser::getLastError() const
{
    return d->lastError;
}

void GDSStreamParser::cancel()
{
    d->cancelled = true;
}

void GDSStreamParser::setProgressCallback(std::function<void(int)> callback)
{
    d->progressCallback = callback;
}

//=============================================================================
// 流式解析接口
//=============================================================================

bool GDSStreamParser::scanMetadata(const QString& filePath)
{
    QMutexLocker locker(&d->mutex);
    
    d->filePath = filePath;
    d->lastError.clear();
    d->cancelled = false;
    d->progress = 0;
    d->cellLocations.clear();
    d->cellCache.clear();
    
    QElapsedTimer timer;
    timer.start();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        d->setError(QString("Cannot open file: %1").arg(filePath));
        return false;
    }
    
    qint64 fileSize = file.size();
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    // 扫描阶段：只记录 Cell 位置
    int structureCount = 0;
    qint64 lastProgressUpdate = 0;
    
    while (!stream.atEnd() && !d->cancelled) {
        qint64 recordPos = file.pos();
        
        quint16 recordLength = 0, recordType = 0;
        stream >> recordLength >> recordType;
        
        if (stream.status() != QDataStream::Ok) {
            if (stream.atEnd()) break;
            d->setError(QString("Read error at position %1").arg(recordPos));
            return false;
        }
        
        int dataSize = recordLength - 4;
        if (dataSize < 0) {
            d->setError(QString("Invalid record length at position %1").arg(recordPos));
            return false;
        }
        
        switch (recordType) {
            case GDSRecord::LIBNAME: {
                QByteArray nameData(dataSize, 0);
                stream.readRawData(nameData.data(), dataSize);
                while (nameData.endsWith('\0')) nameData.chop(1);
                d->libraryName = QString::fromLatin1(nameData);
                break;
            }
            
            case GDSRecord::UNITS: {
                if (dataSize >= 16) {
                    QByteArray real8(8, 0);
                    stream.readRawData(real8.data(), 8);
                    d->userUnitsPerMeter = readGDSReal8(real8);
                    stream.readRawData(real8.data(), 8);
                    d->dbUnitsPerMeter = readGDSReal8(real8);
                } else if (dataSize > 0) {
                    stream.skipRawData(dataSize);
                }
                break;
            }
            
            case GDSRecord::BGNSTR: {
                // 记录 Cell 起始位置
                CellLocation loc;
                loc.fileOffset = recordPos;
                loc.fileSize = 0;  // 稍后计算
                loc.boundaryCount = 0;
                loc.referenceCount = 0;
                
                // 临时存储，等待 STRNAME
                structureCount++;
                
                // 读取直到找到 STRNAME
                while (!stream.atEnd()) {
                    quint16 subLen, subType;
                    stream >> subLen >> subType;
                    int subDataSize = subLen - 4;
                    
                    if (subType == GDSRecord::STRNAME && subDataSize > 0) {
                        QByteArray nameData(subDataSize, 0);
                        stream.readRawData(nameData.data(), subDataSize);
                        while (nameData.endsWith('\0')) nameData.chop(1);
                        loc.name = QString::fromLatin1(nameData);
                        break;
                    } else if (subType == GDSRecord::ENDSTR) {
                        break;
                    } else if (subDataSize > 0) {
                        stream.skipRawData(subDataSize);
                    }
                }
                
                if (!loc.name.isEmpty()) {
                    d->cellLocations[loc.name] = loc;
                }
                
                // 第一个 Cell 作为顶层
                if (d->topCellName.isEmpty() && !loc.name.isEmpty()) {
                    d->topCellName = loc.name;
                }
                
                break;
            }
            
            case GDSRecord::ENDLIB:
                // 扫描完成
                break;
                
            default:
                if (dataSize > 0) {
                    stream.skipRawData(dataSize);
                }
                break;
        }
        
        // 更新进度
        qint64 currentPos = file.pos();
        if (currentPos - lastProgressUpdate > fileSize / 100) {
            int prog = static_cast<int>(currentPos * 100 / fileSize);
            d->updateProgress(prog);
            lastProgressUpdate = currentPos;
        }
    }
    
    file.close();
    
    d->updateProgress(100);
    
    qDebug() << "Scan completed:" << d->cellLocations.size() << "cells in" 
             << timer.elapsed() << "ms";
    
    return !d->cancelled && !d->cellLocations.isEmpty();
}

GDSStructurePtr GDSStreamParser::loadCell(const QString& cellName)
{
    QMutexLocker locker(&d->mutex);
    
    // 1. 检查缓存
    if (d->cellCache.contains(cellName)) {
        return *d->cellCache.object(cellName);
    }
    
    // 2. 检查位置索引
    if (!d->cellLocations.contains(cellName)) {
        d->setError(QString("Cell not found: %1").arg(cellName));
        return nullptr;
    }
    
    // 3. 从文件加载
    auto cell = parseCellFromFile(cellName);
    if (!cell) {
        return nullptr;
    }
    
    // 4. 加入缓存
    auto cellPtr = std::make_shared<GDSStructure>(*cell);
    d->cellCache.insert(cellName, new GDSStructurePtr(cellPtr));
    
    // 5. 更新数据
    if (d->data) {
        d->data->structures[cellName] = cellPtr;
    }
    
    emit cellLoaded(cellName);
    
    return cellPtr;
}

int GDSStreamParser::loadCells(const QStringList& cellNames)
{
    int loaded = 0;
    for (const QString& name : cellNames) {
        if (loadCell(name)) {
            loaded++;
        }
    }
    return loaded;
}

QStringList GDSStreamParser::getCellNames() const
{
    QMutexLocker locker(&d->mutex);
    return d->cellLocations.keys();
}

CellLocation GDSStreamParser::getCellLocation(const QString& cellName) const
{
    QMutexLocker locker(&d->mutex);
    return d->cellLocations.value(cellName);
}

QString GDSStreamParser::getTopCellName() const
{
    QMutexLocker locker(&d->mutex);
    return d->topCellName;
}

void GDSStreamParser::setCacheSize(int maxCells)
{
    QMutexLocker locker(&d->mutex);
    d->maxCacheSize = maxCells;
    d->cellCache.setMaxCost(maxCells);
}

void GDSStreamParser::clearCache()
{
    QMutexLocker locker(&d->mutex);
    d->cellCache.clear();
}

int GDSStreamParser::getCachedCellCount() const
{
    QMutexLocker locker(&d->mutex);
    return d->cellCache.size();
}

//=============================================================================
// 内部解析方法
//=============================================================================

GDSStructurePtr GDSStreamParser::parseCellFromFile(const QString& cellName)
{
    const CellLocation& loc = d->cellLocations[cellName];
    
    QFile file(d->filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        d->setError(QString("Cannot open file for cell loading"));
        return nullptr;
    }
    
    file.seek(loc.fileOffset);
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    auto cell = std::make_shared<GDSStructure>();
    cell->name = cellName;
    
    // 解析 Cell 数据直到 ENDSTR
    bool foundEnd = false;
    
    while (!stream.atEnd() && !foundEnd) {
        quint16 recordLength = 0, recordType = 0;
        stream >> recordLength >> recordType;
        
        int dataSize = recordLength - 4;
        if (dataSize < 0) break;
        
        switch (recordType) {
            case GDSRecord::BGNSTR:
            case GDSRecord::STRNAME:
                // 已处理，跳过数据
                if (dataSize > 0) stream.skipRawData(dataSize);
                break;
                
            case GDSRecord::ENDSTR:
                foundEnd = true;
                break;
                
            case GDSRecord::BOUNDARY: {
                GDSBoundary boundary;
                bool elemEnd = false;
                
                while (!stream.atEnd() && !elemEnd) {
                    quint16 elemLen, elemType;
                    stream >> elemLen >> elemType;
                    int elemDataSize = elemLen - 4;
                    
                    if (elemType == GDSRecord::ENDEL) {
                        elemEnd = true;
                    } else if (elemType == GDSRecord::LAYER && elemDataSize >= 2) {
                        qint16 val;
                        stream >> val;
                        boundary.layer = val;
                    } else if (elemType == GDSRecord::DATATYPE && elemDataSize >= 2) {
                        qint16 val;
                        stream >> val;
                        boundary.dataType = val;
                    } else if (elemType == GDSRecord::XY && elemDataSize >= 8) {
                        int count = elemDataSize / 8;
                        for (int i = 0; i < count; ++i) {
                            qint32 x, y;
                            stream >> x >> y;
                            boundary.points.append(DbPoint(x, y));
                        }
                    } else if (elemDataSize > 0) {
                        stream.skipRawData(elemDataSize);
                    }
                }
                
                cell->boundaries.append(boundary);
                break;
            }
            
            case GDSRecord::PATH: {
                GDSPath path;
                bool elemEnd = false;
                
                while (!stream.atEnd() && !elemEnd) {
                    quint16 elemLen, elemType;
                    stream >> elemLen >> elemType;
                    int elemDataSize = elemLen - 4;
                    
                    if (elemType == GDSRecord::ENDEL) {
                        elemEnd = true;
                    } else if (elemType == GDSRecord::LAYER && elemDataSize >= 2) {
                        qint16 val;
                        stream >> val;
                        path.layer = val;
                    } else if (elemType == GDSRecord::WIDTH && elemDataSize >= 4) {
                        qint32 val;
                        stream >> val;
                        path.width = val;
                    } else if (elemType == GDSRecord::XY && elemDataSize >= 8) {
                        int count = elemDataSize / 8;
                        for (int i = 0; i < count; ++i) {
                            qint32 x, y;
                            stream >> x >> y;
                            path.points.append(DbPoint(x, y));
                        }
                    } else if (elemDataSize > 0) {
                        stream.skipRawData(elemDataSize);
                    }
                }
                
                cell->paths.append(path);
                break;
            }
            
            case GDSRecord::SREF: {
                GDSReference ref;
                bool elemEnd = false;
                
                while (!stream.atEnd() && !elemEnd) {
                    quint16 elemLen, elemType;
                    stream >> elemLen >> elemType;
                    int elemDataSize = elemLen - 4;
                    
                    if (elemType == GDSRecord::ENDEL) {
                        elemEnd = true;
                    } else if (elemType == GDSRecord::SNAME && elemDataSize > 0) {
                        QByteArray data(elemDataSize, 0);
                        stream.readRawData(data.data(), elemDataSize);
                        while (data.endsWith('\0')) data.chop(1);
                        ref.structureName = QString::fromLatin1(data);
                    } else if (elemType == GDSRecord::STRANS && elemDataSize >= 2) {
                        quint16 flags;
                        stream >> flags;
                        ref.reflected = (flags & 0x8000) != 0;
                    } else if (elemType == GDSRecord::ANGLE && elemDataSize >= 8) {
                        QByteArray real8(8, 0);
                        stream.readRawData(real8.data(), 8);
                        ref.angle = readGDSReal8(real8);
                    } else if (elemType == GDSRecord::XY && elemDataSize >= 8) {
                        qint32 x, y;
                        stream >> x >> y;
                        ref.position = DbPoint(x, y);
                    } else if (elemDataSize > 0) {
                        stream.skipRawData(elemDataSize);
                    }
                }
                
                cell->references.append(ref);
                break;
            }
            
            case GDSRecord::AREF: {
                GDSArrayReference aref;
                bool elemEnd = false;
                
                while (!stream.atEnd() && !elemEnd) {
                    quint16 elemLen, elemType;
                    stream >> elemLen >> elemType;
                    int elemDataSize = elemLen - 4;
                    
                    if (elemType == GDSRecord::ENDEL) {
                        elemEnd = true;
                    } else if (elemType == GDSRecord::SNAME && elemDataSize > 0) {
                        QByteArray data(elemDataSize, 0);
                        stream.readRawData(data.data(), elemDataSize);
                        while (data.endsWith('\0')) data.chop(1);
                        aref.structureName = QString::fromLatin1(data);
                    } else if (elemType == GDSRecord::COLROW && elemDataSize >= 4) {
                        quint16 cols, rows;
                        stream >> cols >> rows;
                        aref.columns = cols;
                        aref.rows = rows;
                    } else if (elemType == GDSRecord::XY && elemDataSize >= 24) {
                        qint32 x1, y1, x2, y2, x3, y3;
                        stream >> x1 >> y1 >> x2 >> y2 >> x3 >> y3;
                        aref.position = DbPoint(x1, y1);
                    } else if (elemDataSize > 0) {
                        stream.skipRawData(elemDataSize);
                    }
                }
                
                cell->arrays.append(aref);
                break;
            }
            
            case GDSRecord::TEXT: {
                GDSText text;
                bool elemEnd = false;
                
                while (!stream.atEnd() && !elemEnd) {
                    quint16 elemLen, elemType;
                    stream >> elemLen >> elemType;
                    int elemDataSize = elemLen - 4;
                    
                    if (elemType == GDSRecord::ENDEL) {
                        elemEnd = true;
                    } else if (elemType == GDSRecord::LAYER && elemDataSize >= 2) {
                        qint16 val;
                        stream >> val;
                        text.layer = val;
                    } else if (elemType == GDSRecord::STRING && elemDataSize > 0) {
                        QByteArray data(elemDataSize, 0);
                        stream.readRawData(data.data(), elemDataSize);
                        while (data.endsWith('\0')) data.chop(1);
                        text.text = QString::fromLatin1(data);
                    } else if (elemType == GDSRecord::XY && elemDataSize >= 8) {
                        qint32 x, y;
                        stream >> x >> y;
                        text.position = DbPoint(x, y);
                    } else if (elemDataSize > 0) {
                        stream.skipRawData(elemDataSize);
                    }
                }
                
                cell->texts.append(text);
                break;
            }
            
            default:
                if (dataSize > 0) {
                    stream.skipRawData(dataSize);
                }
                break;
        }
    }
    
    file.close();
    
    return cell;
}

} // namespace QLayoutEDA