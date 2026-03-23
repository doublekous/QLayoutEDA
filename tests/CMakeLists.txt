cmake_minimum_required(VERSION 3.16)

# 查找 Qt Test 模块
find_package(Qt5 REQUIRED COMPONENTS Test Core)

# 通用包含目录
set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/graphic
    ${CMAKE_SOURCE_DIR}/src/parser
    ${CMAKE_SOURCE_DIR}/src/ui
)

# ============================================================
# Cell 列表工具
# ============================================================
add_executable(list_cells
    data/list_cells.cpp
)
target_link_libraries(list_cells Qt5::Core)

# ============================================================
# 核心模块测试
# ============================================================
add_executable(test_core
    TestTypes.cpp
)
target_include_directories(test_core PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_core 
    core_module 
    Qt5::Test
)
add_test(NAME test_core COMMAND test_core)

# ============================================================
# 图形模块测试
# ============================================================
add_executable(test_coordinate_system
    TestCoordinateSystem.cpp
)
target_include_directories(test_coordinate_system PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_coordinate_system 
    graphic_module 
    Qt5::Test
)
add_test(NAME test_coordinate_system COMMAND test_coordinate_system)

add_executable(test_spatial_index
    TestSpatialIndex.cpp
)
target_include_directories(test_spatial_index PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_spatial_index 
    graphic_module 
    Qt5::Test
)
add_test(NAME test_spatial_index COMMAND test_spatial_index)

# ============================================================
# 解析器模块测试
# ============================================================
add_executable(test_gds_parser
    TestGDSParser.cpp
)
target_include_directories(test_gds_parser PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_gds_parser 
    parser_module 
    Qt5::Test
)
add_test(NAME test_gds_parser COMMAND test_gds_parser)

# ============================================================
# 集成测试
# ============================================================
add_executable(test_integration
    TestIntegration.cpp
)
target_include_directories(test_integration PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_integration 
    parser_module 
    graphic_module 
    Qt5::Test
)
add_test(NAME test_integration COMMAND test_integration)

# ============================================================
# GDS 解析器测试
# ============================================================
add_executable(test_simple
    test_simple.cpp
)
target_include_directories(test_simple PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_simple 
    parser_module 
    Qt5::Core
)

add_executable(test_full
    test_full.cpp
)
target_include_directories(test_full PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_full 
    parser_module 
    Qt5::Core
)

add_executable(test_large
    test_large.cpp
)
target_include_directories(test_large PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_large 
    parser_module 
    Qt5::Core
)

add_executable(test_refs
    test_refs.cpp
)
target_include_directories(test_refs PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_refs 
    parser_module 
    Qt5::Core
)

add_executable(diagnose_large
    diagnose_large.cpp
)
target_include_directories(diagnose_large PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(diagnose_large 
    Qt5::Core
)

add_executable(diagnose_records
    diagnose_records.cpp
)
target_include_directories(diagnose_records PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(diagnose_records 
    Qt5::Core
)

# 流式解析器测试
add_executable(test_stream_parser
    test_stream_parser.cpp
)
target_include_directories(test_stream_parser PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_stream_parser 
    parser_module 
    Qt5::Core
)

# 扫描调试
add_executable(test_scan_debug
    test_scan_debug.cpp
)
target_include_directories(test_scan_debug PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_scan_debug 
    Qt5::Core
)

# 层级分析
add_executable(analyze_hierarchy
    analyze_hierarchy.cpp
)
target_include_directories(analyze_hierarchy PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(analyze_hierarchy 
    parser_module 
    Qt5::Core
)

# Cell 层级分析测试
add_executable(test_cell_hierarchy
    test_cell_hierarchy.cpp
)
target_include_directories(test_cell_hierarchy PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_cell_hierarchy 
    parser_module
    Qt5::Core
    Qt5::Widgets
)

# SREF/AREF 渲染测试
add_executable(test_sref_render
    test_sref_render.cpp
)
target_include_directories(test_sref_render PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_sref_render 
    parser_module
    Qt5::Core
    Qt5::Gui
)

# ============================================================
# 测试目标
# ============================================================
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_core test_coordinate_system test_spatial_index test_gds_parser test_integration
    COMMENT "Running all tests..."
)