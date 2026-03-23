/**
 * @file SpatialIndex.cpp
 * @brief R-Tree spatial index implementation
 */

#include "SpatialIndex.h"
#include <QHash>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace QLayoutEDA {

//=============================================================================
// R-Tree Node
//=============================================================================

struct RTreeNode {
    DbRect bounds;
    bool isLeaf = true;
    int level = 0;
    
    // Leaf node: object IDs
    QVector<quint64> objectIds;
    
    // Internal node: child indices
    QVector<int> children;
    
    RTreeNode() = default;
};

//=============================================================================
// R-Tree Implementation
//=============================================================================

class RTree {
public:
    static constexpr int MAX_ENTRIES = 16;
    static constexpr int MIN_ENTRIES = 4;
    
    QVector<RTreeNode> nodes;
    int rootIndex = -1;
    
    void clear() {
        nodes.clear();
        rootIndex = -1;
    }
    
    int depth() const {
        if (rootIndex < 0 || nodes.isEmpty()) return 0;
        return nodes[rootIndex].level + 1;
    }
    
    static DbRect mergeBounds(const DbRect& a, const DbRect& b) {
        return DbRect(qMin(a.left, b.left), qMin(a.top, b.top),
                      qMax(a.right, b.right), qMax(a.bottom, b.bottom));
    }
    
    static qint64 area(const DbRect& r) {
        return r.width() * r.height();
    }
    
    static qint64 expansionArea(const DbRect& r, const DbRect& add) {
        return area(mergeBounds(r, add)) - area(r);
    }
    
    int chooseLeaf(const DbRect& rect) {
        if (rootIndex < 0) return -1;
        
        int nodeIndex = rootIndex;
        
        while (!nodes[nodeIndex].isLeaf) {
            int bestChild = -1;
            qint64 minExpansion = LLONG_MAX;
            qint64 minArea = LLONG_MAX;
            
            for (int childIdx : nodes[nodeIndex].children) {
                qint64 expansion = expansionArea(nodes[childIdx].bounds, rect);
                qint64 childArea = area(nodes[childIdx].bounds);
                
                if (expansion < minExpansion || 
                    (expansion == minExpansion && childArea < minArea)) {
                    minExpansion = expansion;
                    minArea = childArea;
                    bestChild = childIdx;
                }
            }
            
            nodeIndex = bestChild;
        }
        
        return nodeIndex;
    }
    
    void adjustBounds(int nodeIndex) {
        if (nodeIndex < 0) return;
        
        RTreeNode& node = nodes[nodeIndex];
        node.bounds = DbRect();
        
        if (node.isLeaf) {
            bool first = true;
            for (quint64 id : node.objectIds) {
                // Bounds will be updated from objects hash
                Q_UNUSED(id);
            }
        } else {
            bool first = true;
            for (int childIdx : node.children) {
                if (first) {
                    node.bounds = nodes[childIdx].bounds;
                    first = false;
                } else {
                    node.bounds = mergeBounds(node.bounds, nodes[childIdx].bounds);
                }
            }
        }
    }
};

//=============================================================================
// SpatialIndex Implementation
//=============================================================================

struct SpatialIndex::Impl {
    QHash<quint64, SpatialObject> objects;
    RTree rtree;
    DbRect globalBounds;
    bool needsRebuild = true;
};

SpatialIndex::SpatialIndex() : d(new Impl) {}
SpatialIndex::~SpatialIndex() = default;

void SpatialIndex::insert(const SpatialObject& obj)
{
    d->objects[obj.id] = obj;
    
    // Update global bounds
    if (d->objects.size() == 1) {
        d->globalBounds = obj.bounds;
    } else {
        d->globalBounds = RTree::mergeBounds(d->globalBounds, obj.bounds);
    }
    
    d->needsRebuild = true;
}

void SpatialIndex::remove(quint64 id)
{
    d->objects.remove(id);
    d->needsRebuild = true;
}

void SpatialIndex::update(quint64 id, const DbRect& newBounds)
{
    auto it = d->objects.find(id);
    if (it != d->objects.end()) {
        it->bounds = newBounds;
        d->needsRebuild = true;
    }
}

QVector<SpatialObject> SpatialIndex::query(const DbRect& rect) const
{
    QVector<SpatialObject> result;
    
    // Simple linear search with bounding box intersection test
    for (const auto& obj : d->objects) {
        // Check if bounding boxes intersect
        if (obj.bounds.left <= rect.right && obj.bounds.right >= rect.left &&
            obj.bounds.top <= rect.bottom && obj.bounds.bottom >= rect.top) {
            result.append(obj);
        }
    }
    
    return result;
}

QVector<SpatialObject> SpatialIndex::queryPoint(const DbPoint& point) const
{
    QVector<SpatialObject> result;
    
    for (const auto& obj : d->objects) {
        if (obj.bounds.contains(point)) {
            result.append(obj);
        }
    }
    
    return result;
}

QVector<SpatialObject> SpatialIndex::queryByLayer(int layer, const DbRect& rect) const
{
    QVector<SpatialObject> result;
    
    for (const auto& obj : d->objects) {
        if (obj.layer == layer &&
            obj.bounds.left <= rect.right && obj.bounds.right >= rect.left &&
            obj.bounds.top <= rect.bottom && obj.bounds.bottom >= rect.top) {
            result.append(obj);
        }
    }
    
    return result;
}

void SpatialIndex::clear()
{
    d->objects.clear();
    d->rtree.clear();
    d->globalBounds = DbRect();
    d->needsRebuild = true;
}

void SpatialIndex::rebuild()
{
    d->rtree.clear();
    
    if (d->objects.isEmpty()) return;
    
    // Create root node
    RTreeNode root;
    root.isLeaf = true;
    root.level = 0;
    
    // Add all objects to root
    for (const auto& obj : d->objects) {
        root.objectIds.append(obj.id);
        root.bounds = RTree::mergeBounds(root.bounds, obj.bounds);
    }
    
    // If too many objects, split into multiple levels
    if (root.objectIds.size() > RTree::MAX_ENTRIES) {
        // Sort objects by position for better grouping
        QVector<SpatialObject> sortedObjects = d->objects.values().toVector();
        std::sort(sortedObjects.begin(), sortedObjects.end(), 
                  [](const SpatialObject& a, const SpatialObject& b) {
                      if (a.bounds.left != b.bounds.left) 
                          return a.bounds.left < b.bounds.left;
                      return a.bounds.top < b.bounds.top;
                  });
        
        // Build hierarchical tree
        buildTreeRecursive(sortedObjects, 0);
    } else {
        d->rtree.nodes.append(root);
        d->rtree.rootIndex = 0;
    }
    
    d->needsRebuild = false;
}

void SpatialIndex::buildTreeRecursive(const QVector<SpatialObject>& objects, int level)
{
    Q_UNUSED(objects);
    Q_UNUSED(level);
    // Simplified: just create a single level
    // For production, implement STR (Sort-Tile-Recursive) algorithm
    
    RTreeNode root;
    root.isLeaf = true;
    root.level = 0;
    
    for (const auto& obj : objects) {
        root.objectIds.append(obj.id);
        root.bounds = RTree::mergeBounds(root.bounds, obj.bounds);
    }
    
    d->rtree.nodes.append(root);
    d->rtree.rootIndex = d->rtree.nodes.size() - 1;
}

int SpatialIndex::count() const
{
    return d->objects.size();
}

DbRect SpatialIndex::getGlobalBounds() const
{
    return d->globalBounds;
}

} // namespace QLayoutEDA