/**
 * @file main.cpp
 * @brief QLayout EDA 主程序入口
 */

#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QLayout EDA");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QLayoutEDA");
    
    QLayoutEDA::MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}