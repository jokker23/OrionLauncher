/*
* UpdateManager.h
* Copyright (c) 2018 Danny Angelo Carminati Grein
* This file is released under the GPL license
*/
#pragma once

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QList>
#include <QFile>
#include <QProgressDialog>
#include "updateinfo.h"

class ProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString &filename, QWidget *parent = nullptr);

public slots:
    void networkReplyProgress(qint64 bytesRead, qint64 totalBytes);
};

class UpdateManager : public QObject
{
    Q_OBJECT
    QNetworkAccessManager *manager = nullptr;
    bool httpRequestAborted = false;
    QString cachePath;
    QString host;
    QString userAgent;
    QList<CFileInfo> backupList;
    QList<CFileInfo> updateList;

    template <typename F>
    void get(const QString &url, F func);

    QByteArray readReply(QNetworkReply *reply);
    void ParseManifest(const QString &xml);
    bool validateFile(const QFile &filename);

public:
    explicit UpdateManager(
        const QString &host, const QString &userAgent, QObject *parent = nullptr);
    virtual ~UpdateManager();

    void setCacheDirectory(const QString &str);
    QByteArray getHash(const QString &fileName);
    void writeManifest(const QString &filename);
    void generateManifestData(const QString &path);

    void getManifest(const QString &url);
    void getChangelog(const QString &url);
    void getFile(
        const QString &filename,
        const QString &url,
        std::function<void(const QString &)> finishedCb,
        QWidget *parent = nullptr,
        bool silent = false);

signals:
    void backupsListReceived(QList<CFileInfo> &);
    void updatesListReceived(QList<CFileInfo> &);
    void changelogReceived(const QString &);
    void downloadProgress(qint64, qint64);

private slots:
    void streamReadyRead(QNetworkReply *, QFile *);
};
