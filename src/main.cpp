#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("UsbBuilder");
    QApplication::setOrganizationName("UsbBuilder");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
