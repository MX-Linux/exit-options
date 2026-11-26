#include <QApplication>
#include <QDBusConnectionInterface>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

#include "common.h"
#include "mainwindow.h"

namespace
{
bool executeCommand(const QString &program, const QStringList &arguments)
{
    return !QStandardPaths::findExecutable(program).isEmpty() && QProcess::execute(program, arguments) == 0;
}

bool requestPowerAction(const QString &action, const QString &legacyProgram, const QStringList &legacyArgs = {})
{
    if (QFileInfo::exists(QStringLiteral("/run/systemd/system"))) {
        return executeCommand("systemctl", {action});
    }

    auto *bus = QDBusConnection::systemBus().interface();
    if (!bus) {
        return false;
    }
    const QString loginService = QStringLiteral("org.freedesktop.login1");
    const auto registered = bus->isServiceRegistered(loginService);
    if (!registered.isValid()) {
        return false;
    }
    if (registered.value()) {
        return executeCommand("loginctl", {action});
    }
    const auto activatable = bus->activatableServiceNames();
    if (!activatable.isValid()) {
        return false;
    }
    if (activatable.value().contains(loginService)) {
        return executeCommand("loginctl", {action});
    }
    // Only use legacy tools when the bus confirms there is no login manager.
    return executeCommand("sudo", QStringList {"-n", legacyProgram} + legacyArgs)
        || executeCommand(legacyProgram, legacyArgs);
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
    if (!executeCommand("light-locker-command", {"-l"})
        && !executeCommand("xscreensaver-command", {"-lock"})
        && !executeCommand("gnome-screensaver-command", {"-l"})
        && !executeCommand("xdg-screensaver", {"lock"})
        && !executeCommand("loginctl", {"lock-session"})) {
        QMessageBox::warning(this, tr("Lock Screen"), tr("Couldn't request a screen lock."));
    }
}

void MainWindow::onPushExit()
{
    QString sessionDesktop = getDesktopEnvironment();
    bool commandExecuted = false;

    // Map format: {desktop, {program, {arguments}}}
    static const QMap<QString, QPair<QString, QStringList>> desktopCommands
        = {{"fluxbox", {"fluxbox-remote", {"exit"}}},
           {"xfce", {"xfce4-session-logout", {"--logout"}}},
           {"kde", {"qdbus", {"org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"}}},
           {"jwm", {"jwm", {"-exit"}}},
           {"icewm-session", {"icesh", {"logout"}}},
           {"i3", {"i3-msg", {"exit"}}},
           {"mate", {"mate-session-save", {"--logout"}}},
           {"lxde", {"lxsession-logout", {}}},
           {"lxqt", {"lxqt-leave", {"--logout"}}}};

    if (desktopCommands.contains(sessionDesktop)) {
        const auto commandPair = desktopCommands.value(sessionDesktop);
        const auto &program = commandPair.first;
        const auto &arguments = commandPair.second;

        if (sessionDesktop == "fluxbox") {
            commandExecuted = executeCommand(program, arguments);
            if (!commandExecuted) {
                commandExecuted = executeCommand("killall", {"fluxbox"});
            }
        } else {
            commandExecuted = executeCommand(program, arguments);
        }
    }

    if (!commandExecuted) {
        const QString sessionId = qEnvironmentVariable("XDG_SESSION_ID");
        if (sessionId.isEmpty() || !executeCommand("loginctl", {"terminate-session", "--", sessionId})) {
            QMessageBox::warning(this, tr("Log Out"), tr("Couldn't log out of the current session."));
        }
    }
}

void MainWindow::onPushSleep()
{
    requestPowerAction("suspend", "pm-suspend");
}

void MainWindow::saveSettings()
{
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
    requestPowerAction("reboot", "reboot");
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
    requestPowerAction("poweroff", "/sbin/halt", {"-p"});
}

bool MainWindow::isRaspberryPi()
{
    static const bool isRunningOnRaspberryPi = QFile::exists("/etc/rpi-issue");
    return isRunningOnRaspberryPi;
}
