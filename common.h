#pragma once

#include <QString>

inline const QString SYSTEM_CONFIG_PATH = QStringLiteral("/etc/exit-options.conf");
inline const QString DATA_DIR = QStringLiteral("/usr/share/exit-options");

inline QString getDesktopEnvironment()
{
    return QString::fromUtf8(qgetenv("XDG_SESSION_DESKTOP")).toLower();
}