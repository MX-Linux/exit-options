#include <QApplication>
#include <QFileInfo>
#include <QLayout>
#include <QProcess>
#include <QPushButton>
#include <QSettings>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &parser, QWidget *parent)
    : QDialog(parent),
      horizontal {parser.isSet("horizontal")},
      defaultIconSize {50},
      defaultSpacing {3}
{
    // Load icons from settings
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    QSettings systemSettings("/etc/exit-options.conf", QSettings::IniFormat);

    iconSize = userSettings.value("IconSize", systemSettings.value("IconSize", defaultIconSize).toUInt()).toInt();

    auto *pushRestartFluxbox = createButton("RestartFluxbox", "/usr/share/exit-options/awesome/refresh.png",
                                            tr("Restart Fluxbox"), on_pushRestartFluxbox);
    auto *pushExit
        = createButton("LogoutIcon", "/usr/share/exit-options/awesome/logout.png", tr("Log Out"), on_pushExit);
    auto *pushLock
        = createButton("LockIcon", "/usr/share/exit-options/awesome/lock.png", tr("Lock Screen"), on_pushLock);
    auto *pushRestart
        = createButton("RebootIcon", "/usr/share/exit-options/awesome/reboot.png", tr("Reboot"), on_pushRestart);
    auto *pushShutdown
        = createButton("ShutdownIcon", "/usr/share/exit-options/awesome/shutdown.png", tr("Shutdown"), on_pushShutdown);
    auto *pushSleep
        = createButton("SuspendIcon", "/usr/share/exit-options/awesome/suspend.png", tr("Suspend"), on_pushSleep);

    if (!horizontal && !parser.isSet("vertical")) {
        horizontal = userSettings.value("layout", systemSettings.value("layout").toString()).toString() == "horizontal";
    }
    auto *layout = horizontal ? new QHBoxLayout(this) : new QVBoxLayout(this);

    // Add pushRestartFluxbox?
    QString xdg_session_desktop = qgetenv("XDG_SESSION_DESKTOP");
    if (xdg_session_desktop == "fluxbox") {
        layout->addWidget(pushRestartFluxbox);
    }
    layout->addWidget(pushLock);
    // Add pushExit?
    if (QStringList {"xfce", "KDE", "i3", "fluxbox"}.contains(xdg_session_desktop)
        || QProcess::execute("pidof", {"-q", "fluxbox"}) == 0
        || QProcess::execute("systemctl", {"is-active", "--quiet", "service"}) == 0) {
        layout->addWidget(pushExit);
    }
    // Add pushSleep?
    if (!isRaspberryPi()) {
        layout->addWidget(pushSleep);
    }
    layout->addWidget(pushRestart);
    layout->addWidget(pushShutdown);

    // Set layout margins and spacing
    layout->setMargin(userSettings.value("Margin", systemSettings.value("Margin", defaultSpacing).toInt()).toInt());
    layout->setSpacing(userSettings.value("Spacing", systemSettings.value("Spacing", defaultSpacing).toInt()).toInt());

    setLayout(layout);

    // Set window features
    setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    connect(QApplication::instance(), &QApplication::aboutToQuit, this, &MainWindow::saveSettings);

    // Restore or reset geometry
    show(); // can't save geometry if not shown
    if (userSettings.contains("Geometry") || userSettings.contains("geometry")) {
        const QByteArray geometry = saveGeometry();
        restoreGeometry(userSettings.value("Geometry", userSettings.value("geometry").toByteArray()).toByteArray());
        // if too wide/tall reset geometry
        const auto factor = 0.6;
        if ((horizontal && size().height() >= size().width() * factor)
            || (!horizontal && size().width() >= size().height() * factor)) {
            restoreGeometry(geometry);
        }
    }
}

void MainWindow::on_pushLock()
{
    QProcess::startDetached("dm-tool", {"switch-to-greeter"});
}

void MainWindow::on_pushExit()
{
    QString sessionDesktop = qgetenv("XDG_SESSION_DESKTOP");
    if (QProcess::execute("pidof", {"-q", "fluxbox"}) == 0) {
        QProcess::execute("fluxbox-remote", {"exit"});
        QProcess::startDetached("killall", {"fluxbox"});
    } else if (sessionDesktop == "xfce") {
        QProcess::startDetached("xfce4-session-logout", {"--logout"});
    } else if (sessionDesktop == "KDE") {
        QProcess::startDetached("qdbus", {"org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"});
    } else if (sessionDesktop == "i3") {
        QProcess::startDetached("i3-msg", {"exit"});
    } else {
        QProcess::startDetached("loginctl", {"terminate-user", qgetenv("USER")});
    }
}

void MainWindow::on_pushSleep()
{
    QProcess::startDetached("sudo", {"-n", "pm-suspend"});
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
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    QSettings systemSettings("/etc/exit-options.conf", QSettings::IniFormat);
    QString icon = userSettings.value(iconName, systemSettings.value(iconName).toString()).toString();
    if (!QFileInfo::exists(icon)) {
        icon = iconLocation;
    }
    auto *btn = new QPushButton(QIcon(icon), QString());
    btn->setToolTip(toolTip);
    btn->setFlat(true);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    btn->setIconSize(QSize(iconSize, iconSize));
    connect(btn, &QPushButton::clicked, this, action);
    return btn;
}

void MainWindow::on_pushRestart()
{
    QProcess::startDetached("sudo", {"-n", "reboot"});
}

void MainWindow::on_pushRestartFluxbox()
{
    QProcess::startDetached("fluxbox-remote", {"restart"});
    QProcess::startDetached("idesktoggle", {"idesk", "refresh"});
}

void MainWindow::on_pushShutdown()
{
    QProcess::startDetached("sudo", {"-n", "/sbin/halt", "-p"});
}

bool MainWindow::isRaspberryPi()
{
    return QFile::exists("/etc/rpi-issue");
}
