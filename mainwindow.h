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
    ~MainWindow() = default;

private slots:
    static void on_pushExit();
    static void on_pushLock();
    static void on_pushRestart();
    static void on_pushRestartFluxbox();
    static void on_pushShutdown();
    static void on_pushSleep();
    void saveSettings();

private:
    bool horizontal;
    QSettings settings;

    static bool isRaspberryPi();
    void reject() override;
};

#endif // MAINWINDOW_H
