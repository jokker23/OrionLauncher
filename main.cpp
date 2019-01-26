/***********************************************************************************
**
** Main.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#include "launcherwindow.h"
#include <QApplication>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#if defined(USE_RELAUNCH) && defined(USE_RELEASE)
    if (!qApp->applicationFilePath().endsWith("_"))
    {
        auto app = qApp->applicationFilePath() + "_";
        QFile::remove(app);
        QFile::copy(qApp->applicationFilePath(), app);
        QProcess child;
        auto exe = "\"" + app + "\"";
        const auto success = child.startDetached(exe);
        if (!success)
        {
            QString err(QString("Could not start\nReason:\n"));
            QMessageBox::critical(nullptr, "Error", err + child.errorString());
        }
        QThread::msleep(250);
        return 0;
    }
#endif
    LauncherWindow w;
    w.show();
    return a.exec();
}
