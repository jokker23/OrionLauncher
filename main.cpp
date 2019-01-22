/***********************************************************************************
**
** Main.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#include "orionlauncherwindow.h"
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
        child.setProgram("\"" + app + "\"");
        child.startDetached();
        return 0;
    }
#endif
    OrionLauncherWindow w;
    w.show();
    return a.exec();
}
