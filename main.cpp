#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>

#include <memory>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>

#include "mainwindow.h"
#include "common.h"
#include <chrono>

using namespace std::chrono_literals;

namespace
{
volatile std::sig_atomic_t quitWriteFd = -1;

void handleSigterm(int)
{
    const int savedErrno = errno;
    const char byte = 1;
    [[maybe_unused]] const auto written = write(quitWriteFd, &byte, sizeof(byte));
    errno = savedErrno;
}

// Keep the pipe available to the signal handler until the process exits.
int installQuitHandler()
{
    int fds[2];
    if (pipe2(fds, O_NONBLOCK | O_CLOEXEC) != 0) {
        qErrnoWarning("Cannot create the SIGTERM notification pipe");
        return -1;
    }
    quitWriteFd = fds[1];
    struct sigaction action {};
    action.sa_handler = handleSigterm;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, nullptr) != 0) {
        qErrnoWarning("Cannot install the SIGTERM handler");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    return fds[0];
}
}

int main(int argc, char *argv[])
{
    const QByteArray option = argc == 2 ? QByteArray(argv[1]) : QByteArray();
    const bool informational = option == "--help";
    std::unique_ptr<QCoreApplication> app;
    int quitReadFd = -1;
    if (informational) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
            && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            qWarning("exit-options: no display available (DISPLAY and WAYLAND_DISPLAY are both unset); "
                    "a graphical session is required to run this program.");
            return EXIT_FAILURE;
        }

        // Set Qt platform to XCB (X11) if not already set and we're in X11 environment
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("DISPLAY")
            && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }

        quitReadFd = installQuitHandler();
        if (quitReadFd < 0) {
            return EXIT_FAILURE;
        }
        app = std::make_unique<QApplication>(argc, argv);
    }

    QApplication::setOrganizationName("MX-Linux");

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
                     DATA_DIR + "/locale")) {
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
    parser.process(*app);
    if (parser.isSet("help")) {
        QTextStream out(stdout);
        out << QObject::tr("Pop-up with exit options for MX Fluxbox") << '\n';
        out << QObject::tr("Usage: exit-options [options]\n") << '\n';
        out << QObject::tr("Options:") << '\n';
        out << QObject::tr("  -h, --horizontal\t Option to display buttons horizontally") << '\n';
        out << QObject::tr("  -v, --vertical\t Option to display buttons vertically") << '\n';
        out << QObject::tr("  -t, --timeout <sec>\t Timeout duration in seconds\n") << '\n';
        out << QObject::tr("The display orientation option used will be remembered and used the next time you start the app")
            << '\n';
        out << QObject::tr("Alternatively, set the option 'layout=horizontal' or 'layout=vertical' in "
                           "~/.config/MX-Linux/exit-options.conf") << "\n\n";
        out << QObject::tr("To set the timeout in exit-options.conf use 'timeout=X' where X is the timeout in seconds.")
            << '\n';
        out << QObject::tr("You can also use 'timeout=off' to turn the timeout off.") << "\n\n";
        out << QObject::tr("You can define custom icons by adding IconName=/path/iconame.ext in the exit-options.conf "
                           "file. The names of the icons that you remap: %1")
                   .arg("LockIcon, LogoutIcon, SuspendIcon, RebootIcon, ShutdownIcon.") << "\n\n";
        out << QObject::tr("Other options that can be set in the exit-options.conf file: %1")
                   .arg("IconSize=, Margin=, Spacing=") << '\n';
        return EXIT_SUCCESS;
    }

    // Allow informational invocations like --help to bypass single-instance toggle behavior.
    QProcess proc;
    const QStringList instanceMatch {"--euid", QString::number(geteuid()), "--exact", QApplication::applicationName()};
    proc.start("pgrep", QStringList {"--count"} + instanceMatch);
    proc.waitForFinished();
    if (proc.exitCode() == 0 && proc.readAllStandardOutput().trimmed().toInt() > 1) {
        QProcess::execute("pkill", QStringList {"--oldest"} + instanceMatch);
        return EXIT_SUCCESS;
    }

    MainWindow mainWindow(parser);
    mainWindow.show();
    QSocketNotifier quitNotifier(quitReadFd, QSocketNotifier::Read);
    QObject::connect(&quitNotifier, &QSocketNotifier::activated, app.get(), [&quitNotifier] {
        quitNotifier.setEnabled(false);
        QCoreApplication::quit();
    });

    // Load timeout settings
    const QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    const QSettings systemSettings(SYSTEM_CONFIG_PATH, QSettings::IniFormat);

    QString timeout;
    if (parser.isSet("timeout")) {
        timeout = parser.value("timeout");
    } else if (userSettings.contains("timeout")) {
        timeout = userSettings.value("timeout").toString();
    } else if (systemSettings.contains("timeout")) {
        timeout = systemSettings.value("timeout").toString();
    } else {
        timeout = QStringLiteral("off");
    }

    // Set up timeout for quitting the application
    if (timeout != QStringLiteral("off")) {
        bool ok {false};
        const auto timeoutDuration = std::chrono::seconds(timeout.toUInt(&ok));
        if (ok) {
            QTimer::singleShot(timeoutDuration, &mainWindow, &MainWindow::quitOnTimeout);
        }
    }
    return QApplication::exec();
}
