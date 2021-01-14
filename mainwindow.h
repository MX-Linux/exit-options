#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_buttonExit_clicked();
    void on_buttonLock_clicked();
    void on_buttonRestart_clicked();
    void on_buttonShutdown_clicked();
    void on_buttonSleep_clicked();
    void saveSettings();

private:
    Ui::MainWindow *ui;
    bool isRaspberryPi();
};

#endif // MAINWINDOW_H
