#-------------------------------------------------
#
# Project created by QtCreator 2021-01-13T21:15:40
#
#-------------------------------------------------

QT       += core gui widgets
CONFIG   += debug_and_release warn_on strict_c++ c++17

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -flto=auto
    QMAKE_LFLAGS += -flto=auto
    QMAKE_CXXFLAGS_RELEASE = -O3
}

QMAKE_CXXFLAGS += -Wpedantic -pedantic -Werror=return-type -Werror=switch
QMAKE_CXXFLAGS += -Werror=uninitialized -Werror=return-local-addr -Werror

TARGET = exit-options
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h

TRANSLATIONS += \
        translations/exit-options_en.ts

