#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

#include "mainwindow.h"
#include <chrono>

using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("MX-Linux");

    QTranslator qtTran;
    if (qtTran.load("qt_" + QLocale().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(QApplication::applicationName() + "_" + QLocale().name(),
                     "/usr/share/" + QApplication::applicationName() + "/locale"))
        QApplication::installTranslator(&appTran);

    QCommandLineParser parser;
    parser.setApplicationDescription("Pop-up with exit options for MX Fluxbox");
    parser.addOption({{"v", "vertical"}, "Display buttons in a vertical window"});
    parser.addOption({{"h", "horizontal"}, "Display buttons in a horizontal window"});
    parser.addOption({{"t", "timeout"}, "Timeout duration in seconds", "seconds", "5"});
    parser.addOption({"help", "use -v, --vertical to display buttons vertically\nuse "
                              "-h, --horizontal to display buttons horizontally."});
    parser.process(a);
    if (parser.isSet("help")) {
        qDebug() << "Pop-up with exit options for MX Fluxbox";
        qDebug() << "Usage: exit-options [options]\n";
        qDebug() << "Options:";
        qDebug() << "  -h, --horizontal\t Option to display buttons horizontally";
        qDebug() << "  -v, --vertical\t Option to display buttons vertically";
        qDebug() << "  -t, --timeout <sec>\t Timeout duration in seconds\n";
        qDebug() << "The display orientation option used will be remembered and used the next time you start the app";
        qDebug() << "Alternativelly, set the option 'layout=horizontal' or 'layout=vertical' in "
                    "~/.config/MX-Linux/exit-options.conf";
        exit(EXIT_SUCCESS);
    }

    MainWindow w(parser);
    w.show();

    std::chrono::seconds timeout(parser.value("timeout").toUInt());

    QTimer::singleShot(timeout, &a, &QApplication::quit);
    return a.exec();
}
