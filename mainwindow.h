#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCommandLineParser>
#include <QDialog>
#include <QSettings>

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &arg_parser, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushExit();
    void on_pushLock();
    void on_pushRestart();
    void on_pushShutdown();
    void on_pushSleep();
    void saveSettings();

private:
    bool horizontal;
    QSettings settings;

    bool isRaspberryPi();

};

#endif // MAINWINDOW_H
