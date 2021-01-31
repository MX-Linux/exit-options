#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Pop-up with exit options for MX Fluxbox"));
    parser.addOption({{"v", "vertical"}, "Display buttons in a vertical window"});
    parser.addOption({{"h", "horizontal"}, "Display buttons in a horizontal window"});
    parser.process(a);

    MainWindow w(parser);
    w.show();

    QTimer::singleShot(5000, &a, &QApplication::quit);
    return a.exec();
}
