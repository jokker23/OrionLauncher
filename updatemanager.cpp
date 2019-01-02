/*
* UpdateManager.cpp
* Copyright (c) 2018 Danny Angelo Carminati Grein
* This file is released under the GPL license
*/
#include "updatemanager.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDirIterator>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>

ProgressDialog::ProgressDialog(const QString &filename, QWidget *parent)
    : QProgressDialog(parent)
{
    setWindowTitle(tr("Download Progress"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setCancelButton(nullptr);
    setLabelText(tr("Downloading to %1.").arg(filename));
    setMinimum(0);
    setValue(0);
    setMinimumDuration(0);
    setMinimumSize(QSize(400, 75));
}

void ProgressDialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    setMaximum(int(totalBytes));
    setValue(int(bytesRead));
}

UpdateManager::UpdateManager(const QString &host, const QString &userAgent, QObject *parent)
    : QObject(parent)
    , host(host)
    , userAgent(userAgent)
{
    manager = new QNetworkAccessManager(this);
}

UpdateManager::~UpdateManager()
{
    delete manager;
}

void UpdateManager::setCacheDirectory(const QString &cachePath)
{
    QDir dir(cachePath);
    dir.mkpath(cachePath);
    this->cachePath = cachePath;
}

QByteArray UpdateManager::getHash(const QString &fileName)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Sha1);
        if (hash.addData(&f))
        {
            return hash.result();
        }
    }
    return QByteArray();
}

void UpdateManager::getManifest(const QString &endpoint)
{
    get(endpoint, [this](QNetworkReply *reply) {
        const auto data = readReply(reply);
        ParseManifest(data);
        emit backupsListReceived(backupList);
        emit updatesListReceived(updateList);
    });
}

void UpdateManager::getChangelog(const QString &endpoint)
{
    get(endpoint, [this](QNetworkReply *reply) {
        const auto data = readReply(reply);
        emit changelogReceived(data);
    });
}

template <typename F>
void UpdateManager::get(const QString &endpoint, F func)
{
    QNetworkRequest request;
    request.setUrl(QUrl(host + endpoint));
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() { func(reply); });
}

bool UpdateManager::validateFile(const QFile &)
{
    // validate file checksum based on entries in backupInfo or updateInfo
    return true;
}

void UpdateManager::getFile(
    const QString &endpoint,
    const QString &filename,
    std::function<void(const QString &)> finishedCb,
    QWidget *parent,
    bool silent)
{
    QString dir{ cachePath + "/" + filename };
    auto file = new QFile{ dir };
    if (file->exists() && validateFile(*file))
    {
        finishedCb(dir);
        return;
    }

    file->open(QIODevice::WriteOnly);
    const auto url = QUrl(host + endpoint + "/" + filename);
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    auto reply = manager->get(request);
    if (!silent)
    {
        auto progressDialog = new ProgressDialog(filename, parent);
        progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(
            reply,
            &QNetworkReply::downloadProgress,
            progressDialog,
            &ProgressDialog::networkReplyProgress);
        connect(reply, &QNetworkReply::finished, progressDialog, &ProgressDialog::hide);
        progressDialog->show();
    }
    connect(
        reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
            emit downloadProgress(bytesRead, totalBytes);
        });
    connect(
        reply, &QIODevice::readyRead, this, [file, reply] { file->write(reply->readAll()); });

    connect(reply, &QNetworkReply::finished, this, [=] {
        QFileInfo fi;
        if (file)
        {
            fi.setFile(file->fileName());
            file->close();
            delete file;
        }

        if (reply->error())
        {
            auto err = reply->errorString();
            QFile::remove(fi.absoluteFilePath());
            reply->deleteLater();
            return;
        }

        if (validateFile(*file))
        {
            finishedCb(dir);
        }
    });
}

QByteArray UpdateManager::readReply(QNetworkReply *reply)
{
    if (httpRequestAborted || reply->error())
    {
        auto err = reply->errorString();
        reply->deleteLater();
        reply = nullptr;
        return {};
    }
    auto data = reply->readAll();
    reply->deleteLater();
    reply = nullptr;
    return data;
}

void UpdateManager::streamReadyRead(QNetworkReply *reply, QFile *file)
{
    if (file)
        file->write(reply->readAll());
}

void UpdateManager::ParseManifest(const QString &xml)
{
#define ReadAttribute(var, name)                                                                   \
    if (attributes.hasAttribute(name))                                                             \
    var = attributes.value(name).toString()

    updateList.clear();
    backupList.clear();
    QString tmp;

    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.hasError())
    {
        if (reader.isStartElement())
        {
            auto attributes = reader.attributes();
            if (reader.name().toString().trimmed().toLower() == "meta")
            {
                CFileInfo info;
                ReadAttribute(info.Name, "name");
                info.Name = info.Name.replace("\\", "/");
                if (info.Name.length())
                {
                    ReadAttribute(tmp, "uodir");
                    info.UODir = tmp == "yes";

                    ReadAttribute(tmp, "hash");
                    info.Hash = QByteArray::fromHex(tmp.toLatin1());

                    ReadAttribute(info.ZipFileName, "filename");
                    info.ZipFileName = info.ZipFileName.replace("\\", "/");

                    ReadAttribute(tmp, "ziphash");
                    info.ZipHash = QByteArray::fromHex(tmp.toLatin1());

                    updateList.push_back(info);
                }
                else
                {
                    ReadAttribute(info.Name, "backup");
                    info.Name = info.Name.replace("\\", "/");
                    if (info.Name.length())
                    {
                        ReadAttribute(info.ZipFileName, "filename");
                        info.ZipFileName = info.ZipFileName.replace("\\", "/");

                        ReadAttribute(tmp, "ziphash");
                        info.ZipHash = QByteArray::fromHex(tmp.toLatin1());

                        backupList.push_back(info);
                    }
                }
            }
        }
        reader.readNext();
    }
}

void UpdateManager::writeManifest(const QString &filename)
{
    QFile file{ filename };
    file.open(QIODevice::WriteOnly);
    QXmlStreamWriter stream(&file);

    stream.setAutoFormatting(true);
    stream.setAutoFormattingIndent(4);
    stream.writeStartDocument();
    stream.writeStartElement("html");
    stream.writeStartElement("head");
    stream.writeStartElement("meta");
    stream.writeAttribute("http-equiv", "Content-Type");
    stream.writeAttribute("content", "text/html; charset=UTF-8");
    stream.writeEndElement(); // meta
    stream.writeStartElement("meta");
    stream.writeAttribute("charset", "UTF-8");
    stream.writeEndElement(); // meta

    for (const auto &item : updateList)
    {
        auto hash = getHash((item.UODir ? cachePath + "/orionuo" : cachePath) + "/" + item.Name);
        const auto zipHash = getHash(cachePath + "/" + item.ZipFileName);

        stream.writeStartElement("meta");
        stream.writeAttribute("name", item.Name);
        stream.writeAttribute("hash", hash.toHex());
        stream.writeAttribute("filename", item.ZipFileName);
        stream.writeAttribute("filehash", zipHash.toHex());

        if (item.UODir)
            stream.writeAttribute("uodir", "yes");

        stream.writeEndElement(); // meta
    }

    for (auto &item : backupList)
    {
        const auto zipHash = getHash(cachePath + "/" + item.ZipFileName);

        stream.writeStartElement("meta");
        stream.writeAttribute("backup", item.Name);
        stream.writeAttribute("filename", item.ZipFileName);
        stream.writeAttribute("filehash", zipHash.toHex());
        stream.writeEndElement(); // meta
    }

    stream.writeEndElement(); // head
    stream.writeEndElement(); // html
    stream.writeEndDocument();

    file.close();
}

void UpdateManager::generateManifestData(const QString &path)
{
    cachePath = path;

    QDirIterator it{ path, QStringList() << "*.*", QDir::Files, QDirIterator::Subdirectories };
    while (it.hasNext())
    {
        // build a new manifest from scratch
        // change file format to something simpler to generate
        // change folder structures
    }
}
