cmake_minimum_required(VERSION 3.16)
project(QLayoutEDA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 路径
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/msvc2019_64")

# 查找 Qt
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Concurrent)

# 编译选项
if(MSVC)
    add_compile_options(/utf-8 /W4)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/interfaces
    ${CMAKE_SOURCE_DIR}/include/core
)

# 核心模块
add_library(core_module STATIC
    src/core/Types.cpp
)
target_link_libraries(core_module Qt5::Core Qt5::Gui)

# 解析器模块
add_library(parser_module STATIC
    src/parser/GDSParser.cpp
    src/parser/GDSFormat.cpp
)
target_link_libraries(parser_module core_module Qt5::Core)

# 图形模块
add_library(graphic_module STATIC
    src/graphic/CoordinateSystem.cpp
    src/graphic/SpatialIndex.cpp
    src/graphic/RenderEngine.cpp
    src/graphic/LayerManager.cpp
    src/graphic/GeometryAlgorithms.cpp
    src/graphic/QuadTree.cpp
    src/graphic/RenderCoordinator.cpp
    src/graphic/CellRenderCache.cpp
    src/graphic/SelectionEngine.cpp
)
target_link_libraries(graphic_module parser_module Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent)

# UI 模块
add_library(ui_module STATIC
    src/ui/MainWindow.cpp
    src/ui/CanvasWidget.cpp
    src/ui/LayerPanel.cpp
    src/ui/PropertyPanel.cpp
    src/ui/CellListPanel.cpp
)
target_link_libraries(ui_module graphic_module Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL)

# 主程序
add_executable(QLayoutEDA
    src/main.cpp
)
target_link_libraries(QLayoutEDA ui_module)

# 诊断工具
add_executable(diagnose_gds src/test/test_diagnose.cpp)
target_link_libraries(diagnose_gds parser_module Qt5::Core)

# 深度诊断工具
add_executable(deep_diagnose_gds src/test/test_deep_diagnose.cpp)
target_link_libraries(deep_diagnose_gds parser_module Qt5::Core)

# 四叉树测试工具
add_executable(test_quadtree src/test/test_quadtree.cpp)
target_link_libraries(test_quadtree graphic_module Qt5::Core)

# 快速解析测试
add_executable(test_fast_parse src/test/test_fast_parse.cpp)
target_link_libraries(test_fast_parse Qt5::Core)

# 显示结构信息
add_executable(test_show_structs src/test/test_show_structs.cpp)
target_link_libraries(test_show_structs parser_module Qt5::Core)

# 创建多Cell测试文件
add_executable(create_multi_cell_gds src/test/create_multi_cell_gds.cpp)
target_link_libraries(create_multi_cell_gds Qt5::Core)

# Cell引用分析
add_executable(test_cell_refs src/test/test_cell_refs.cpp)
target_link_libraries(test_cell_refs Qt5::Core)

# 测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()