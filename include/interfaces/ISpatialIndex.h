/**
 * @file ISpatialIndex.h
 * @brief 空间索引接口
 */

#pragma once

#include "core/Types.h"
#include <QVector>
#include <QObject>

namespace QLayoutEDA {

/**
 * @brief 空间对象
 */
struct SpatialObject {
    quint64 id;         ///< 对象ID
    int layer;          ///< 层号
    DbRect bounds;      ///< 边界框
    int type;           ///< 类型 (0=boundary, 1=path, 2=text, 3=reference)
    
    SpatialObject() : id(0), layer(0), type(0) {}
};

/**
 * @brief 空间索引接口
 * 
 * 提供高效的空间查询能力
 */
class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;
    
    /**
     * @brief 插入对象
     */
    virtual void insert(const SpatialObject& obj) = 0;
    
    /**
     * @brief 删除对象
     */
    virtual void remove(quint64 id) = 0;
    
    /**
     * @brief 更新对象
     */
    virtual void update(quint64 id, const DbRect& newBounds) = 0;
    
    /**
     * @brief 范围查询
     */
    virtual QVector<SpatialObject> query(const DbRect& rect) const = 0;
    
    /**
     * @brief 点查询
     */
    virtual QVector<SpatialObject> queryPoint(const DbPoint& point) const = 0;
    
    /**
     * @brief 按图层查询
     */
    virtual QVector<SpatialObject> queryByLayer(int layer, const DbRect& rect) const = 0;
    
    /**
     * @brief 清空索引
     */
    virtual void clear() = 0;
    
    /**
     * @brief 重建索引
     */
    virtual void rebuild() = 0;
    
    /**
     * @brief 获取对象数量
     */
    virtual int count() const = 0;
    
    /**
     * @brief 获取全局边界框
     */
    virtual DbRect getGlobalBounds() const = 0;
};

} // namespace QLayoutEDA

Q_DECLARE_INTERFACE(QLayoutEDA::ISpatialIndex, "com.QLayoutEDA.ISpatialIndex/1.0")