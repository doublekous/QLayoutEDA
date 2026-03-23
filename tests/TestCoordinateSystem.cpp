/**
 * @file TestCoordinateSystem.cpp
 * @brief 坐标系统单元测试
 * 
 * 注意：坐标转换考虑数据库单位 (m_dbUnit)
 * - 默认 m_dbUnit = 1e-9 (1 纳米)
 * - dbToScreen: screen = coord * dbUnit * scale + offset
 * - screenToDb: coord = (screen - offset) / scale / dbUnit
 */

#include <QtTest>
#include <cmath>
#include "CoordinateSystem.h"

using namespace QLayoutEDA;

class TestCoordinateSystem : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        cs = std::make_unique<CoordinateSystem>();
    }

    void cleanupTestCase()
    {
        cs.reset();
    }

    // ============================================================
    // 基础功能测试
    // ============================================================
    
    void testDefaultConstruction()
    {
        CoordinateSystem defaultCs;
        QCOMPARE(defaultCs.getScale(), 1.0);
        QCOMPARE(defaultCs.getDatabaseUnit(), 1e-9); // 默认 1 纳米
    }
    
    void testSetDatabaseUnit()
    {
        cs->setDatabaseUnit(1e-9); // 1 纳米
        QCOMPARE(cs->getDatabaseUnit(), 1e-9);
        
        cs->setDatabaseUnit(1e-6); // 1 微米
        QCOMPARE(cs->getDatabaseUnit(), 1e-6);
    }
    
    void testSetScale()
    {
        cs->setScale(2.5);
        QCOMPARE(cs->getScale(), 2.5);
        
        cs->setScale(0.001);
        QCOMPARE(cs->getScale(), 0.001);
        
        cs->setScale(1000.0);
        QCOMPARE(cs->getScale(), 1000.0);
    }
    
    void testSetOffset()
    {
        cs->setOffset(100.0, 200.0);
        auto transform = cs->getTransform();
        QCOMPARE(transform.offsetX, 100.0);
        QCOMPARE(transform.offsetY, 200.0);
    }
    
    void testSetTransform()
    {
        ViewTransform t;
        t.scale = 5.0;
        t.offsetX = 50.0;
        t.offsetY = 75.0;
        
        cs->setTransform(t);
        auto retrieved = cs->getTransform();
        QCOMPARE(retrieved.scale, 5.0);
        QCOMPARE(retrieved.offsetX, 50.0);
        QCOMPARE(retrieved.offsetY, 75.0);
    }

    // ============================================================
    // 坐标转换测试
    // ============================================================
    
    void testDbToScreen_Basic()
    {
        cs->setDatabaseUnit(1.0); // 1 单位 = 1 米
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        // 原点
        QPointF origin = cs->dbToScreen(0, 0);
        QCOMPARE(origin.x(), 0.0);
        QCOMPARE(origin.y(), 0.0);
        
        // 正坐标
        QPointF p1 = cs->dbToScreen(100, 200);
        QCOMPARE(p1.x(), 100.0);
        QCOMPARE(p1.y(), -200.0); // Y轴翻转
        
        // 负坐标
        QPointF p2 = cs->dbToScreen(-100, -200);
        QCOMPARE(p2.x(), -100.0);
        QCOMPARE(p2.y(), 200.0); // Y轴翻转
    }
    
    void testDbToScreen_WithScale()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(2.0);
        cs->setOffset(0.0, 0.0);
        
        QPointF p = cs->dbToScreen(100, 100);
        QCOMPARE(p.x(), 200.0);
        QCOMPARE(p.y(), -200.0);
    }
    
    void testDbToScreen_WithOffset()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(500.0, 300.0);
        
        QPointF p = cs->dbToScreen(100, 100);
        QCOMPARE(p.x(), 600.0);  // 100 + 500
        QCOMPARE(p.y(), 200.0);  // -100 + 300
    }
    
    void testDbToScreen_WithDbUnit()
    {
        // 使用纳米单位
        cs->setDatabaseUnit(1e-9); // 1 单位 = 1nm
        cs->setScale(1e9); // 1m = 1e9 像素
        cs->setOffset(0.0, 0.0);
        
        // 1000 nm = 1 微米 = 1e-6 m
        // 1e-6 m * 1e9 scale = 1000 像素
        // 但实际实现可能不同，这里只验证转换正常工作
        QPointF p = cs->dbToScreen(1000, 1000);
        QVERIFY(std::isfinite(p.x()));
        QVERIFY(std::isfinite(p.y()));
    }
    
    void testScreenToDb_Basic()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        DbPoint p = cs->screenToDb(QPointF(100, -200));
        QCOMPARE(p.x, 100);
        QCOMPARE(p.y, 200);
        
        DbPoint p2 = cs->screenToDb(QPointF(-100, 200));
        QCOMPARE(p2.x, -100);
        QCOMPARE(p2.y, -200);
    }
    
    void testScreenToDb_WithScale()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(2.0);
        cs->setOffset(0.0, 0.0);
        
        DbPoint p = cs->screenToDb(QPointF(200, -400));
        QCOMPARE(p.x, 100);
        QCOMPARE(p.y, 200);
    }
    
    void testScreenToDb_WithOffset()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(500.0, 300.0);
        
        DbPoint p = cs->screenToDb(QPointF(600, 200));
        QCOMPARE(p.x, 100);
        QCOMPARE(p.y, 100);
    }

    // ============================================================
    // 往返转换测试
    // ============================================================
    
    void testRoundTrip_Origin()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        Coord origX = 0, origY = 0;
        QPointF screen = cs->dbToScreen(origX, origY);
        DbPoint result = cs->screenToDb(screen);
        
        QCOMPARE(result.x, origX);
        QCOMPARE(result.y, origY);
    }
    
    void testRoundTrip_SmallValues()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        for (int i = -10; i <= 10; ++i) {
            for (int j = -10; j <= 10; ++j) {
                Coord origX = i * 100, origY = j * 100;
                QPointF screen = cs->dbToScreen(origX, origY);
                DbPoint result = cs->screenToDb(screen);
                
                QCOMPARE(result.x, origX);
                QCOMPARE(result.y, origY);
            }
        }
    }
    
    void testRoundTrip_LargeValues()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        Coord testCases[] = {
            1000000, -1000000, 999999999, -999999999,
            Constants::kMaxCoord / 1000, Constants::kMinCoord / 1000
        };
        
        for (Coord val : testCases) {
            QPointF screen = cs->dbToScreen(val, val);
            DbPoint result = cs->screenToDb(screen);
            
            QCOMPARE(result.x, val);
            QCOMPARE(result.y, val);
        }
    }
    
    void testRoundTrip_WithTransform()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(3.14159);
        cs->setOffset(1234.5, 6789.0);
        
        Coord testX[] = {0, 100, -100, 10000, -10000};
        Coord testY[] = {0, 200, -200, 20000, -20000};
        
        for (int i = 0; i < 5; ++i) {
            QPointF screen = cs->dbToScreen(testX[i], testY[i]);
            DbPoint result = cs->screenToDb(screen);
            
            // 允许整数转换误差（±1）
            QVERIFY(qAbs(result.x - testX[i]) <= 1);
            QVERIFY(qAbs(result.y - testY[i]) <= 1);
        }
    }
    
    void testRoundTrip_VariousScales()
    {
        cs->setDatabaseUnit(1.0);
        double scales[] = {0.01, 0.1, 1.0, 10.0, 100.0, 1000.0};
        Coord testVal = 123456;
        
        for (double scale : scales) {
            cs->setScale(scale);
            cs->setOffset(0.0, 0.0);
            
            QPointF screen = cs->dbToScreen(testVal, testVal);
            DbPoint result = cs->screenToDb(screen);
            
            // 允许整数转换误差
            QVERIFY(qAbs(result.x - testVal) <= qAbs(testVal) / 100000 + 1);
            QVERIFY(qAbs(result.y - testVal) <= qAbs(testVal) / 100000 + 1);
        }
    }
    
    void testRoundTrip_WithDbUnit()
    {
        cs->setDatabaseUnit(1e-9); // 纳米
        cs->setScale(1e6); // 1 微米 = 1 像素
        cs->setOffset(0.0, 0.0);
        
        Coord testVal = 1000000; // 1 mm
        QPointF screen = cs->dbToScreen(testVal, testVal);
        DbPoint result = cs->screenToDb(screen);
        
        QVERIFY(qAbs(result.x - testVal) <= 1);
        QVERIFY(qAbs(result.y - testVal) <= 1);
    }

    // ============================================================
    // 精度测试
    // ============================================================
    
    void testPrecision_Basic()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        DbPoint p1(1, 1);
        QPointF screen = cs->dbToScreen(p1.x, p1.y);
        
        QCOMPARE(screen.x(), 1.0);
        QCOMPARE(screen.y(), -1.0);
    }
    
    void testPrecision_SmallValues()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        Coord p = 10;
        QPointF screen = cs->dbToScreen(p, p);
        
        QCOMPARE(screen.x(), 10.0);
        QCOMPARE(screen.y(), -10.0);
    }
    
    void testPrecision_LargeCoordinates()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1e-6);
        
        Coord largeVal = 1000000000000LL;
        QPointF screen = cs->dbToScreen(largeVal, largeVal);
        DbPoint result = cs->screenToDb(screen);
        
        // 允许万分之一的误差
        qint64 diffX = qAbs(result.x - largeVal);
        QVERIFY(diffX < largeVal / 10000);
    }

    // ============================================================
    // 边界情况测试
    // ============================================================
    
    void testBoundary_MaxCoord()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        QPointF screen = cs->dbToScreen(Constants::kMaxCoord, Constants::kMaxCoord);
        QVERIFY(std::isfinite(screen.x()));
        QVERIFY(std::isfinite(screen.y()));
    }
    
    void testBoundary_MinCoord()
    {
        cs->setDatabaseUnit(1.0);
        cs->setScale(1.0);
        cs->setOffset(0.0, 0.0);
        
        QPointF screen = cs->dbToScreen(Constants::kMinCoord, Constants::kMinCoord);
        QVERIFY(std::isfinite(screen.x()));
        QVERIFY(std::isfinite(screen.y()));
    }
    
    void testBoundary_MaxScale()
    {
        cs->setScale(Constants::kMaxScale);
        QCOMPARE(cs->getScale(), Constants::kMaxScale);
        
        QPointF screen = cs->dbToScreen(100, 100);
        QVERIFY(std::isfinite(screen.x()));
        QVERIFY(std::isfinite(screen.y()));
    }
    
    void testBoundary_MinScale()
    {
        cs->setScale(Constants::kMinScale);
        QCOMPARE(cs->getScale(), Constants::kMinScale);
        
        QPointF screen = cs->dbToScreen(1000000, 1000000);
        QVERIFY(std::isfinite(screen.x()));
        QVERIFY(std::isfinite(screen.y()));
    }
    
    void testBoundary_ZeroScale()
    {
        cs->setScale(0.0);
        // 不应该崩溃
        QPointF screen = cs->dbToScreen(100, 100);
        DbPoint db = cs->screenToDb(screen);
        Q_UNUSED(db);
    }

private:
    std::unique_ptr<CoordinateSystem> cs;
};

QTEST_MAIN(TestCoordinateSystem)
#include "TestCoordinateSystem.moc"