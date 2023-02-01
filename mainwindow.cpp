#include <QApplication>
#include <QLayout>
#include <QProcess>
#include <QPushButton>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent)
    : QDialog(parent)
{
    auto *pushLock = new QPushButton(QIcon(":/icons/system-lock-mxfb.png"), QString());
    auto *pushExit = new QPushButton(QIcon(":/icons/system-log-out.png"), QString());
    auto *pushSleep = new QPushButton(QIcon(":/icons/system-sleep.png"), QString());
    auto *pushRestart = new QPushButton(QIcon(":/icons/system-restart.png"), QString());
    auto *pushShutdown = new QPushButton(QIcon(":/icons/system-shutdown.png"), QString());
    pushLock->setToolTip(tr("Lock Screen"));
    pushExit->setToolTip(tr("Log Out"));
    pushSleep->setToolTip(tr("Suspend"));
    pushRestart->setToolTip(tr("Reboot"));
    pushShutdown->setToolTip(tr("Shutdown"));

    connect(pushLock, &QPushButton::clicked, this, &MainWindow::on_pushLock);
    connect(pushExit, &QPushButton::clicked, this, &MainWindow::on_pushExit);
    connect(pushSleep, &QPushButton::clicked, this, &MainWindow::on_pushSleep);
    connect(pushRestart, &QPushButton::clicked, this, &MainWindow::on_pushRestart);
    connect(pushShutdown, &QPushButton::clicked, this, &MainWindow::on_pushShutdown);
    QList<QPushButton *> btnList {pushLock, pushExit, pushSleep, pushRestart, pushShutdown};

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
    if (QStringList {"xfce", "KDE", "i3", "fluxbox"}.contains(qgetenv("XDG_SESSION_DESKTOP"))
        || QProcess::execute("pidof", {"-q", "fluxbox"}) == 0
        || QProcess::execute("systemctl", {"is-active", "--quiet", "service"}) == 0)
        layout->addWidget(pushExit);
    if (!isRaspberryPi())
        layout->addWidget(pushSleep);
    layout->addWidget(pushRestart);
    layout->addWidget(pushShutdown);
    setLayout(layout);

    const int iconSize = 50;
    for (auto *btn : btnList) {
        btn->setAutoDefault(false);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        btn->setIconSize(QSize(iconSize, iconSize));
    }

    this->setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    connect(QApplication::instance(), &QApplication::aboutToQuit, [this] { saveSettings(); });

    if (settings.contains("geometry")) {
        QByteArray geometry = saveGeometry();
        restoreGeometry(settings.value("geometry").toByteArray());
        // if too wide/tall reset geometry
        if ((horizontal && size().height() >= size().width()) || (!horizontal && size().height() <= size().width()))
            restoreGeometry(geometry);
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

void MainWindow::on_pushRestart() { QProcess::startDetached("sudo", {"-n", "reboot"}); }

void MainWindow::on_pushShutdown() { QProcess::startDetached("sudo", {"-n", "/sbin/halt"}); }

bool MainWindow::isRaspberryPi() { return QProcess::execute("test", {"-f", "/etc/rpi-issue"}) == 0; }
