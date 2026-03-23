/**
 * @file GDSStreamParser.h
 * @brief GDSII 流式解析器 - 支持延迟加载
 */

#pragma once

#include "interfaces/IGDSParser.h"
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QCache>

namespace QLayoutEDA {

/**
 * @brief Cell 位置信息
 */
struct CellLocation {
    QString name;           ///< Cell 名称
    qint64 fileOffset;      ///< 文件偏移量
    qint64 fileSize;        ///< Cell 数据大小
    int boundaryCount;      ///< 边界数量估计
    int referenceCount;     ///< 引用数量估计
};

/**
 * @brief GDSII 流式解析器
 * 
 * 支持两阶段解析：
 * 1. 快速扫描：只提取 Cell 列表和位置信息
 * 2. 按需加载：根据需要加载特定 Cell 的详细数据
 */
class GDSStreamParser : public QObject, public IGDSParser {
    Q_OBJECT
    Q_INTERFACES(QLayoutEDA::IGDSParser)

public:
    explicit GDSStreamParser(QObject* parent = nullptr);
    ~GDSStreamParser() override;

    // ============ IGDSParser 接口 ============
    bool parse(const QString& filePath) override;
    GDSDataPtr getData() const override;
    int getProgress() const override;
    QString getLastError() const override;
    void cancel() override;
    void setProgressCallback(std::function<void(int)> callback) override;

    // ============ 流式解析接口 ============
    
    /**
     * @brief 阶段1：快速扫描文件元数据
     * @param filePath GDS 文件路径
     * @return 扫描是否成功
     * 
     * 只读取 Cell 列表和引用关系，不加载详细数据
     */
    bool scanMetadata(const QString& filePath);
    
    /**
     * @brief 阶段2：按需加载指定 Cell
     * @param cellName Cell 名称
     * @return Cell 数据指针，失败返回 nullptr
     */
    GDSStructurePtr loadCell(const QString& cellName);
    
    /**
     * @brief 预加载多个 Cell
     * @param cellNames Cell 名称列表
     * @return 成功加载的数量
     */
    int loadCells(const QStringList& cellNames);
    
    /**
     * @brief 获取所有 Cell 名称
     */
    QStringList getCellNames() const;
    
    /**
     * @brief 获取 Cell 位置信息
     */
    CellLocation getCellLocation(const QString& cellName) const;
    
    /**
     * @brief 获取顶层 Cell 名称
     */
    QString getTopCellName() const;
    
    /**
     * @brief 设置 Cell 缓存大小
     * @param maxCells 最大缓存 Cell 数量
     */
    void setCacheSize(int maxCells);
    
    /**
     * @brief 清除 Cell 缓存
     */
    void clearCache();
    
    /**
     * @brief 获取已缓存的 Cell 数量
     */
    int getCachedCellCount() const;

signals:
    /**
     * @brief Cell 加载完成信号
     */
    void cellLoaded(const QString& cellName);
    
    /**
     * @brief Cell 缓存清除信号
     */
    void cellEvicted(const QString& cellName);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    
    // 内部解析方法
    bool scanFile();
    GDSStructurePtr parseCellFromFile(const QString& cellName);
    bool parseCellData(QDataStream& stream, GDSStructure& cell, qint64 endOffset);
};

} // namespace QLayoutEDA