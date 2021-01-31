#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCommandLineParser>
#include <QDialog>

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &arg_parser, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_buttonExit();
    void on_buttonLock();
    void on_buttonRestart();
    void on_buttonShutdown();
    void on_buttonSleep();
    void saveSettings();

private:
    bool horizontal;

    bool isRaspberryPi();

};

#endif // MAINWINDOW_H
