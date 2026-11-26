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
    void onPushExit();
    void onPushLock();
    void onPushRestart();
    void onPushRestartDe();
    void onPushShutdown();
    void onPushSleep();
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
