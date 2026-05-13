// Starts the Qt application and opens the TaskManager window
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.showMaximized();
    return QCoreApplication::exec();
}



