#include "MainWindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("BMPViewerQt");
    app.setApplicationDisplayName(QStringLiteral("BMP 图像查看器（C++ / Qt 版）"));

    QFont font(QStringLiteral("Microsoft YaHei"), 10);  // 设置全局字体为微软雅黑，大小为10pt
    app.setFont(font);  // 应用全局字体设置

    MainWindow window;  // 创建主窗口实例
    window.show();  // 显示主窗口
    return app.exec();  // 进入Qt事件循环,让程序持续运行
}
