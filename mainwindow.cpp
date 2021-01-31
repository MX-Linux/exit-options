#include <QApplication>
#include <QLayout>
#include <QPushButton>
#include <QSettings>

#include "mainwindow.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent) :
    QDialog(parent)
{
    QSettings settings("MX-Linux", qApp->applicationName());

    QPushButton *buttonLock = new QPushButton(QIcon("/usr/local/share/icons/system-lock-mxfb.png"), QString());
    QPushButton *buttonExit = new QPushButton(QIcon("/usr/local/share/icons/system-log-out.png"), QString());
    QPushButton *buttonSleep = new QPushButton(QIcon("/usr/local/share/icons/system-sleep.png"), QString());
    QPushButton *buttonRestart = new QPushButton(QIcon("/usr/local/share/icons/system-restart.png"), QString());
    QPushButton *buttonShutdown = new QPushButton(QIcon("/usr/local/share/icons/system-shutdown.png"), QString());

    connect(buttonLock, &QPushButton::clicked, this, &MainWindow::on_buttonLock);
    connect(buttonExit, &QPushButton::clicked, this, &MainWindow::on_buttonExit);
    connect(buttonSleep, &QPushButton::clicked, this, &MainWindow::on_buttonSleep);
    connect(buttonRestart, &QPushButton::clicked, this, &MainWindow::on_buttonRestart);
    connect(buttonShutdown, &QPushButton::clicked, this, &MainWindow::on_buttonShutdown);
    QList<QPushButton *> btnList {buttonLock, buttonExit, buttonSleep, buttonRestart, buttonShutdown};

    QBoxLayout *layout;
    if ((settings.value("layout").toString() == "horizontal" or arg_parser.isSet("horizontal"))
            and not arg_parser.isSet("vertical")) {
        horizontal = true;
        layout = new QHBoxLayout(this);
    } else {
        horizontal = false;
        layout = new QVBoxLayout(this);
    }

    layout->addWidget(buttonLock);
    layout->addWidget(buttonExit);
    if (not isRaspberryPi()) layout->addWidget(buttonSleep);
    layout->addWidget(buttonRestart);
    layout->addWidget(buttonShutdown);
    setLayout(layout);

    for (auto btn : btnList) {
        btn->setAutoDefault(false);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        btn->setIconSize(QSize(50, 50));
    }

    this->setSizeGripEnabled(true);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    connect(qApp, &QApplication::aboutToQuit, [this] { saveSettings(); });

    if (settings.contains("geometry")) {
       QByteArray geometry = saveGeometry();
       restoreGeometry(settings.value("geometry").toByteArray());
       // if too wide/tall reset geometry
       if ((horizontal && size().height() >= size().width() ) ||
               (!horizontal && size().height() <= size().width()))
                   restoreGeometry(geometry);
    }
}

MainWindow::~MainWindow()
{    
}

void MainWindow::on_buttonLock()
{   
    system("dm-tool switch-to-greeter &");
}

void MainWindow::on_buttonExit()
{
    system("fluxbox-remote 'Exit' || killall fluxbox &");
}

void MainWindow::on_buttonSleep()
{
    system("sudo pm-suspend &");
}

void MainWindow::saveSettings()
{
    QSettings settings("MX-Linux", qApp->applicationName());
    settings.setValue("geometry", saveGeometry());
    if (horizontal)
        settings.setValue("layout", "horizontal");
    else
        settings.setValue("layout", "vertical");
}

void MainWindow::on_buttonRestart()
{
    system("sudo reboot &");
}

void MainWindow::on_buttonShutdown()
{
    system("sudo /sbin/halt &");
}

bool MainWindow::isRaspberryPi()
{
    return (system("[ -f /etc/rpi-issue ]") == 0);
}
