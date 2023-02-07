#include <QApplication>
#include <QFileInfo>
#include <QLayout>
#include <QProcess>
#include <QPushButton>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent)
    : QDialog(parent)
{
    QPushButton *pushLock {};
    QPushButton *pushExit {};
    QPushButton *pushSleep {};
    QPushButton *pushRestart {};
    QPushButton *pushShutdown {};

    // Build ordered list iconsNames, toolTips, IconFile, btnList
    QList<QPushButton **> btnList {&pushLock, &pushExit, &pushSleep, &pushRestart, &pushShutdown};
    QStringList iconName {"LockIcon", "LogoutIcon", "SuspendIcon", "RebootIcon", "ShutdownIcon"};
    QStringList iconLocation {":/icons/system-lock-mxfb.png", ":/icons/system-log-out.png", ":/icons/system-sleep.png",
                              ":/icons/system-restart.png", ":/icons/system-shutdown.png"};
    QStringList toolTips {tr("Lock Screen"), tr("Log Out"), tr("Suspend"), tr("Reboot"), tr("Shutdown")};
    QList<QFunctionPointer> action = {MainWindow::on_pushLock, MainWindow::on_pushExit, MainWindow::on_pushSleep,
                                      MainWindow::on_pushRestart, MainWindow::on_pushShutdown};
    // Load icons from settings
    for (auto i = 0; i < iconName.size(); ++i) {
        QString icon = settings.value(iconName.at(i)).toString();
        if (QFileInfo::exists(icon))
            iconLocation[i] = icon;
    }

    // Set buttons
    const uint iconSize = settings.value("IconSize", 50).toUInt();
    for (auto i = 0; i < btnList.size(); ++i) {
        *btnList[i] = new QPushButton(QIcon(iconLocation.at(i)), QString());
        (*btnList[i])->setToolTip(toolTips.at(i));
        (*btnList[i])->setFlat(true);
        (*btnList[i])->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        (*btnList[i])->setIconSize(QSize(iconSize, iconSize));
        connect(*btnList.at(i), &QPushButton::clicked, this, action.at(i));
    }

    // Set layout
    QBoxLayout *layout {nullptr};
    if ((settings.value("layout").toString() == QLatin1String("horizontal") || arg_parser.isSet("horizontal"))
        && !arg_parser.isSet("vertical")) {
        horizontal = true;
        layout = new QHBoxLayout(this);
    } else {
        horizontal = false;
        layout = new QVBoxLayout(this);
    }
    layout->addWidget(pushLock);
    // Add pushExit?
    if (QStringList {"xfce", "KDE", "i3", "fluxbox"}.contains(qgetenv("XDG_SESSION_DESKTOP"))
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
    layout->setMargin(settings.value("Margin", default_spacing).toUInt());
    layout->setSpacing(settings.value("Spacing", default_spacing).toUInt());
    setLayout(layout);

    // Set window features
    this->setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Save settings connection
    connect(QApplication::instance(), &QApplication::aboutToQuit, [this] { saveSettings(); });

    // Restore or reset geometry
    this->show(); // can't save geometry if not shown
    if (settings.contains("geometry")) {
        const QByteArray geometry = saveGeometry();
        restoreGeometry(settings.value("geometry").toByteArray());
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
    settings.setValue("geometry", saveGeometry());
    settings.setValue("layout", horizontal ? "horizontal" : "vertical");
}

void MainWindow::reject() { QApplication::quit(); }

void MainWindow::on_pushRestart() { QProcess::startDetached("sudo", {"-n", "reboot"}); }

void MainWindow::on_pushShutdown() { QProcess::startDetached("sudo", {"-n", "/sbin/halt"}); }

bool MainWindow::isRaspberryPi() { return QProcess::execute("test", {"-f", "/etc/rpi-issue"}) == 0; }
