/**
 * @file SpatialIndex.h
 * @brief R-Tree spatial index
 */

#pragma once

#include "interfaces/ISpatialIndex.h"
#include <QScopedPointer>

namespace QLayoutEDA {

class SpatialIndex : public ISpatialIndex {
public:
    explicit SpatialIndex();
    ~SpatialIndex() override;
    
    void insert(const SpatialObject& obj) override;
    void remove(quint64 id) override;
    void update(quint64 id, const DbRect& newBounds) override;
    QVector<SpatialObject> query(const DbRect& rect) const override;
    QVector<SpatialObject> queryPoint(const DbPoint& point) const override;
    QVector<SpatialObject> queryByLayer(int layer, const DbRect& rect) const override;
    void clear() override;
    void rebuild() override;
    int count() const override;
    DbRect getGlobalBounds() const override;

private:
    void buildTreeRecursive(const QVector<SpatialObject>& objects, int level);
    
    struct Impl;
    QScopedPointer<Impl> d;
};

} // namespace QLayoutEDA