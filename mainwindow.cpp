#include <QApplication>
#include <QFileInfo>
#include <QLayout>
#include <QProcess>
#include <QPushButton>

#include "mainwindow.h"
#include <array>

MainWindow::MainWindow(const QCommandLineParser &parser, QWidget *parent)
    : QDialog(parent)
{
    QPushButton *pushExit {};
    QPushButton *pushLock {};
    QPushButton *pushRestart {};
    QPushButton *pushRestartFluxbox {};
    QPushButton *pushShutdown {};
    QPushButton *pushSleep {};

    struct ButtonInfo {
        QString iconName;
        QString iconLocation;
        QString toolTip;
        QPushButton **btn;
        std::function<void()> action;
    };

    const int NUM_BUTTONS = 6;
    std::array<ButtonInfo, NUM_BUTTONS> buttons {
        {{"RestartFluxbox", "/usr/share/exit-options/awesome/refresh.png", tr("Restart Fluxbox"), &pushRestartFluxbox,
          [] { MainWindow::on_pushRestartFluxbox(); }},
         {"LockIcon", "/usr/share/exit-options/awesome/lock.png", tr("Lock Screen"), &pushLock,
          [] { MainWindow::on_pushLock(); }},
         {"LogoutIcon", "/usr/share/exit-options/awesome/logout.png", tr("Log Out"), &pushExit,
          [] { MainWindow::on_pushExit(); }},
         {"SuspendIcon", "/usr/share/exit-options/awesome/suspend.png", tr("Suspend"), &pushSleep,
          [] { MainWindow::on_pushSleep(); }},
         {"RebootIcon", "/usr/share/exit-options/awesome/reboot.png", tr("Reboot"), &pushRestart,
          [] { MainWindow::on_pushRestart(); }},
         {"ShutdownIcon", "/usr/share/exit-options/awesome/shutdown.png", tr("Shutdown"), &pushShutdown,
          [] { MainWindow::on_pushShutdown(); }}}};

    // Load icons from settings
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    QSettings systemSettings("/etc/exit-options.conf", QSettings::IniFormat);

    // Set buttons
    const int defaultIconSize = 50;
    int iconSize = userSettings.value("IconSize", systemSettings.value("IconSize", defaultIconSize).toUInt()).toInt();
    for (auto &button : buttons) {
        QString icon = userSettings.value(button.iconName, systemSettings.value(button.iconName).toString()).toString();
        if (QFileInfo::exists(icon))
            button.iconLocation = icon;
        *button.btn = new QPushButton(QIcon(button.iconLocation), QString());
        (*button.btn)->setToolTip(button.toolTip);
        (*button.btn)->setFlat(true);
        (*button.btn)->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        (*button.btn)->setIconSize(QSize(iconSize, iconSize));
        connect(*button.btn, &QPushButton::clicked, this, button.action);
    }

    // Set layout
    QBoxLayout *layout {nullptr};
    horizontal = parser.isSet("horizontal");
    if (!horizontal && !parser.isSet("vertical"))
        horizontal = userSettings.value("layout", systemSettings.value("layout").toString()).toString() == "horizontal";
    layout = horizontal ? static_cast<QBoxLayout *>(new QHBoxLayout(this))
                        : static_cast<QBoxLayout *>(new QVBoxLayout(this));

    // Add pushRestartFluxbox?
    auto xdg_session_desktop = qgetenv("XDG_SESSION_DESKTOP");
    if (xdg_session_desktop == "fluxbox")
        layout->addWidget(pushRestartFluxbox);
    layout->addWidget(pushLock);
    // Add pushExit?
    if (QStringList {"xfce", "KDE", "i3", "fluxbox"}.contains(xdg_session_desktop)
        || QProcess::execute("pidof", {"-q", "fluxbox"}) == 0
        || QProcess::execute("systemctl", {"is-active", "--quiet", "service"}) == 0)
        layout->addWidget(pushExit);
    // Add pushSleep?
    if (!isRaspberryPi())
        layout->addWidget(pushSleep);
    layout->addWidget(pushRestart);
    layout->addWidget(pushShutdown);

    // Spacing
    const auto default_spacing = 3;
    layout->setMargin(userSettings.value("Margin", systemSettings.value("Margin", default_spacing).toInt()).toInt());
    layout->setSpacing(
        userSettings.value("Spacing", systemSettings.value("Spacing", default_spacing).toInt()).toInt());

    setLayout(layout);

    // Set window features
    this->setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Save settings connection
    connect(QApplication::instance(), &QApplication::aboutToQuit, this, [this] { saveSettings(); });

    // Restore or reset geometry
    this->show(); // can't save geometry if not shown
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

void MainWindow::on_pushLock() { QProcess::startDetached("dm-tool", {"switch-to-greeter"}); }

void MainWindow::on_pushExit()
{
    if (QProcess::execute("pidof", {"-q", "fluxbox"}) == 0) {
        QProcess::execute("fluxbox-remote", {"exit"});
        QProcess::startDetached("killall", {"fluxbox"}); // make sure it exits even if remote actions are not enabled
    } else if (qgetenv("XDG_SESSION_DESKTOP") == "xfce") {
        QProcess::startDetached("xfce4-session-logout", {"--logout"});
    } else if (qgetenv("XDG_SESSION_DESKTOP") == "KDE") {
        QProcess::startDetached("qdbus", {"org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"});
    } else if (qgetenv("XDG_SESSION_DESKTOP") == "i3") {
        QProcess::startDetached("i3-msg", {"exit"});
    } else {
        QProcess::startDetached("loginctl", {"terminate-user", qgetenv("USER")});
    }
}

void MainWindow::on_pushSleep() { QProcess::startDetached("sudo", {"-n", "pm-suspend"}); }

void MainWindow::saveSettings()
{
    QSettings userSettings(QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName());
    userSettings.setValue("geometry", saveGeometry());
    userSettings.setValue("layout", horizontal ? "horizontal" : "vertical");
}

void MainWindow::reject() { QApplication::quit(); }

void MainWindow::on_pushRestart() { QProcess::startDetached("sudo", {"-n", "reboot"}); }

void MainWindow::on_pushRestartFluxbox()
{
    QProcess::startDetached("fluxbox-remote", {"restart"});
    QProcess::startDetached("idesktoggle", {"idesk", "refresh"});
}

void MainWindow::on_pushShutdown() { QProcess::startDetached("sudo", {"-n", "/sbin/halt", "-p"}); }

bool MainWindow::isRaspberryPi()
{
    QFile file("/etc/rpi-issue");
    return file.exists();
}
