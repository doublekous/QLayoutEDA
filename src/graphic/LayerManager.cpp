/**
 * @file LayerManager.cpp
 * @brief Layer manager implementation
 */

#include "LayerManager.h"
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <algorithm>

namespace QLayoutEDA {

struct LayerManager::Impl {
    QHash<int, LayerData> layers;
    
    LayerStyle createDefaultStyle(int layer) {
        LayerStyle style;
        style.layer = layer;
        int hue = (layer * 137) % 360;
        style.fillColor = QColor::fromHsl(hue, 180, 160, 180);
        style.borderColor = QColor::fromHsl(hue, 200, 100);
        style.borderWidth = 1;
        style.visible = true;
        style.selectable = true;
        return style;
    }
};

LayerManager::LayerManager() : d(new Impl) {}
LayerManager::~LayerManager() = default;

void LayerManager::addLayer(int layer, const QString& name)
{
    if (d->layers.contains(layer)) {
        qWarning() << "Layer already exists:" << layer;
        return;
    }
    
    LayerData data;
    data.layer = layer;
    data.name = name.isEmpty() ? QString("Layer %1").arg(layer) : name;
    data.style = d->createDefaultStyle(layer);
    data.objectCount = 0;
    
    d->layers[layer] = data;
    qDebug() << "Layer added:" << layer << data.name;
}

void LayerManager::removeLayer(int layer)
{
    if (d->layers.remove(layer) > 0) {
        qDebug() << "Layer removed:" << layer;
    }
}

bool LayerManager::hasLayer(int layer) const
{
    return d->layers.contains(layer);
}

QVector<int> LayerManager::getAllLayers() const
{
    return d->layers.keys().toVector();
}

int LayerManager::getLayerCount() const
{
    return d->layers.size();
}

QString LayerManager::getLayerName(int layer) const
{
    return d->layers.value(layer).name;
}

void LayerManager::setLayerName(int layer, const QString& name)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].name = name;
    }
}

void LayerManager::setLayerStyle(int layer, const LayerStyle& style)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].style = style;
    } else {
        LayerData data;
        data.layer = layer;
        data.name = QString("Layer %1").arg(layer);
        data.style = style;
        d->layers[layer] = data;
    }
}

LayerStyle LayerManager::getLayerStyle(int layer) const
{
    if (d->layers.contains(layer)) {
        return d->layers[layer].style;
    }
    return d->createDefaultStyle(layer);
}

void LayerManager::setLayerVisible(int layer, bool visible)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].style.visible = visible;
    }
}

bool LayerManager::isLayerVisible(int layer) const
{
    return d->layers.value(layer).style.visible;
}

void LayerManager::setAllVisible(bool visible)
{
    for (auto it = d->layers.begin(); it != d->layers.end(); ++it) {
        it->style.visible = visible;
    }
}

QVector<int> LayerManager::getVisibleLayers() const
{
    QVector<int> result;
    for (auto it = d->layers.begin(); it != d->layers.end(); ++it) {
        if (it->style.visible) {
            result.append(it.key());
        }
    }
    return result;
}

void LayerManager::setLayerSelectable(int layer, bool selectable)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].style.selectable = selectable;
    }
}

bool LayerManager::isLayerSelectable(int layer) const
{
    return d->layers.value(layer).style.selectable;
}

void LayerManager::setObjectCount(int layer, int count)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].objectCount = count;
    }
}

int LayerManager::getObjectCount(int layer) const
{
    return d->layers.value(layer).objectCount;
}

void LayerManager::setDrawOrder(int layer, int order)
{
    if (d->layers.contains(layer)) {
        d->layers[layer].style.fillPattern = QString::number(order);
    }
}

int LayerManager::getDrawOrder(int layer) const
{
    QString pattern = d->layers.value(layer).style.fillPattern;
    bool ok;
    int order = pattern.toInt(&ok);
    return ok ? order : layer;
}

QVector<int> LayerManager::getLayersInDrawOrder() const
{
    QVector<int> layers = d->layers.keys().toVector();
    std::sort(layers.begin(), layers.end(), [this](int a, int b) {
        return getDrawOrder(a) < getDrawOrder(b);
    });
    return layers;
}

bool LayerManager::saveToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for saving:" << filePath;
        return false;
    }
    
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_15);
    
    out << QString("QLAYOUT_LAYERS_V1");
    out << d->layers.size();
    
    for (auto it = d->layers.begin(); it != d->layers.end(); ++it) {
        const LayerData& data = it.value();
        out << data.layer;
        out << data.name;
        out << data.style.layer;
        out << data.style.fillColor;
        out << data.style.borderColor;
        out << data.style.borderWidth;
        out << data.style.visible;
        out << data.style.selectable;
        out << data.style.fillPattern;
        out << data.objectCount;
    }
    
    qDebug() << "Layer config saved to:" << filePath;
    return true;
}

bool LayerManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for loading:" << filePath;
        return false;
    }
    
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_15);
    
    QString magic;
    in >> magic;
    if (magic != "QLAYOUT_LAYERS_V1") {
        qWarning() << "Invalid layer config file format";
        return false;
    }
    
    d->layers.clear();
    
    int count;
    in >> count;
    
    for (int i = 0; i < count; ++i) {
        LayerData data;
        in >> data.layer;
        in >> data.name;
        in >> data.style.layer;
        in >> data.style.fillColor;
        in >> data.style.borderColor;
        in >> data.style.borderWidth;
        in >> data.style.visible;
        in >> data.style.selectable;
        in >> data.style.fillPattern;
        in >> data.objectCount;
        
        d->layers[data.layer] = data;
    }
    
    qDebug() << "Layer config loaded from:" << filePath << "count:" << count;
    return true;
}

} // namespace QLayoutEDA