#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QMap>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

#include "common.h"
#include "mainwindow.h"

namespace
{
bool isExecutableAvailable(const QString &program)
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

bool executeWithFallback(const QString &primary, const QStringList &primaryArgs, const QString &fallback,
                         const QStringList &fallbackArgs)
{
    if (isExecutableAvailable(primary) && QProcess::execute(primary, primaryArgs) == 0) {
        return true;
    }
    if (isExecutableAvailable(fallback) && QProcess::execute(fallback, fallbackArgs) == 0) {
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

    iconSize = qBound(16, userSettings.value("IconSize", systemSettings.value("IconSize", defaultIconSize).toUInt()).toInt(), 256);

    // Detect desktop environment
    QString sessionDesktop = getDesktopEnvironment();
    if (sessionDesktop.isEmpty()) {
        sessionDesktop = QStringLiteral("unknown");
    }

    // Set up desktop-specific restart label
    static const QMap<QString, QString> desktopLabels
        = {{"fluxbox", tr("Restart Fluxbox")}, {"icewm-session", tr("Restart IceWM")}, {"jwm", tr("Restart JWM")}};
    const QString restartDeLabel = desktopLabels.value(sessionDesktop, QString());

    // Create buttons
    auto *restartDeButton = createButton("RestartFluxbox", DATA_DIR + "/awesome/refresh.png",
                                         restartDeLabel, [this] { onPushRestartDe(); });
    auto *exitButton = createButton("LogoutIcon", DATA_DIR + "/awesome/logout.png", tr("Log Out"),
                                    [this] { onPushExit(); });
    auto *lockButton = createButton("LockIcon", DATA_DIR + "/awesome/lock.png", tr("Lock Screen"),
                                    [this] { onPushLock(); });
    auto *rebootButton = createButton("RebootIcon", DATA_DIR + "/awesome/reboot.png", tr("Reboot"),
                                      [this] { onPushRestart(); });
    auto *shutdownButton = createButton("ShutdownIcon", DATA_DIR + "/awesome/shutdown.png", tr("Shutdown"),
                                        [this] { onPushShutdown(); });
    auto *suspendButton = createButton("SuspendIcon", DATA_DIR + "/awesome/suspend.png", tr("Suspend"),
                                       [this] { onPushSleep(); });

    // Determine layout orientation
    if (!horizontal && !parser.isSet("vertical")) {
        horizontal = userSettings.value("layout", systemSettings.value("layout").toString()).toString() == "horizontal";
    }
    auto *layout
        = horizontal ? static_cast<QLayout *>(new QHBoxLayout(this)) : static_cast<QLayout *>(new QVBoxLayout(this));

    // Add buttons to layout
    if (desktopLabels.contains(sessionDesktop)) {
        layout->addWidget(restartDeButton);
    }

    layout->addWidget(lockButton);
    layout->addWidget(exitButton);

    if (!isRaspberryPi()) {
        layout->addWidget(suspendButton);
    }

    layout->addWidget(rebootButton);
    layout->addWidget(shutdownButton);

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
        const auto currentGeometry = saveGeometry();
        const auto savedGeometry
            = userSettings.value("Geometry", userSettings.value("geometry").toByteArray()).toByteArray();

        if (!savedGeometry.isEmpty() && restoreGeometry(savedGeometry)) {
            // Reset geometry if window proportions don't match the current layout orientation
            const double currentAspectRatio = static_cast<double>(size().width()) / size().height();

            // For horizontal layout, expect width > height (aspect ratio > 1)
            // For vertical layout, expect height > width (aspect ratio < 1)
            // Use reasonable thresholds to avoid resetting valid geometries
            const bool geometryMismatch = horizontal ? (currentAspectRatio < 1.1)  // Horizontal but too tall/square
                                                     : (currentAspectRatio > 0.9); // Vertical but too wide/square

            if (geometryMismatch) {
                restoreGeometry(currentGeometry);
            }
        }
    }
}

void MainWindow::onPushLock()
{
    // Try common screen lockers in order of preference
    if (!executeWithFallback("dm-tool", {"switch-to-greeter"}, "loginctl", {"lock-session"})) {
        executeWithFallback("xscreensaver-command", {"-lock"}, "gnome-screensaver-command", {"-l"});
    }
}

void MainWindow::onPushExit()
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
        const auto commandPair = desktopCommands.value(sessionDesktop);
        const auto &program = commandPair.first;
        const auto &arguments = commandPair.second;

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

void MainWindow::onPushSleep()
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

void MainWindow::onPushRestart()
{
    // Try systemd first, then loginctl, then direct reboot with sudo
    if (!executeWithFallback("systemctl", {"reboot"}, "loginctl", {"reboot"})) {
        executeWithFallback("sudo", {"-n", "reboot"}, "reboot", {});
    }
}

void MainWindow::onPushRestartDe()
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

void MainWindow::onPushShutdown()
{
    // Try systemd first, then loginctl, then direct shutdown with sudo
    if (!executeWithFallback("systemctl", {"poweroff"}, "loginctl", {"poweroff"})) {
        executeWithFallback("sudo", {"-n", "/sbin/halt", "-p"}, "/sbin/halt", {"-p"});
    }
}

bool MainWindow::isRaspberryPi()
{
    static const bool isRunningOnRaspberryPi = QFile::exists("/etc/rpi-issue");
    return isRunningOnRaspberryPi;
}
