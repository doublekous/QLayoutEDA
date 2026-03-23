/**
 * @file main.cpp
 * @brief QLayout EDA 主程序入口
 */

#include <QApplication>
#include <QCommandLineParser>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QLayout EDA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QLayoutEDA");
    
    // 解析命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("QLayout EDA - GDS Viewer");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "GDS file to open");
    parser.process(app);
    
    // 获取位置参数（文件路径）
    QStringList args = parser.positionalArguments();
    
    QLayoutEDA::MainWindow mainWindow;
    mainWindow.show();
    
    // 如果指定了文件，打开它
    if (!args.isEmpty()) {
        QString filePath = args.first();
        mainWindow.onImportGDS(filePath);
    }
    
    return app.exec();
}