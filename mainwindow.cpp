#include <QApplication>
#include <QLayout>
#include <QPushButton>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent) :
    QDialog(parent)
{
    auto *pushLock = new QPushButton(QIcon("/usr/local/share/icons/system-lock-mxfb.png"), QString());
    auto *pushExit = new QPushButton(QIcon("/usr/local/share/icons/system-log-out.png"), QString());
    auto *pushSleep = new QPushButton(QIcon("/usr/local/share/icons/system-sleep.png"), QString());
    auto *pushRestart = new QPushButton(QIcon("/usr/local/share/icons/system-restart.png"), QString());
    auto *pushShutdown = new QPushButton(QIcon("/usr/local/share/icons/system-shutdown.png"), QString());

    connect(pushLock, &QPushButton::clicked, this, &MainWindow::on_pushLock);
    connect(pushExit, &QPushButton::clicked, this, &MainWindow::on_pushExit);
    connect(pushSleep, &QPushButton::clicked, this, &MainWindow::on_pushSleep);
    connect(pushRestart, &QPushButton::clicked, this, &MainWindow::on_pushRestart);
    connect(pushShutdown, &QPushButton::clicked, this, &MainWindow::on_pushShutdown);
    QList<QPushButton*> btnList {pushLock, pushExit, pushSleep, pushRestart, pushShutdown};

    QBoxLayout *layout {nullptr};
    if ((settings.value(QStringLiteral("layout")).toString() == QLatin1String("horizontal") || arg_parser.isSet(QStringLiteral("horizontal")))
            && !arg_parser.isSet(QStringLiteral("vertical"))) {
        horizontal = true;
        layout = new QHBoxLayout(this);
    } else {
        horizontal = false;
        layout = new QVBoxLayout(this);
    }

    layout->addWidget(pushLock);
    layout->addWidget(pushExit);
    if (!isRaspberryPi())
        layout->addWidget(pushSleep);
    layout->addWidget(pushRestart);
    layout->addWidget(pushShutdown);
    setLayout(layout);

    for (auto *btn : btnList) {
        btn->setAutoDefault(false);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        btn->setIconSize(QSize(50, 50));
    }

    this->setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    connect(qApp, &QApplication::aboutToQuit, [this] { saveSettings(); });

    if (settings.contains(QStringLiteral("geometry"))) {
       QByteArray geometry = saveGeometry();
       restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
       // if too wide/tall reset geometry
       if ((horizontal && size().height() >= size().width()) || (!horizontal && size().height() <= size().width()))
           restoreGeometry(geometry);
    }
}

void MainWindow::on_pushLock()
{
    system("dm-tool switch-to-greeter &");
}

void MainWindow::on_pushExit()
{
    system("fluxbox-remote 'Exit' || killall fluxbox &");
}

void MainWindow::on_pushSleep()
{
    system("sudo pm-suspend &");
}

void MainWindow::saveSettings()
{
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("layout"), horizontal ? "horizontal" : "vertical");
}

void MainWindow::on_pushRestart()
{
    system("sudo reboot &");
}

void MainWindow::on_pushShutdown()
{
    system("sudo /sbin/halt &");
}

bool MainWindow::isRaspberryPi()
{
    return (system("[ -f /etc/rpi-issue ]") == 0);
}
