#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("MX-Linux");

    QCommandLineParser parser;
    parser.setApplicationDescription("Pop-up with exit options for MX Fluxbox");
    parser.addOption({{"v", "vertical"}, "Display buttons in a vertical window"});
    parser.addOption({{"h", "horizontal"}, "Display buttons in a horizontal window"});
    parser.addOption({"help", "use -v, --vertical to display buttons vertically\nuse -h, --horizontal to display buttons horizontally."});
    parser.process(a);
    if (parser.isSet("help")) {
        qDebug("Pop-up with exit options for MX Fluxbox");
        qDebug("Usage: exit-options [options]\n");
        qDebug("Options:");
        qDebug("  -v, --vertical\t Option to display buttons vertically");
        qDebug("  -h, --horizontal\t Option to display buttons horizontally\n");
        qDebug("The last option used will be remembered and used the next time you start the app");
        qDebug("Alternativelly, set the option 'layout=horizontal' or 'layout=vertical' in ~/.config/MX-Linux/exit-options.conf");
        exit(EXIT_SUCCESS);
    }

    MainWindow w(parser);
    w.show();

    QTimer::singleShot(5000, &a, &QApplication::quit);
    return a.exec();
}
