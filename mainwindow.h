#pragma once

#include <QCommandLineParser>
#include <QDialog>
#include <QSettings>

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &parser, QWidget *parent = nullptr);

private slots:
    static void onPushExit();
    static void onPushLock();
    static void onPushRestart();
    static void onPushRestartDe();
    static void onPushShutdown();
    static void onPushSleep();
    void saveSettings();

private:
    bool horizontal;
    const int defaultIconSize;
    const int defaultSpacing;
    int iconSize;
    QSettings userSettings;
    QSettings systemSettings;

    static bool isRaspberryPi();
    void reject() override;
    QPushButton *createButton(const QString &iconName, const QString &iconLocation, const QString &toolTip,
                              const std::function<void()> &action);
};
