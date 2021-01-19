#include <QSettings>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    connect(qApp, &QApplication::aboutToQuit, [this] { saveSettings(); });

    QSettings settings("MX-Linux", qApp->applicationName());
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    ui->buttonSleep->setHidden(isRaspberryPi());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_buttonLock_clicked()
{
    system("dm-tool switch-to-greeter &");
}

void MainWindow::on_buttonExit_clicked()
{
    system("fluxbox-remote 'Exit' || killall fluxbox &");
}

void MainWindow::on_buttonSleep_clicked()
{
    system("sudo pm-suspend &");
}

void MainWindow::saveSettings()
{
    QSettings settings("MX-Linux", qApp->applicationName());
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::on_buttonRestart_clicked()
{
    system("sudo reboot &");
}

void MainWindow::on_buttonShutdown_clicked()
{
    system("sudo /sbin/halt &");
}

bool MainWindow::isRaspberryPi()
{
    return (system("[ -f /etc/rpi-issue ]") == 0);
}
