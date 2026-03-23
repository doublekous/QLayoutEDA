# QLayoutEDA 渲染架构重构方案

## 一、问题诊断

### 1.1 当前架构问题

| 问题 | 当前实现 | KLayout 实现 |
|------|----------|--------------|
| **视口变换** | 分散在多处 | 集中在 Viewport 类 |
| **坐标转换** | 直接计算 | 变换矩阵统一管理 |
| **缓存** | 零散实现 | ImageCacheEntry + CellCache 统一 |
| **异步渲染** | 未启用 | RedrawThread 多线程 |

### 1.2 根本原因

**缺乏统一的视口变换管理**，导致：
- 缩放位置计算错误
- 缓存键不正确
- 坐标转换混乱

---

## 二、KLayout 核心架构

### 2.1 Viewport 类（核心）

```cpp
class Viewport {
  unsigned int m_width, m_height;  // 像素尺寸
  DCplxTrans m_trans;              // 世界→像素变换矩阵
  DBox m_target_box;               // 目标区域（微米）
  DCplxTrans m_global_trans;       // 全局变换（旋转/镜像）
  
  void set_box(const DBox& box);   // 设置显示区域
  void set_trans(const DCplxTrans& trans);  // 直接设置变换
  DBox box() const;                // 获取当前显示区域
};
```

**关键算法 - set_box()**:
```cpp
void Viewport::set_box(const DBox& in_box) {
  m_target_box = in_box;
  
  // 计算缩放因子：显示区域 / 像素尺寸
  double w = box.right() - box.left();
  double h = box.top() - box.bottom();
  double fx = w / width();
  double fy = h / height();
  double f = max(fx, fy);  // 保持宽高比
  
  // 计算位移（居中）
  double mx = box.right() + box.left();
  double my = box.top() + box.bottom();
  double dx = floor(0.5 + (mx/f - width()) * 0.5);
  double dy = floor(0.5 + (my/f - height()) * 0.5);
  
  // 创建变换矩阵
  m_trans = DCplxTrans(1.0/f, 0.0, false, DVector(-dx, -dy));
}
```

### 2.2 ImageCacheEntry（图像缓存）

```cpp
class ImageCacheEntry {
  Viewport m_viewport;           // 视口信息
  vector<RedrawLayerInfo> m_layers;  // 图层信息
  bool m_precious;               // fitAll 结果，优先保留
  BitmapCanvasData m_data;       // 渲染数据
  
  bool equals(const Viewport& vp, const vector<RedrawLayerInfo>& layers) const;
};
```

### 2.3 渲染流程

```
用户操作（缩放/平移）
    ↓
更新 Viewport
    ↓
查找 ImageCache
    ├─ 命中 → restore_data()
    └─ 未命中 → 创建 ImageCacheEntry → 启动 RedrawThread
    ↓
RedrawThread 异步渲染
    ↓
渲染完成 → 更新显示
```

---

## 三、重构方案

### 3.1 新增 Viewport 类

**文件**: `src/graphic/Viewport.h`

```cpp
#pragma once

#include <QTransform>
#include <QRectF>

namespace QLayoutEDA {

/**
 * @brief 视口类 - 参考 KLayout 实现
 * 
 * 管理世界坐标到像素坐标的变换
 */
class Viewport {
public:
    Viewport();
    Viewport(int width, int height, const QRectF& targetBox);
    
    // 设置像素尺寸
    void setSize(int width, int height);
    
    // 设置目标区域（世界坐标，微米）
    void setBox(const QRectF& box);
    
    // 直接设置变换矩阵
    void setTransform(const QTransform& trans);
    
    // 获取变换矩阵
    const QTransform& transform() const { return m_trans; }
    
    // 坐标转换
    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
    
    // 获取当前显示区域（世界坐标）
    QRectF box() const;
    
    // 获取缩放级别
    double scale() const;
    
    // 缩放操作
    void zoom(double factor, const QPointF& center);  // 以指定点为中心缩放
    void pan(const QPointF& delta);  // 平移
    
    int width() const { return m_width; }
    int height() const { return m_height; }
    
private:
    int m_width, m_height;
    QTransform m_trans;      // 世界→屏幕变换
    QTransform m_invTrans;   // 屏幕→世界变换（缓存）
    QRectF m_targetBox;      // 目标区域
    
    void updateTransform();
};

} // namespace QLayoutEDA
```

### 3.2 实现 Viewport.cpp

```cpp
#include "Viewport.h"
#include <QtMath>

namespace QLayoutEDA {

Viewport::Viewport()
    : m_width(800), m_height(600)
{
    updateTransform();
}

Viewport::Viewport(int width, int height, const QRectF& targetBox)
    : m_width(width), m_height(height), m_targetBox(targetBox)
{
    updateTransform();
}

void Viewport::setSize(int width, int height)
{
    m_width = width;
    m_height = height;
    updateTransform();
}

void Viewport::setBox(const QRectF& box)
{
    m_targetBox = box;
    updateTransform();
}

void Viewport::updateTransform()
{
    if (m_targetBox.isEmpty()) {
        m_trans.reset();
        m_invTrans.reset();
        return;
    }
    
    // 计算缩放因子（保持宽高比）
    double w = m_targetBox.width();
    double h = m_targetBox.height();
    double fx = w / m_width;
    double fy = h / m_height;
    double f = qMax(fx, fy);
    
    if (f < 1e-13) f = 0.001;
    
    // 计算位移（居中）
    double mx = m_targetBox.left() + m_targetBox.width() / 2.0;
    double my = m_targetBox.top() + m_targetBox.height() / 2.0;
    
    // 创建变换矩阵
    // 注意：Qt 的 Y 轴向下，需要翻转
    m_trans.reset();
    m_trans.translate(m_width / 2.0, m_height / 2.0);  // 移到屏幕中心
    m_trans.scale(1.0 / f, -1.0 / f);  // 缩放 + Y轴翻转
    m_trans.translate(-mx, -my);  // 移到世界坐标中心
    
    m_invTrans = m_trans.inverted();
}

QPointF Viewport::worldToScreen(const QPointF& world) const
{
    return m_trans.map(world);
}

QPointF Viewport::screenToWorld(const QPointF& screen) const
{
    return m_invTrans.map(screen);
}

QRectF Viewport::box() const
{
    QPointF tl = screenToWorld(QPointF(0, 0));
    QPointF br = screenToWorld(QPointF(m_width, m_height));
    return QRectF(tl, br);
}

double Viewport::scale() const
{
    return m_trans.m11();  // 缩放因子
}

void Viewport::zoom(double factor, const QPointF& screenCenter)
{
    // 获取缩放中心的世界坐标
    QPointF worldCenter = screenToWorld(screenCenter);
    
    // 计算新的目标区域
    QRectF currentBox = box();
    double newW = currentBox.width() / factor;
    double newH = currentBox.height() / factor;
    
    // 以世界中心为新区域的中心
    QRectF newBox(
        worldCenter.x() - newW / 2,
        worldCenter.y() - newH / 2,
        newW, newH
    );
    
    setBox(newBox);
}

void Viewport::pan(const QPointF& screenDelta)
{
    QPointF worldDelta = m_invTrans.map(screenDelta) - m_invTrans.map(QPointF(0, 0));
    
    QRectF currentBox = box();
    currentBox.translate(-worldDelta.x(), -worldDelta.y());
    setBox(currentBox);
}

} // namespace QLayoutEDA
```

### 3.3 修改 CanvasWidget 使用 Viewport

```cpp
// CanvasWidget.h
#include "../graphic/Viewport.h"

class CanvasWidget : public QWidget {
private:
    Viewport m_viewport;  // 统一的视口管理
    ImageCache m_imageCache;
    
protected:
    void wheelEvent(QWheelEvent* event) override {
        QPointF mousePos = event->position();
        double delta = event->angleDelta().y() / 120.0;
        double factor = std::pow(1.2, delta);
        
        m_viewport.zoom(factor, mousePos);
        update();
    }
    
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        
        // 检查缓存
        QImage cached = m_imageCache.get(m_viewport.box(), m_currentStructure);
        if (!cached.isNull()) {
            painter.drawImage(0, 0, cached);
            return;
        }
        
        // 渲染
        QImage image = renderToImage();
        m_imageCache.put(m_viewport.box(), m_currentStructure, image);
        painter.drawImage(0, 0, image);
    }
};
```

---

## 四、实施计划

### 阶段 1：Viewport 类实现（1天）

| 任务 | 文件 |
|------|------|
| 创建 Viewport.h | src/graphic/Viewport.h |
| 创建 Viewport.cpp | src/graphic/Viewport.cpp |
| 更新 CMakeLists.txt | CMakeLists.txt |

### 阶段 2：CanvasWidget 重构（2天）

| 任务 | 说明 |
|------|------|
| 移除分散的坐标转换 | 使用 Viewport 统一管理 |
| 重构 wheelEvent | 使用 Viewport::zoom() |
| 重构 paintEvent | 使用 Viewport::worldToScreen() |

### 阶段 3：缓存系统重构（1天）

| 任务 | 说明 |
|------|------|
| 重构 ImageCache | 使用 Viewport.box() 作为键 |
| 实现 ImageCacheEntry | 参考 KLayout |

---

## 五、预期效果

| 功能 | 当前 | 重构后 |
|------|------|--------|
| **缩放位置** | 错误 | ✅ 正确 |
| **缓存命中** | 不稳定 | ✅ 可靠 |
| **代码结构** | 分散 | ✅ 统一 |
| **性能** | 4秒 | ✅ 缓存命中 <100ms |

---

**文档版本**: v1.0
**日期**: 2026-03-23
**参考**: KLayout 源码 layViewport.h/cc, layLayoutCanvas.h/cc