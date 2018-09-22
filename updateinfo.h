/**
** @file UpdateInfo.hpp
**
** @brief Класс с информацией о обновлении
**
** @author Мустакимов Т.Р.
**
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
**/
#pragma once

#include <QListWidgetItem>

class CFileInfo
{
public:
    CFileInfo() = default;
    QString Name;
    QString ZipFileName;
    QByteArray Hash;
    QByteArray ZipHash;
    bool UODir;
};

class CChangelogInfo
{
public:
    CChangelogInfo() = default;
    QString Name;
    QString Description;
};

class CUpdateInfoListWidgetItem : public QListWidgetItem
{
public:
    CUpdateInfoListWidgetItem(const CFileInfo &info)
        : QListWidgetItem()
        , m_Info(info)
    {
        setText(info.Name);
    }

    virtual ~CUpdateInfoListWidgetItem() {}

    CFileInfo m_Info;
};

class CBackupInfoListWidgetItem : public QListWidgetItem
{
public:
    CBackupInfoListWidgetItem(const CFileInfo &backup)
        : QListWidgetItem()
        , m_Backup(backup)
    {
        setText(backup.Name);
    }

    virtual ~CBackupInfoListWidgetItem() {}

    CFileInfo m_Backup;
};
