# 选择功能实现方案

## 功能需求

1. **单选** - 点击选择单个对象
2. **框选** - 拖拽选择多个对象（橡皮筋）
3. **全选** - Ctrl+A 选择所有对象
4. **取消选择** - 点击空白处或 Esc

## 实现方案

### CanvasWidget 新增成员

```cpp
// 选择状态
QSet<quint64> m_selectedObjects;
quint64 m_hoveredObject = 0;

// 选择方法
void selectObject(quint64 id);
void deselectObject(quint64 id);
void selectAll();
void clearSelection();
QVector<quint64> getSelectedObjects() const;
bool isSelected(quint64 id) const;
```

### 信号

```cpp
signals:
    void selectionChanged(const QVector<quint64>& selected);
    void objectClicked(quint64 id, const QPointF& pos);
```

### 鼠标交互

```cpp
void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    QPointF worldPos = screenToWorld(event->pos());
    
    if (m_currentTool == Tool::Select) {
        // 检测点击的对象
        auto hitObjects = hitTest(worldPos);
        
        if (hitObjects.isEmpty()) {
            // 点击空白处，取消选择
            clearSelection();
        } else {
            // 选择对象
            quint64 id = hitObjects.first();
            
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+点击：切换选择
                if (m_selectedObjects.contains(id)) {
                    deselectObject(id);
                } else {
                    selectObject(id);
                }
            } else {
                // 普通点击：替换选择
                clearSelection();
                selectObject(id);
            }
        }
        update();
    }
}
```

### 橡皮筋选择

```cpp
void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_rubberBandActive) {
        m_rubberBandEnd = event->pos();
        update(); // 重绘橡皮筋
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_rubberBandActive) {
        // 结束框选
        QRectF rubberRect(m_rubberBandStart, m_rubberBandEnd);
        auto objects = hitTestRect(screenToWorldRect(rubberRect));
        
        for (auto id : objects) {
            selectObject(id);
        }
        
        m_rubberBandActive = false;
        update();
    }
}
```

### 渲染选择高亮

```cpp
void CanvasWidget::drawSelection(QPainter& painter) {
    painter.save();
    
    // 选择高亮
    QPen selectionPen(QColor(255, 255, 0), 3); // 黄色边框
    QBrush selectionBrush(QColor(255, 255, 0, 50)); // 半透明黄色填充
    
    painter.setPen(selectionPen);
    painter.setBrush(selectionBrush);
    
    for (auto id : m_selectedObjects) {
        auto bounds = getObjectBounds(id);
        painter.drawRect(worldToScreenRect(bounds));
    }
    
    painter.restore();
}
```

### 快捷键

```cpp
void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        clearSelection();
        update();
    } else if (event->key() == Qt::Key_A && 
               event->modifiers() & Qt::ControlModifier) {
        selectAll();
        update();
    }
}
```

## 与 MainWindow 集成

```cpp
// MainWindow.cpp
void MainWindow::setupConnections() {
    connect(m_canvas, &CanvasWidget::selectionChanged, 
            this, [this](const QVector<quint64>& selected) {
        if (selected.size() == 1) {
            // 显示单个对象属性
            m_propertyPanel->setObject(...);
        } else {
            m_propertyPanel->clear();
        }
    });
}
```