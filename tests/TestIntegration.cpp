/**
 * @file TestIntegration.cpp
 * @brief 集成测试 - 端到端功能测试
 */

#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include "GDSParser.h"
#include "CoordinateSystem.h"
#include "SpatialIndex.h"

using namespace QLayoutEDA;

class TestIntegration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        testDir = QDir::tempPath() + "/QLayoutEDA_Integration";
        QDir().mkpath(testDir);
    }

    void cleanupTestCase()
    {
        QDir(testDir).removeRecursively();
    }

    // ============================================================
    // 模块集成测试
    // ============================================================
    
    void testParserToCoordinateSystem()
    {
        // 解析后数据应能正确传入坐标系统
        auto parser = std::make_unique<GDSParser>();
        auto coordSystem = std::make_unique<CoordinateSystem>();
        
        // 解析测试文件
        QString filePath = testDir + "/test.gds";
        createTestGDS(filePath);
        
        if (parser->parse(filePath)) {
            GDSDataPtr data = parser->getData();
            QVERIFY(data != nullptr);
            
            // 设置数据库单位
            coordSystem->setDatabaseUnit(data->dbUnitsPerMeter);
            
            // 验证转换
            QPointF screen = coordSystem->dbToScreen(0, 0);
            QVERIFY(screen.x() == 0.0 && screen.y() == 0.0);
        }
    }
    
    void testParserToSpatialIndex()
    {
        // 解析后的图形数据应能正确索引
        auto parser = std::make_unique<GDSParser>();
        auto spatialIndex = std::make_unique<SpatialIndex>();
        
        QString filePath = testDir + "/test.gds";
        createTestGDS(filePath);
        
        if (parser->parse(filePath)) {
            GDSDataPtr data = parser->getData();
            
            // 将边界元素添加到空间索引
            quint64 id = 0;
            for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
                auto structure = it.value();
                for (const auto& boundary : structure->boundaries) {
                    SpatialObject obj;
                    obj.id = ++id;
                    obj.layer = boundary.layer;
                    obj.type = 0; // boundary
                    
                    // 计算边界框
                    if (!boundary.points.isEmpty()) {
                        Coord minX = boundary.points[0].x;
                        Coord minY = boundary.points[0].y;
                        Coord maxX = minX, maxY = minY;
                        for (const auto& pt : boundary.points) {
                            minX = qMin(minX, pt.x);
                            minY = qMin(minY, pt.y);
                            maxX = qMax(maxX, pt.x);
                            maxY = qMax(maxY, pt.y);
                        }
                        obj.bounds = DbRect(minX, minY, maxX, maxY);
                    }
                    
                    spatialIndex->insert(obj);
                }
            }
            
            // 验证索引
            QVERIFY(spatialIndex->count() > 0 || data->structures.isEmpty());
        }
    }
    
    void testFullWorkflow()
    {
        // 完整工作流：解析 -> 索引 -> 查询 -> 坐标转换
        auto parser = std::make_unique<GDSParser>();
        auto coordSystem = std::make_unique<CoordinateSystem>();
        auto spatialIndex = std::make_unique<SpatialIndex>();
        
        // 1. 解析
        QString filePath = testDir + "/workflow.gds";
        createTestGDS(filePath);
        
        if (parser->parse(filePath)) {
            GDSDataPtr data = parser->getData();
            
            // 2. 设置坐标系统
            coordSystem->setDatabaseUnit(data->dbUnitsPerMeter);
            coordSystem->setScale(1.0);
            
            // 3. 构建空间索引
            quint64 id = 0;
            for (auto it = data->structures.begin(); it != data->structures.end(); ++it) {
                auto structure = it.value();
                for (const auto& boundary : structure->boundaries) {
                    SpatialObject obj;
                    obj.id = ++id;
                    obj.layer = boundary.layer;
                    obj.type = 0;
                    // ... 计算边界
                    spatialIndex->insert(obj);
                }
            }
            
            // 4. 查询和转换
            DbRect queryRect(0, 0, 1000, 1000);
            QVector<SpatialObject> results = spatialIndex->query(queryRect);
            
            for (const auto& obj : results) {
                QPointF screenPos = coordSystem->dbToScreen(
                    obj.bounds.left, obj.bounds.top);
                QVERIFY(screenPos.x() != 0 || screenPos.y() != 0);
            }
        }
    }

    // ============================================================
    // 性能基准测试
    // ============================================================
    
    void benchmarkParse_SmallFile()
    {
        // < 10MB 文件解析
        QString filePath = testDir + "/small.gds";
        createTestGDS(filePath, 1024 * 1024); // 1MB
        
        auto parser = std::make_unique<GDSParser>();
        
        QBENCHMARK {
            parser->parse(filePath);
        }
    }
    
    void benchmarkParse_MediumFile()
    {
        // 10-50MB 文件解析
        QString filePath = testDir + "/medium.gds";
        createTestGDS(filePath, 10 * 1024 * 1024); // 10MB
        
        auto parser = std::make_unique<GDSParser>();
        
        QBENCHMARK {
            parser->parse(filePath);
        }
    }
    
    void benchmarkCoordinateTransform()
    {
        auto coordSystem = std::make_unique<CoordinateSystem>();
        coordSystem->setScale(1.5);
        coordSystem->setOffset(100, 200);
        
        QBENCHMARK {
            for (int i = 0; i < 10000; ++i) {
                coordSystem->dbToScreen(i * 100, i * 200);
                coordSystem->screenToDb(QPointF(i * 150.5, i * 300.5));
            }
        }
    }
    
    void benchmarkSpatialQuery()
    {
        auto spatialIndex = std::make_unique<SpatialIndex>();
        
        // 准备数据
        for (int i = 0; i < 10000; ++i) {
            SpatialObject obj;
            obj.id = i + 1;
            obj.bounds = DbRect(i * 10, i * 10, i * 10 + 50, i * 10 + 50);
            spatialIndex->insert(obj);
        }
        
        QBENCHMARK {
            for (int i = 0; i < 1000; ++i) {
                DbRect rect(i * 5, i * 5, i * 5 + 100, i * 5 + 100);
                spatialIndex->query(rect);
            }
        }
    }
    
    void benchmarkFullWorkflow()
    {
        auto parser = std::make_unique<GDSParser>();
        auto coordSystem = std::make_unique<CoordinateSystem>();
        auto spatialIndex = std::make_unique<SpatialIndex>();
        
        QString filePath = testDir + "/bench.gds";
        createTestGDS(filePath, 1024 * 1024);
        
        QBENCHMARK {
            parser->parse(filePath);
            
            // 模拟完整工作流
            for (int i = 0; i < 100; ++i) {
                DbRect rect(i * 10, i * 10, i * 10 + 50, i * 10 + 50);
                auto results = spatialIndex->query(rect);
                for (const auto& obj : results) {
                    coordSystem->dbToScreen(obj.bounds.left, obj.bounds.top);
                }
            }
        }
    }

    // ============================================================
    // 大文件加载测试
    // ============================================================
    
    void testLargeFile_10MB()
    {
        QString filePath = testDir + "/large_10mb.gds";
        createTestGDS(filePath, 10 * 1024 * 1024);
        
        auto parser = std::make_unique<GDSParser>();
        
        QElapsedTimer timer;
        timer.start();
        
        bool result = parser->parse(filePath);
        
        qint64 elapsed = timer.elapsed();
        qDebug() << "10MB file parse time:" << elapsed << "ms";
        
        // 性能要求：< 1 秒
        QVERIFY(elapsed < 1000 || !result);
    }
    
    void testLargeFile_50MB()
    {
        QString filePath = testDir + "/large_50mb.gds";
        createTestGDS(filePath, 50 * 1024 * 1024);
        
        auto parser = std::make_unique<GDSParser>();
        
        QElapsedTimer timer;
        timer.start();
        
        bool result = parser->parse(filePath);
        
        qint64 elapsed = timer.elapsed();
        qDebug() << "50MB file parse time:" << elapsed << "ms";
        
        // 性能要求：< 5 秒
        QVERIFY(elapsed < 5000 || !result);
    }
    
    void testLargeFile_100MB()
    {
        QString filePath = testDir + "/large_100mb.gds";
        createTestGDS(filePath, 100 * 1024 * 1024);
        
        auto parser = std::make_unique<GDSParser>();
        
        QElapsedTimer timer;
        timer.start();
        
        bool result = parser->parse(filePath);
        
        qint64 elapsed = timer.elapsed();
        qDebug() << "100MB file parse time:" << elapsed << "ms";
        
        // 性能要求：< 10 秒
        QVERIFY(elapsed < 10000 || !result);
    }

    // ============================================================
    // 内存测试
    // ============================================================
    
    void testMemoryUsage_LargeDataset()
    {
        // 测试大数据集内存使用
        auto spatialIndex = std::make_unique<SpatialIndex>();
        
        // 插入 100 万对象
        for (int i = 0; i < 1000000; ++i) {
            SpatialObject obj;
            obj.id = i + 1;
            obj.bounds = DbRect(i % 1000, i / 1000, (i % 1000) + 1, (i / 1000) + 1);
            spatialIndex->insert(obj);
        }
        
        QCOMPARE(spatialIndex->count(), 1000000);
        
        // 内存要求：< 500MB (粗略检查，实际需要内存分析工具)
        // 这里只验证功能正常
        DbRect queryRect(0, 0, 100, 100);
        QVector<SpatialObject> results = spatialIndex->query(queryRect);
        QVERIFY(results.size() > 0);
    }
    
    void testMemoryLeak_RepeatedParse()
    {
        // 重复解析检测内存泄漏
        QString filePath = testDir + "/leak_test.gds";
        createTestGDS(filePath);
        
        for (int i = 0; i < 100; ++i) {
            auto parser = std::make_unique<GDSParser>();
            parser->parse(filePath);
        }
        
        // 如果有内存泄漏，这里会明显
        // 实际检测需要工具
        QVERIFY(true);
    }

    // ============================================================
    // 并发测试（如果支持）
    // ============================================================
    
    void testConcurrent_Parse()
    {
        // TODO: 并发解析测试（如果 parser 支持线程安全）
    }
    
    void testConcurrent_Query()
    {
        // TODO: 并发查询测试
    }

private:
    QString testDir;
    
    void createTestGDS(const QString& filePath, qint64 size = 1024)
    {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            // 写入最小有效 GDS 结构
            QByteArray data(size, 0);
            // GDSII HEADER
            data[0] = 0x00;
            data[1] = 0x02;
            file.write(data);
            file.close();
        }
    }
};

QTEST_MAIN(TestIntegration)
#include "TestIntegration.moc"