/**
 * @file GDSParser.h
 * @brief GDSII 解析器
 */

#pragma once

#include "interfaces/IGDSParser.h"
#include <QFile>
#include <QDataStream>
#include <memory>

namespace QLayoutEDA {

/**
 * @brief GDSII 解析器
 */
class GDSParser : public QObject, public IGDSParser {
    Q_OBJECT
    Q_INTERFACES(QLayoutEDA::IGDSParser)

public:
    explicit GDSParser(QObject* parent = nullptr);
    ~GDSParser() override;

    // IGDSParser 接口
    bool parse(const QString& filePath) override;
    GDSDataPtr getData() const override;
    int getProgress() const override;
    QString getLastError() const override;
    void cancel() override;
    void setProgressCallback(std::function<void(int)> callback) override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    
    // 记录解析
    bool parseRecord(QDataStream& stream);
    
    // 库级别解析
    bool parseHeader(QDataStream& stream, int dataSize);
    bool parseLibName(QDataStream& stream, int dataSize);
    bool parseUnits(QDataStream& stream, int dataSize);
    
    // 结构级别解析
    bool parseBeginStructure(QDataStream& stream, int dataSize);
    bool parseStructureName(QDataStream& stream, int dataSize);
    bool parseEndStructure();
    
    // 元素解析
    bool parseBoundary(QDataStream& stream);
    bool parsePath(QDataStream& stream);
    bool parseSRef(QDataStream& stream);
    bool parseARef(QDataStream& stream);
    bool parseText(QDataStream& stream);
    bool parseBox(QDataStream& stream);
    
    // 辅助函数
    bool skipData(QDataStream& stream, int dataSize);
    void calculateBoundingBox(GDSStructure& structure);
};

} // namespace QLayoutEDA