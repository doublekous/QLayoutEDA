/**
 * @file IGDSParser.h
 * @brief GDS 解析器接口
 */

#pragma once

#include "core/Types.h"
#include <QString>
#include <functional>
#include <QObject>

namespace QLayoutEDA {

/**
 * @brief GDS 解析器接口
 */
class IGDSParser {
public:
    virtual ~IGDSParser() = default;
    
    virtual bool parse(const QString& filePath) = 0;
    virtual GDSDataPtr getData() const = 0;
    virtual int getProgress() const = 0;
    virtual QString getLastError() const = 0;
    virtual void cancel() = 0;
    virtual void setProgressCallback(std::function<void(int)> callback) = 0;
};

} // namespace QLayoutEDA

Q_DECLARE_INTERFACE(QLayoutEDA::IGDSParser, "com.QLayoutEDA.IGDSParser/1.0")