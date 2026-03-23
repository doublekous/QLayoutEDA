/**
 * @file LayerManager.h
 * @brief Layer manager
 */

#pragma once

#include "core/Types.h"
#include <QHash>
#include <QScopedPointer>

namespace QLayoutEDA {

class LayerManager {
public:
    LayerManager();
    ~LayerManager();
    
    // Layer management
    void addLayer(int layer, const QString& name);
    void removeLayer(int layer);
    bool hasLayer(int layer) const;
    QVector<int> getAllLayers() const;
    int getLayerCount() const;
    
    // Layer info
    QString getLayerName(int layer) const;
    void setLayerName(int layer, const QString& name);
    
    // Style
    void setLayerStyle(int layer, const LayerStyle& style);
    LayerStyle getLayerStyle(int layer) const;
    
    // Visibility
    void setLayerVisible(int layer, bool visible);
    bool isLayerVisible(int layer) const;
    void setAllVisible(bool visible);
    QVector<int> getVisibleLayers() const;
    
    // Selectable
    void setLayerSelectable(int layer, bool selectable);
    bool isLayerSelectable(int layer) const;
    
    // Object count
    void setObjectCount(int layer, int count);
    int getObjectCount(int layer) const;
    
    // Draw order
    void setDrawOrder(int layer, int order);
    int getDrawOrder(int layer) const;
    QVector<int> getLayersInDrawOrder() const;
    
    // Serialization
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

private:
    struct Impl;
    QScopedPointer<Impl> d;
};

} // namespace QLayoutEDA