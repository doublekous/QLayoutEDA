# KLayout 渲染引擎实现参考

## 核心概念

### 1. 层级结构 (Hierarchy)

GDSII 文件使用层级结构：
- **Library** → 包含多个 Structure
- **Structure** → 包含 Boundaries, Paths, Texts, References
- **Reference (SREF)** → 引用另一个 Structure
- **Array Reference (AREF)** → 阵列引用

### 2. 渲染流程

```
顶层结构 (top_cell)
├── Boundaries (直接绘制)
├── Paths (直接绘制)
├── Texts (直接绘制)
├── SREF → sub_cell1 (递归渲染)
│   ├── Boundaries
│   └── SREF → sub_cell2
│       └── ...
└── AREF → sub_cell3 x 10x10 (展开渲染)
    └── ...
```

### 3. KLayout 关键实现

```cpp
// 简化的 KLayout 渲染逻辑

class LayoutView {
    void render(LayoutObject* obj, Transform t, int depth) {
        if (depth > MAX_DEPTH) return;  // 防止无限递归
        
        // 视口裁剪
        if (!obj->bbox().intersects(viewport)) return;
        
        if (auto* cell = dynamic_cast<Cell*>(obj)) {
            // 渲染 Cell 的所有元素
            for (auto& shape : cell->shapes()) {
                if (shape.bbox().intersects(viewport)) {
                    renderShape(shape, t);
                }
            }
            
            // 递归渲染实例
            for (auto& inst : cell->instances()) {
                Transform nt = t * inst.transform();
                render(inst.cell(), nt, depth + 1);
            }
        }
    }
};
```

### 4. 变换累积

每次引用都有变换：
- 位置偏移 (x, y)
- 旋转角度 (angle)
- 缩放倍数 (magnification)
- 镜像 (reflection)

```cpp
Transform combine(Transform parent, Reference ref) {
    Transform result;
    result.offset = parent.offset + parent.scale * ref.offset.rotated(parent.angle);
    result.angle = parent.angle + ref.angle;
    result.scale = parent.scale * ref.magnification;
    result.mirror = parent.mirror ^ ref.reflection;
    return result;
}
```

### 5. 性能优化

#### 视口裁剪
```cpp
// 只渲染可见区域
if (!bbox.intersects(viewport)) {
    return; // 跳过整个实例及其子树
}
```

#### 实例缓存
```cpp
// 缓存已渲染的 Cell
struct CellCache {
    QPicture picture;
    QRectF bbox;
    bool valid;
};

QHash<QString, CellCache> cellCache;
```

#### LOD (Level of Detail)
```cpp
// 根据缩放级别简化渲染
if (scale < 0.01) {
    // 只画边界框
    drawBoundingBox(cell->bbox());
} else if (scale < 0.1) {
    // 简化渲染
    drawSimplified(cell);
} else {
    // 完整渲染
    drawFull(cell);
}
```

### 6. 阵列引用 (AREF)

```cpp
void renderArray(AREF aref, Transform t) {
    for (int row = 0; row < aref.rows; row++) {
        for (int col = 0; col < aref.columns; col++) {
            Transform nt = t;
            nt.offset += col * aref.columnVector;
            nt.offset += row * aref.rowVector;
            render(aref.targetCell, nt);
        }
    }
}
```

## 实现要点

1. **从顶层结构开始** - 找到 library 的顶层 cell
2. **递归渲染引用** - 深度遍历所有引用
3. **累积变换** - 每层引用都要累积变换
4. **视口裁剪** - 尽早剔除不可见对象
5. **限制深度** - 防止循环引用导致无限递归

## 测试验证

使用 130MB GDS 文件验证：
- 顶层结构：top_cell2
- 引用数量：1,484,282
- 预期：应该看到完整的版图布局