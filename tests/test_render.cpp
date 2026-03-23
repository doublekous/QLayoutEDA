/**
 * @file test_render.cpp
 * @brief 渲染测试 - 简化版
 */

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QElapsedTimer>
#include "../parser/GDSParser.h"

using namespace QLayoutEDA;

void renderStructureSimple(QPainter& painter, const GDSStructurePtr& structure, 
                           double scale, double offsetY);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QString testFile = "D:\\testFile\\20250530\\test22.gds";
    if (argc > 1) testFile = argv[1];
    
    qDebug() << "=== 渲染测试 ===";
    qDebug() << "文件:" << testFile;
    
    GDSParser parser;
    QElapsedTimer timer;
    timer.start();
    
    if (!parser.parse(testFile)) {
        qCritical() << "解析失败:" << parser.getLastError();
        return 1;
    }
    
    qDebug() << "解析耗时:" << timer.elapsed() << "ms";
    
    auto data = parser.getData();
    if (!data || data->structures.isEmpty()) {
        qCritical() << "无数据";
        return 1;
    }
    
    // 找最大结构
    QString topStructure;
    qint64 maxObjs = 0;
    for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
        auto s = it.value();
        if (!s) continue;
        qint64 cnt = s->boundaries.size() + s->paths.size();
        if (cnt > maxObjs) { maxObjs = cnt; topStructure = it.key(); }
    }
    
    qDebug() << "顶层结构:" << topStructure;
    
    auto structure = data->getStructure(topStructure);
    if (!structure) return 1;
    
    // 计算边界
    qint64 minX=LLONG_MAX, maxX=LLONG_MIN, minY=LLONG_MAX, maxY=LLONG_MIN;
    for (const auto& b : structure->boundaries) {
        for (const auto& p : b.points) {
            minX = qMin(minX, p.x); maxX = qMax(maxX, p.x);
            minY = qMin(minY, p.y); maxY = qMax(maxY, p.y);
        }
    }
    
    qint64 dbW = maxX - minX, dbH = maxY - minY;
    qDebug() << "边界:" << minX << minY << maxX << maxY;
    qDebug() << "尺寸:" << dbW << "x" << dbH;
    
    // 图像
    int imgW = 1920, imgH = 1080;
    QImage image(imgW, imgH, QImage::Format_ARGB32);
    image.fill(QColor(30, 30, 30));
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 变换
    double scale = qMin((imgW-100.0)/dbW, (imgH-100.0)/dbH);
    double offsetY = (imgH + dbH * scale) / 2.0 + minY * scale;
    
    qDebug() << "缩放:" << scale;
    
    timer.restart();
    renderStructureSimple(painter, structure, scale, offsetY);
    qDebug() << "渲染耗时:" << timer.elapsed() << "ms";
    
    painter.end();
    
    QString out = "E:\\QLayoutEDA\\test_output.png";
    image.save(out);
    qDebug() << "保存到:" << out;
    
    return 0;
}

void renderStructureSimple(QPainter& painter, const GDSStructurePtr& s, double scale, double offsetY)
{
    int cnt = 0;
    for (const auto& b : s->boundaries) {
        if (b.points.size() < 3) continue;
        int hue = (b.layer * 137) % 360;
        QColor fill = QColor::fromHsl(hue, 180, 160, 180);
        QColor border = QColor::fromHsl(hue, 200, 100);
        
        QPolygonF poly;
        for (const auto& p : b.points) {
            poly << QPointF(p.x * scale, -p.y * scale + offsetY);
        }
        painter.setPen(QPen(border, 1));
        painter.setBrush(QBrush(fill));
        painter.drawPolygon(poly);
        cnt++;
    }
    
    for (const auto& p : s->paths) {
        if (p.points.size() < 2) continue;
        int hue = (p.layer * 137) % 360;
        QPainterPath pp;
        for (int i = 0; i < p.points.size(); i++) {
            double x = p.points[i].x * scale;
            double y = -p.points[i].y * scale + offsetY;
            if (i == 0) pp.moveTo(x, y);
            else pp.lineTo(x, y);
        }
        painter.setPen(QPen(QColor::fromHsl(hue, 200, 100), qMax(1.0, (double)(p.width)*scale)));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(pp);
        cnt++;
    }
    qDebug() << "渲染对象:" << cnt;
}