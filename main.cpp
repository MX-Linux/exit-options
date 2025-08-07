#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

#include "mainwindow.h"
#include "common.h"
#include <chrono>

using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // Set Qt platform to XCB (X11) if not already set and we're in X11 environment
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (!qEnvironmentVariableIsEmpty("DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }

    QApplication app(argc, argv);
    QApplication::setOrganizationName("MX-Linux");

    // Check for existing instances of the application
    QProcess proc;
    proc.start("pgrep", {"--count", "--exact", QApplication::applicationName()});
    proc.waitForFinished();
    if (proc.exitCode() == 0 && proc.readAllStandardOutput().trimmed().toInt() > 1) {
        QProcess::startDetached("pkill", {"--oldest", QApplication::applicationName()});
        return EXIT_SUCCESS;
    }

    QTranslator qtTran;
    if (qtTran.load("qt_" + QLocale().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTran);
    }

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtBaseTran);
    }

    QTranslator appTran;
    if (appTran.load(QApplication::applicationName() + "_" + QLocale().name(),
                     "/usr/share/" + QApplication::applicationName() + "/locale")) {
        QApplication::installTranslator(&appTran);
    }

    // Command line parser setup
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Pop-up with exit options for MX Fluxbox"));
    parser.addOption({{"v", "vertical"}, QObject::tr("Display buttons in a vertical window")});
    parser.addOption({{"h", "horizontal"}, QObject::tr("Display buttons in a horizontal window")});
    parser.addOption({{"t", "timeout"}, QObject::tr("Timeout duration in seconds"), "seconds", "5"});
    parser.addOption({"help", QObject::tr("use -v, --vertical to display buttons vertically\nuse "
                                          "-h, --horizontal to display buttons horizontally.")});
    parser.process(app);
    if (parser.isSet("help")) {
        qDebug().noquote() << QObject::tr("Pop-up with exit options for MX Fluxbox");
        qDebug().noquote() << QObject::tr("Usage: exit-options [options]\n");
        qDebug().noquote() << QObject::tr("Options:");
        qDebug().noquote() << QObject::tr("  -h, --horizontal\t Option to display buttons horizontally");
        qDebug().noquote() << QObject::tr("  -v, --vertical\t Option to display buttons vertically");
        qDebug().noquote() << QObject::tr("  -t, --timeout <sec>\t Timeout duration in seconds\n");
        qDebug().noquote() << QObject::tr(
            "The display orientation option used will be remembered and used the next time you start the app");
        qDebug().noquote() << QObject::tr("Alternativelly, set the option 'layout=horizontal' or 'layout=vertical' in "
                                          "~/.config/MX-Linux/exit-options.conf")
                                  + "\n";
        qDebug().noquote() << QObject::tr(
            "To set the timeout in exit-options.conf use 'timeout=X' where X is the timeout in seconds.");
        qDebug().noquote() << QObject::tr("You can also use 'timeout=off' to turn the timeout off.") + "\n";
        qDebug().noquote()
            << QObject::tr("You can define custom icons by adding IconName=/path/iconame.ext in the exit-options.conf "
                           "file. The names of the icons that you remap: %1")
                       .arg("LockIcon, LogoutIcon, SuspendIcon, RebootIcon, ShutdownIcon.")
                   + "\n";
        qDebug().noquote() << QObject::tr("Other options that can be set in the exit-options.conf file: %1")
                                  .arg("IconSize=, Margin=, Spacing=");
        exit(EXIT_SUCCESS);
    }

    MainWindow mainWindow(parser);
    mainWindow.show();

    // Load timeout settings
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    QSettings systemSettings(SYSTEM_CONFIG_PATH, QSettings::IniFormat);

    QString timeout;
    if (parser.isSet("timeout")) {
        timeout = parser.value("timeout");
    } else if (userSettings.contains("timeout")) {
        timeout = userSettings.value("timeout").toString();
    } else if (systemSettings.contains("timeout")) {
        timeout = systemSettings.value("timeout").toString();
    } else {
        timeout = "off";
    }

    // Set up timeout for quitting the application
    if (timeout != "off") {
        bool ok {false};
        std::chrono::seconds timeoutDuration(timeout.toUInt(&ok));
        if (ok) {
            QTimer::singleShot(timeoutDuration, &app, &QApplication::quit);
        }
    }
    return app.exec();
}
