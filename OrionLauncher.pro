#-------------------------------------------------
#
# Project created by QtCreator 2016-12-20T16:45:23
#
#-------------------------------------------------
VERSION = 1.30.0

DEFINES += APP_VERSION=\\\"$$VERSION\\\"

QT       += core gui  network widgets
# concurrent
TARGET = orionlauncher
TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp\
        orionlauncherwindow.cpp \
        serverlistitem.cpp \
        proxylistitem.cpp \
        changelogform.cpp \
        updatemanager.cpp \
    xxhash.cpp

HEADERS  += orionlauncherwindow.h \
        serverlistitem.h \
        proxylistitem.h \
        qzipreader_p.h \
        changelogform.h \
        updatemanager.h \
        updateinfo.h \
        xxhash.h \
    qzipwriter_p.h


FORMS    += orionlauncherwindow.ui \
    changelogform.ui

OTHER_FILES += orionlauncher.rc

RC_FILE = orionlauncher.rc

RESOURCES += \
    res.qrc
