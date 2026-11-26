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
// Map format: {desktop, {program, {arguments}}}
const QMap<QString, QPair<QString, QStringList>> desktopCommands
    = {{"fluxbox", {"fluxbox-remote", {"exit"}}},
       {"xfce", {"xfce4-session-logout", {"--logout"}}},
       {"kde", {"qdbus", {"org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"}}},
       {"jwm", {"jwm", {"-exit"}}},
       {"icewm-session", {"icesh", {"logout"}}},
       {"i3", {"i3-msg", {"exit"}}},
       {"mate", {"mate-session-save", {"--logout"}}},
       {"lxde", {"lxsession-logout", {}}},
       {"lxqt", {"lxqt-leave", {"--logout"}}}};

QString getDesktopEnvironment()
{
    QStringList candidates = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts);
    candidates.prepend(qEnvironmentVariable("XDG_SESSION_DESKTOP"));
    for (const QString &candidate : candidates) {
        QString desktop = candidate.trimmed().toLower();
        if (desktop == "icewm") {
            desktop = QStringLiteral("icewm-session");
        }
        if (desktopCommands.contains(desktop)) {
            return desktop;
        }
    }
    return {};
}

bool executeCommand(const QString &program, const QStringList &arguments, QString &error)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedOutputChannel);
    process.start(executable, arguments);
    const bool finished = process.waitForFinished(-1);
    if (finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return true;
    }

    QString detail = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (detail.isEmpty()) {
        detail = finished && process.exitStatus() == QProcess::NormalExit
            ? QObject::tr("Exit status %1").arg(process.exitCode()) : process.errorString();
    }
    error = program + QStringLiteral(": ") + detail;
    return false;
}

void showActionError(QWidget *parent, const QString &title, const QString &message, const QString &detail)
{
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    box.setDetailedText(detail.isEmpty() ? QObject::tr("No helper is available for this action.") : detail);
    box.exec();
}

bool requestPowerAction(const QString &action, const QString &legacyProgram, const QStringList &legacyArgs, QString &error)
{
    if (QFileInfo::exists(QStringLiteral("/run/systemd/system"))) {
        return executeCommand("systemctl", {action}, error);
    }

    auto *bus = QDBusConnection::systemBus().interface();
    if (!bus) {
        error = QObject::tr("Couldn't connect to the system bus.");
        return false;
    }
    const QString loginService = QStringLiteral("org.freedesktop.login1");
    const auto registered = bus->isServiceRegistered(loginService);
    if (!registered.isValid()) {
        error = registered.error().message();
        return false;
    }
    if (registered.value()) {
        return executeCommand("loginctl", {action}, error);
    }
    const auto activatable = bus->activatableServiceNames();
    if (!activatable.isValid()) {
        error = activatable.error().message();
        return false;
    }
    if (activatable.value().contains(loginService)) {
        return executeCommand("loginctl", {action}, error);
    }
    // Only use legacy tools when the bus confirms there is no login manager.
    return executeCommand("sudo", QStringList {"-n", legacyProgram} + legacyArgs, error)
        || executeCommand(legacyProgram, legacyArgs, error);
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
    const QString sessionDesktop = getDesktopEnvironment();

    // Set up desktop-specific restart label
    static const QMap<QString, QString> desktopLabels
        = {{"fluxbox", tr("Restart Fluxbox")}, {"icewm-session", tr("Restart IceWM")}, {"jwm", tr("Restart JWM")}};

    // Create buttons
    auto *exitButton = createButton("LogoutIcon", DATA_DIR + "/awesome/logout.png", tr("Log Out"),
                                    [this] { onPushExit(); });
    auto *lockButton = createButton("LockIcon", DATA_DIR + "/awesome/lock.png", tr("Lock Screen"),
                                    [this] { onPushLock(); });
    auto *rebootButton = createButton("RebootIcon", DATA_DIR + "/awesome/reboot.png", tr("Reboot"),
                                      [this] { onPushRestart(); });
    auto *shutdownButton = createButton("ShutdownIcon", DATA_DIR + "/awesome/shutdown.png", tr("Shutdown"),
                                        [this] { onPushShutdown(); });

    // Determine layout orientation
    if (!horizontal && !parser.isSet("vertical")) {
        horizontal = userSettings.value("layout", systemSettings.value("layout").toString()).toString() == "horizontal";
    }
    auto *layout
        = horizontal ? static_cast<QLayout *>(new QHBoxLayout(this)) : static_cast<QLayout *>(new QVBoxLayout(this));

    // Add buttons to layout
    if (desktopLabels.contains(sessionDesktop)) {
        layout->addWidget(createButton("RestartFluxbox", DATA_DIR + "/awesome/refresh.png",
                                       desktopLabels.value(sessionDesktop), [this] { onPushRestartDe(); }));
    }

    layout->addWidget(lockButton);
    layout->addWidget(exitButton);

    if (!isRaspberryPi()) {
        layout->addWidget(createButton("SuspendIcon", DATA_DIR + "/awesome/suspend.png", tr("Suspend"),
                                       [this] { onPushSleep(); }));
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
            = userSettings.value("geometry", userSettings.value("Geometry")).toByteArray();

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
    QString error;
    if (!executeCommand("light-locker-command", {"-l"}, error)
        && !executeCommand("xscreensaver-command", {"-lock"}, error)
        && !executeCommand("gnome-screensaver-command", {"-l"}, error)
        && !executeCommand("xdg-screensaver", {"lock"}, error)
        && !executeCommand("loginctl", {"lock-session"}, error)) {
        showActionError(this, tr("Lock Screen"), tr("Couldn't request a screen lock."), error);
    }
}

void MainWindow::onPushExit()
{
    QString sessionDesktop = getDesktopEnvironment();
    QString error;

    if (desktopCommands.contains(sessionDesktop)) {
        const auto commandPair = desktopCommands.value(sessionDesktop);
        const auto &program = commandPair.first;
        const auto &arguments = commandPair.second;

        if (executeCommand(program, arguments, error)
            || (sessionDesktop == "fluxbox" && executeCommand("killall", {"fluxbox"}, error))) {
            return;
        }
    }

    const QString sessionId = qEnvironmentVariable("XDG_SESSION_ID");
    if (!sessionId.isEmpty() && executeCommand("loginctl", {"terminate-session", "--", sessionId}, error)) {
        return;
    }
    if (sessionId.isEmpty() && error.isEmpty()) {
        error = tr("The current session has no session ID.");
    }
    showActionError(this, tr("Log Out"), tr("Couldn't log out of the current session."), error);
}

void MainWindow::onPushSleep()
{
    QString error;
    if (!requestPowerAction("suspend", "pm-suspend", {}, error)) {
        showActionError(this, tr("Suspend"), tr("Couldn't suspend the computer."), error);
    }
}

void MainWindow::saveSettings()
{
    userSettings.setValue("geometry", saveGeometry());
    userSettings.remove("Geometry");
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
    QString error;
    if (!requestPowerAction("reboot", "reboot", {}, error)) {
        showActionError(this, tr("Reboot"), tr("Couldn't reboot the computer."), error);
    }
}

void MainWindow::onPushRestartDe()
{
    QString sessionDesktop = getDesktopEnvironment();

    static const QMap<QString, QPair<QString, QStringList>> restartCommands
        = {{"fluxbox", {"fluxbox-remote", {"restart"}}},
           {"icewm-session", {"icesh", {"restart"}}},
           {"jwm", {"jwm", {"-restart"}}}};

    if (restartCommands.contains(sessionDesktop)) {
        const auto &command = restartCommands.value(sessionDesktop);
        QString error;
        if (!executeCommand(command.first, command.second, error)) {
            showActionError(this, tr("Restart"), tr("Couldn't restart the window manager."), error);
            return;
        }

        // Special case for Fluxbox - also refresh idesk if available
        if (sessionDesktop == "fluxbox" && !QStandardPaths::findExecutable("idesktoggle").isEmpty()) {
            QProcess::startDetached("idesktoggle", {"idesk", "refresh"});
        }
    }
}

void MainWindow::onPushShutdown()
{
    QString error;
    if (!requestPowerAction("poweroff", "/sbin/halt", {"-p"}, error)) {
        showActionError(this, tr("Shutdown"), tr("Couldn't shut down the computer."), error);
    }
}

bool MainWindow::isRaspberryPi()
{
    static const bool isRunningOnRaspberryPi = QFile::exists("/etc/rpi-issue");
    return isRunningOnRaspberryPi;
}
