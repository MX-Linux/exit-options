#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <chrono>

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

bool executeCommand(const QString &program, const QStringList &arguments, QString &error, QByteArray *output = nullptr)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        return false;
    }

    QProcess process;
    if (!output) {
        process.setProcessChannelMode(QProcess::ForwardedOutputChannel);
    }
    process.start(executable, arguments);
    const bool finished = process.waitForFinished(-1);
    if (finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        if (output) {
            *output = process.readAllStandardOutput();
        }
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

// Check the live EWMH window as well as the root property, which can outlive the WM.
QString fluxboxWindow()
{
    const QString check = QStringLiteral("_NET_SUPPORTING_WM_CHECK");
    const QStringList format {"-notype", "-f", check, "32x", " = $0\\n"};
    QString error;
    QByteArray root;
    if (!executeCommand("xprop", format + QStringList {"-root", check}, error, &root)) {
        return {};
    }
    const QByteArray prefix = "_NET_SUPPORTING_WM_CHECK = ";
    if (!root.startsWith(prefix)) {
        return {};
    }
    const QByteArray id = root.mid(prefix.size()).trimmed();
    bool ok = false;
    if (id.toUInt(&ok, 16) == 0 || !ok) {
        return {};
    }
    const QString window = QString::fromLatin1(id);
    QByteArray properties;
    if (!executeCommand("xprop", format + QStringList {"-id", window, check, "_NET_WM_NAME"}, error, &properties)) {
        return {};
    }
    const auto lines = properties.split('\n');
    if (!lines.contains(prefix + id) || !lines.contains("_NET_WM_NAME = \"Fluxbox\"")) {
        return {};
    }
    return window;
}

QString terminateActiveGraphicalSession(const QString &session)
{
    if (session.isEmpty()) {
        return QObject::tr("The current session has no session ID.");
    }

    // "self" verifies the caller belongs to the graphical session we intend to end.
    const QStringList query {"show-session", "--property=Id", "--property=Type", "--property=State", "self"};
    QString error;
    QByteArray output;
    if (!executeCommand("loginctl", query, error, &output)) {
        return error.isEmpty() ? QObject::tr("Couldn't check the current session with loginctl.") : error;
    }
    const auto properties = output.split('\n');
    if (!properties.contains("Id=" + session.toUtf8()) || !properties.contains("Type=x11")) {
        return QObject::tr("Couldn't verify the current graphical session.");
    }
    if (properties.contains("State=active")
        && !executeCommand("loginctl", {"terminate-session", "--", session}, error)) {
        return error.isEmpty() ? QObject::tr("Couldn't terminate the current session.") : error;
    }
    return {};
}

void showActionError(QWidget *parent, const QString &title, const QString &message, const QString &detail)
{
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    box.setDetailedText(detail.isEmpty() ? QObject::tr("No helper is available for this action.") : detail);
    box.exec();
}

bool requestPowerAction(const QString &method, const QString &sysvProgram, const QStringList &args, QString &error)
{
    if (QFileInfo::exists(QStringLiteral("/run/systemd/system"))) {
        return executeCommand("systemctl", {method.toLower()}, error);
    }

    const auto bus = QDBusConnection::systemBus();
    if (bus.isConnected()) {
        auto request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                      QStringLiteral("/org/freedesktop/login1"),
                                                      QStringLiteral("org.freedesktop.login1.Manager"), method);
        request << true; // Allow the graphical authentication agent to prompt.
        request.setInteractiveAuthorizationAllowed(true);
        const auto reply = bus.call(request);
        if (reply.type() == QDBusMessage::ReplyMessage) {
            return true;
        }
        // Only retry when the service or method was unavailable. A timeout may
        // mean the request was already accepted, and a refusal must be respected.
        const QDBusError failure(reply);
        const bool activationFailed = failure.name().startsWith(QStringLiteral("org.freedesktop.DBus.Error.Spawn."));
        if (failure.type() != QDBusError::ServiceUnknown && failure.type() != QDBusError::UnknownObject
            && failure.type() != QDBusError::UnknownInterface && failure.type() != QDBusError::UnknownMethod
            && !activationFailed) {
            error = failure.message();
            return false;
        }
    } else if (bus.lastError().type() == QDBusError::AccessDenied
               || bus.lastError().name() == QStringLiteral("org.freedesktop.DBus.Error.AuthFailed")) {
        error = bus.lastError().message();
        return false;
    }

    // SysVinit installations may grant access through sudoers, the tool itself,
    // or an administrator authenticating through the desktop's polkit agent.
    const QStringList command = QStringList {sysvProgram} + args;
    return executeCommand("sudo", QStringList {"-n"} + command, error)
        || executeCommand(sysvProgram, args, error)
        || (QFileInfo(sysvProgram).isExecutable() && executeCommand("pkexec", command, error));
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
    if (getDesktopEnvironment() == "xfce"
        && (executeCommand("xflock4", {}, error)
            || executeCommand("xfce4-screensaver-command", {"--lock"}, error))) {
        return;
    }
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

    if (fluxboxLogoutPending) {
        return;
    }

    if (sessionDesktop == "kde") {
        const auto logout = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Shutdown"),
                                                          QStringLiteral("/Shutdown"),
                                                          QStringLiteral("org.kde.Shutdown"),
                                                          QStringLiteral("logout"));
        const auto reply = QDBusConnection::sessionBus().call(logout);
        if (reply.type() == QDBusMessage::ReplyMessage) {
            return;
        }
        error = reply.errorMessage();
        // Older Plasma sessions use the KSMServer command below.
    }

    if (desktopCommands.contains(sessionDesktop)) {
        const auto commandPair = desktopCommands.value(sessionDesktop);
        const auto &program = commandPair.first;
        const auto &arguments = commandPair.second;

        const bool fluxbox = sessionDesktop == "fluxbox";
        const QString originalWindow = fluxbox ? fluxboxWindow() : QString();
        if (executeCommand(program, arguments, error)) {
            if (fluxbox) {
                checkFluxboxLogoutAfterDelay(originalWindow);
            }
            return;
        }
        if (fluxbox && executeCommand("killall", {"fluxbox"}, error)) {
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

void MainWindow::checkFluxboxLogoutAfterDelay(const QString &window)
{
    fluxboxLogoutPending = true;
    const QString session = qEnvironmentVariable("XDG_SESSION_ID");

    // A successful send doesn't tell us whether Fluxbox honored the request.
    QTimer::singleShot(std::chrono::seconds(3), Qt::PreciseTimer, this, [this, window, session] {
        QString detail;
        if (window.isEmpty()) {
            detail = tr("Couldn't identify the original Fluxbox window manager.");
        } else if (window == fluxboxWindow()) {
            detail = terminateActiveGraphicalSession(session);
        }
        if (!detail.isEmpty()) {
            showActionError(this, tr("Log Out"), tr("Couldn't log out of the current session."), detail);
        }
        fluxboxLogoutPending = false;
        if (timeoutExpired) {
            QApplication::quit();
        }
    });
}

void MainWindow::quitOnTimeout()
{
    timeoutExpired = true;
    if (!fluxboxLogoutPending) {
        QApplication::quit();
    }
}

void MainWindow::onPushSleep()
{
    QString error;
    if (!requestPowerAction("Suspend", "/usr/sbin/pm-suspend", {}, error)) {
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
    if (!requestPowerAction("Reboot", "/sbin/reboot", {}, error)) {
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
    if (!requestPowerAction("PowerOff", "/sbin/halt", {"-p"}, error)) {
        showActionError(this, tr("Shutdown"), tr("Couldn't shut down the computer."), error);
    }
}

bool MainWindow::isRaspberryPi()
{
    static const bool isRunningOnRaspberryPi = QFile::exists("/etc/rpi-issue");
    return isRunningOnRaspberryPi;
}
