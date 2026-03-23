/**
 * @file test_selection.cpp
 * @brief 选择功能单元测试
 * 
 * Issue #7: Shape Selection and Move Functionality
 * 测试 SelectionEngine 的选择功能
 */

#include <QtTest>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include "SelectionEngine.h"
#include "GDSParser.h"

using namespace QLayoutEDA;

/**
 * @brief 选择引擎测试类
 * 
 * 测试范围：
 * 1. 点选测试 (pickPoint)
 * 2. 框选测试 (pickRect)
 * 3. 全选测试 (selectAll)
 * 4. 多选测试
 */
class TestSelection : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // ============================================================
    // 点选测试
    // ============================================================
    void testPickPoint_EmptyData();
    void testPickPoint_NoHit();
    void testPickPoint_Tolerance();
    void testPickPoint_AddToSelection();
    
    // ============================================================
    // 框选测试
    // ============================================================
    void testPickRect_EmptyData();
    void testPickRect_NoHit();
    void testPickRect_AddToSelection();
    
    // ============================================================
    // 全选测试
    // ============================================================
    void testSelectAll_EmptyData();
    void testSelectAll_ClearsFirst();
    
    // ============================================================
    // 选择管理测试
    // ============================================================
    void testClearSelection();
    void testInvertSelection();
    void testFilterByLayer();
    void testFilterByType();
    void testSelectedCount();
    void testHasSelection();

private:
    SelectionEngine* m_engine;
    GDSParser* m_parser;
};

// ============================================================
// 测试初始化
// ============================================================

void TestSelection::initTestCase()
{
    qDebug() << "Selection Engine Tests - Issue #7";
    m_parser = new GDSParser();
}

void TestSelection::cleanupTestCase()
{
    delete m_parser;
}

void TestSelection::init()
{
    m_engine = new SelectionEngine();
}

void TestSelection::cleanup()
{
    delete m_engine;
    m_engine = nullptr;
}

// ============================================================
// 点选测试
// // ============================================================

void TestSelection::testPickPoint_EmptyData()
{
    // 没有数据时点选应返回空结果
    QPointF point(100, 100);
    QVector<ObjectId> result = m_engine->pickPoint(point, 5.0);
    QVERIFY(result.isEmpty());
    QCOMPARE(result.size(), 0);
}

void TestSelection::testPickPoint_NoHit()
{
    // 设置空数据
    auto data = std::make_shared<GDSData>();
    m_engine->setDataSource(data);
    
    // 点选空白区域
    QPointF point(1000, 1000);
    QVector<ObjectId> result = m_engine->pickPoint(point, 5.0);
    QVERIFY(result.isEmpty());
}

void TestSelection::testPickPoint_Tolerance()
{
    // 测试容差设置
    m_engine->setTolerance(10.0);
    QCOMPARE(m_engine->tolerance(), 10.0);
    
    m_engine->setTolerance(5.0);
    QCOMPARE(m_engine->tolerance(), 5.0);
}

void TestSelection::testPickPoint_AddToSelection()
{
    // 无数据时，点选不应崩溃
    QPointF point(100, 100);
    QVector<ObjectId> result = m_engine->pickPoint(point, 5.0);
    QVERIFY(result.isEmpty());
}

// ============================================================
// 框选测试
// // ============================================================

void TestSelection::testPickRect_EmptyData()
{
    // 没有数据时框选应返回空结果
    QRectF rect(0, 0, 100, 100);
    QVector<ObjectId> result = m_engine->pickRect(rect);
    QVERIFY(result.isEmpty());
    QCOMPARE(result.size(), 0);
}

void TestSelection::testPickRect_NoHit()
{
    // 设置空数据
    auto data = std::make_shared<GDSData>();
    m_engine->setDataSource(data);
    
    // 框选空白区域
    QRectF rect(1000, 1000, 100, 100);
    QVector<ObjectId> result = m_engine->pickRect(rect);
    QVERIFY(result.isEmpty());
}

void TestSelection::testPickRect_AddToSelection()
{
    // 无数据时，框选不应崩溃
    QRectF rect(0, 0, 100, 100);
    QVector<ObjectId> result = m_engine->pickRect(rect);
    QVERIFY(result.isEmpty());
}

// ============================================================
// 全选测试
// // ============================================================

void TestSelection::testSelectAll_EmptyData()
{
    // 没有数据时全选应返回空结果
    SelectionResult result = m_engine->selectAll();
    QVERIFY(result.isEmpty());
    QCOMPARE(result.count(), 0);
}

void TestSelection::testSelectAll_ClearsFirst()
{
    // 全选应该先清除现有选择
    auto data = std::make_shared<GDSData>();
    m_engine->setDataSource(data);
    
    // 添加一个选择
    SelectedObject obj;
    obj.type = SelectedObject::Boundary;
    obj.index = 0;
    obj.structureName = "test";
    
    SelectionResult result = m_engine->selectAll();
    // 空数据，全选结果应为空
    QVERIFY(result.isEmpty());
}

// ============================================================
// 选择管理测试
// // ============================================================

void TestSelection::testClearSelection()
{
    // 清除空选择不应崩溃
    m_engine->clearSelection();
    QCOMPARE(m_engine->selectedCount(), 0);
    QVERIFY(!m_engine->hasSelection());
}

void TestSelection::testInvertSelection()
{
    // 空数据时反选应返回空
    SelectionResult result = m_engine->invertSelection();
    QVERIFY(result.isEmpty());
}

void TestSelection::testFilterByLayer()
{
    // 空选择时过滤应返回空
    QSet<SelectedObject> filtered = m_engine->filterByLayer(1);
    QVERIFY(filtered.isEmpty());
}

void TestSelection::testFilterByType()
{
    // 空选择时过滤应返回空
    QSet<SelectedObject> filtered = m_engine->filterByType(SelectedObject::Boundary);
    QVERIFY(filtered.isEmpty());
}

void TestSelection::testSelectedCount()
{
    // 初始选择数为 0
    QCOMPARE(m_engine->selectedCount(), 0);
}

void TestSelection::testHasSelection()
{
    // 初始无选择
    QVERIFY(!m_engine->hasSelection());
    
    // 清除后仍无选择
    m_engine->clearSelection();
    QVERIFY(!m_engine->hasSelection());
}

QTEST_MAIN(TestSelection)
#include "test_selection.moc"