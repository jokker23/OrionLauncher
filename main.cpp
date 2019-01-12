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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#if _WINDOWS
    // Self-update hack
    if (!qApp->applicationFilePath().endsWith("_"))
    {
        auto app = qApp->applicationFilePath() + "_";
        QFile::copy(qApp->applicationFilePath(), app);
        QProcess child;
        child.startDetached(app);
        return 0;
    }
#endif
    OrionLauncherWindow w;
    w.show();
    return a.exec();
}
