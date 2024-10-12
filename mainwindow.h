#pragma once

#include <QCommandLineParser>
#include <QDialog>

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &parser, QWidget *parent = nullptr);

private slots:
    static void on_pushExit();
    static void on_pushLock();
    static void on_pushRestart();
    static void on_pushRestartDE();
    static void on_pushShutdown();
    static void on_pushSleep();
    void saveSettings();

private:
    bool horizontal;
    const int defaultIconSize;
    const int defaultSpacing;
    int iconSize;

    static bool isRaspberryPi();
    void reject() override;
    QPushButton *createButton(const QString &iconName, const QString &iconLocation, const QString &toolTip,
                              const std::function<void()> &action);
};
