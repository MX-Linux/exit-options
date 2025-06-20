#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QMap>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

#include "mainwindow.h"
#include "common.h"

namespace {
    bool isExecutableAvailable(const QString &program) {
        return !QStandardPaths::findExecutable(program).isEmpty();
    }

    bool executeWithFallback(const QString &primary, const QStringList &primaryArgs,
                            const QString &fallback, const QStringList &fallbackArgs) {
        if (isExecutableAvailable(primary) && QProcess::execute(primary, primaryArgs) == 0) {
            return true;
        }
        if (isExecutableAvailable(fallback)) {
            QProcess::startDetached(fallback, fallbackArgs);
            return true;
        }
        return false;
    }
}

MainWindow::MainWindow(const QCommandLineParser &parser, QWidget *parent)
    : QDialog(parent),
      horizontal {parser.isSet("horizontal")},
      defaultIconSize {50},
      defaultSpacing {3},
      userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName()),
      systemSettings(SYSTEM_CONFIG_PATH, QSettings::IniFormat)
{

    iconSize = userSettings.value("IconSize", systemSettings.value("IconSize", defaultIconSize).toUInt()).toInt();

    // Detect desktop environment
    QString sessionDesktop = getDesktopEnvironment();
    if (sessionDesktop.isEmpty()) {
        sessionDesktop = "unknown";
    }

    // Set up desktop-specific restart label
    static const QMap<QString, QString> desktopLabels
        = {{"fluxbox", tr("Restart Fluxbox")}, {"icewm-session", tr("Restart IceWM")}, {"jwm", tr("Restart JWM")}};
    QString labelRestartDE = desktopLabels.value(sessionDesktop, QString());

    // Create buttons
    auto *pushRestartDE = createButton("RestartFluxbox", "/usr/share/exit-options/awesome/refresh.png", labelRestartDE,
                                       [this] { on_pushRestartDE(); });
    auto *pushExit = createButton("LogoutIcon", "/usr/share/exit-options/awesome/logout.png", tr("Log Out"),
                                  [this] { on_pushExit(); });
    auto *pushLock = createButton("LockIcon", "/usr/share/exit-options/awesome/lock.png", tr("Lock Screen"),
                                  [this] { on_pushLock(); });
    auto *pushRestart = createButton("RebootIcon", "/usr/share/exit-options/awesome/reboot.png", tr("Reboot"),
                                     [this] { on_pushRestart(); });
    auto *pushShutdown = createButton("ShutdownIcon", "/usr/share/exit-options/awesome/shutdown.png", tr("Shutdown"),
                                      [this] { on_pushShutdown(); });
    auto *pushSleep = createButton("SuspendIcon", "/usr/share/exit-options/awesome/suspend.png", tr("Suspend"),
                                   [this] { on_pushSleep(); });

    // Determine layout orientation
    if (!horizontal && !parser.isSet("vertical")) {
        horizontal = userSettings.value("layout", systemSettings.value("layout").toString()).toString() == "horizontal";
    }
    auto *layout
        = horizontal ? static_cast<QLayout *>(new QHBoxLayout(this)) : static_cast<QLayout *>(new QVBoxLayout(this));

    // Add buttons to layout
    if (desktopLabels.contains(sessionDesktop)) {
        layout->addWidget(pushRestartDE);
    }

    layout->addWidget(pushLock);
    layout->addWidget(pushExit);

    if (!isRaspberryPi()) {
        layout->addWidget(pushSleep);
    }

    layout->addWidget(pushRestart);
    layout->addWidget(pushShutdown);

    // Configure layout
    int margin = userSettings.value("Margin", systemSettings.value("Margin", defaultSpacing).toInt()).toInt();
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(userSettings.value("Spacing", systemSettings.value("Spacing", defaultSpacing).toInt()).toInt());
    setLayout(layout);

    // Configure window
    setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Connect signals
    connect(QApplication::instance(), &QApplication::aboutToQuit, this, &MainWindow::saveSettings);

    // Restore window geometry
    show(); // Must show before saving/restoring geometry

    // Ensure window is fully initialized before geometry operations
    QApplication::processEvents();

    if (userSettings.contains("Geometry") || userSettings.contains("geometry")) {
        const QByteArray geometry = saveGeometry();
        const QByteArray savedGeometry = userSettings.value("Geometry", userSettings.value("geometry").toByteArray()).toByteArray();

        if (!savedGeometry.isEmpty() && restoreGeometry(savedGeometry)) {
            // Reset geometry if window proportions don't match the current layout orientation
            const double currentAspectRatio = static_cast<double>(size().width()) / size().height();

            // For horizontal layout, expect width > height (aspect ratio > 1)
            // For vertical layout, expect height > width (aspect ratio < 1)
            // Use reasonable thresholds to avoid resetting valid geometries
            const bool geometryMismatch = horizontal
                ? (currentAspectRatio < 1.1)  // Horizontal but too tall/square
                : (currentAspectRatio > 0.9); // Vertical but too wide/square

            if (geometryMismatch) {
                restoreGeometry(geometry);
            }
        }
    }
}

void MainWindow::on_pushLock()
{
    // Try common screen lockers in order of preference
    if (!executeWithFallback("dm-tool", {"switch-to-greeter"}, "loginctl", {"lock-session"})) {
        executeWithFallback("xscreensaver-command", {"-lock"}, "gnome-screensaver-command", {"-l"});
    }
}

void MainWindow::on_pushExit()
{
    QString sessionDesktop = getDesktopEnvironment();
    bool commandExecuted = false;

    auto executeCommand = [&](const QString &program, const QStringList &arguments) -> bool {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            return false;
        }
        return QProcess::execute(program, arguments) == 0;
    };

    // Map format: {desktop, {program, {arguments}}}
    static const QMap<QString, QPair<QString, QStringList>> desktopCommands
        = {{"fluxbox", {"fluxbox-remote", {"exit"}}},
           {"xfce", {"xfce4-session-logout", {"--logout"}}},
           {"kde", {"qdbus", {"org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"}}},
           {"jwm", {"jwm", {"-exit"}}},
           {"i3", {"i3-msg", {"exit"}}},
           {"mate", {"mate-session-save", {"--logout"}}},
           {"lxde", {"lxsession-logout", {}}},
           {"lxqt", {"lxqt-leave", {"--logout"}}}};

    if (desktopCommands.contains(sessionDesktop)) {
        const QPair<QString, QStringList> &commandPair = desktopCommands.value(sessionDesktop);
        const QString &program = commandPair.first;
        const QStringList arguments = commandPair.second;

        if (sessionDesktop == "fluxbox") {
            if (!executeCommand(program, arguments)) {
                executeCommand("killall", {"fluxbox"});
                commandExecuted = true;
            }
        } else {
            commandExecuted = executeCommand(program, arguments);
        }
    }

    if (!commandExecuted) {
        const QString user = qgetenv("USER");
        if (!executeCommand("loginctl", {"terminate-user", user})) {
            executeCommand("pkill", {"-KILL", "-u", user});
        }
    }
}

void MainWindow::on_pushSleep()
{
    // Try systemd first, then loginctl, then pm-utils with sudo
    if (!executeWithFallback("systemctl", {"suspend"}, "loginctl", {"suspend"})) {
        executeWithFallback("sudo", {"-n", "pm-suspend"}, "pm-suspend", {});
    }
}

void MainWindow::saveSettings()
{
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    userSettings.setValue("geometry", saveGeometry());
    userSettings.setValue("layout", horizontal ? "horizontal" : "vertical");
}

void MainWindow::reject()
{
    QApplication::quit();
}

QPushButton *MainWindow::createButton(const QString &iconName, const QString &iconLocation, const QString &toolTip,
                                      const std::function<void()> &action)
{
    // Get icon path from settings or use default
    QString icon = userSettings.value(iconName, systemSettings.value(iconName).toString()).toString();
    if (icon.isEmpty() || !QFileInfo::exists(icon)) {
        icon = iconLocation;
    }

    // Create and configure button
    auto *btn = new QPushButton(QIcon(icon), QString());
    btn->setToolTip(toolTip);
    btn->setFlat(true);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    btn->setIconSize(QSize(iconSize, iconSize));

    // Only connect if we have a valid action and tooltip
    if (action && !toolTip.isEmpty()) {
        connect(btn, &QPushButton::clicked, this, action);
    }

    return btn;
}

void MainWindow::on_pushRestart()
{
    // Try systemd first, then loginctl, then direct reboot with sudo
    if (!executeWithFallback("systemctl", {"reboot"}, "loginctl", {"reboot"})) {
        executeWithFallback("sudo", {"-n", "reboot"}, "reboot", {});
    }
}

void MainWindow::on_pushRestartDE()
{
    QString sessionDesktop = getDesktopEnvironment();

    static const QMap<QString, QPair<QString, QStringList>> restartCommands
        = {{"fluxbox", {"fluxbox-remote", {"restart"}}},
           {"icewm-session", {"icewm", {"-r"}}},
           {"jwm", {"jwm", {"-restart"}}}};

    if (restartCommands.contains(sessionDesktop)) {
        const auto &command = restartCommands.value(sessionDesktop);
        QProcess::startDetached(command.first, command.second);

        // Special case for Fluxbox - also refresh idesk if available
        if (sessionDesktop == "fluxbox" && !QStandardPaths::findExecutable("idesktoggle").isEmpty()) {
            QProcess::startDetached("idesktoggle", {"idesk", "refresh"});
        }
    }
}

void MainWindow::on_pushShutdown()
{
    // Try systemd first, then loginctl, then direct shutdown with sudo
    if (!executeWithFallback("systemctl", {"poweroff"}, "loginctl", {"poweroff"})) {
        executeWithFallback("sudo", {"-n", "/sbin/halt", "-p"}, "/sbin/halt", {"-p"});
    }
}

bool MainWindow::isRaspberryPi()
{
    static const bool isRaspberryPi = QFile::exists("/etc/rpi-issue");
    return isRaspberryPi;
}
