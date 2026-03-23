/**
 * @file TestTypes.cpp
 * @brief 类型测试
 */

#include <QtTest>
#include "core/Types.h"

class TestTypes : public QObject {
    Q_OBJECT

private slots:
    void testDbPoint()
    {
        QLayoutEDA::DbPoint p1(100, 200);
        QCOMPARE(p1.x, 100);
        QCOMPARE(p1.y, 200);
        
        QLayoutEDA::DbPoint p2(100, 200);
        QVERIFY(p1 == p2);
        
        QLayoutEDA::DbPoint p3(101, 200);
        QVERIFY(p1 != p3);
    }
    
    void testDbRect()
    {
        QLayoutEDA::DbRect r(0, 0, 100, 50);
        QCOMPARE(r.width(), 100);
        QCOMPARE(r.height(), 50);
        QVERIFY(r.isValid());
        QVERIFY(r.contains(QLayoutEDA::DbPoint(50, 25)));
        QVERIFY(!r.contains(QLayoutEDA::DbPoint(150, 25)));
    }
};

QTEST_MAIN(TestTypes)
#include "TestTypes.moc"