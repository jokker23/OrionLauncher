/***********************************************************************************
**
** ChangelogForm.cpp
**
** Copyright (C) October 2017 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#include "changelogform.h"
#include "ui_changelogform.h"
#include <QDesktopServices>

ChangelogForm::ChangelogForm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ChangelogForm)
{
    ui->setupUi(this);
    connect(this, SIGNAL(changelogReceived(QString)), this, SLOT(onChangelogReceived(QString)));
    connect(ui->tb_Log, SIGNAL(anchorClicked(QUrl)), this, SLOT(onStartLink(QUrl)));
}

ChangelogForm::~ChangelogForm()
{
    delete ui;
}

void ChangelogForm::onChangelogReceived(const QString &str)
{
    ui->tb_Log->setHtml(str);
}

void ChangelogForm::onStartLink(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}
