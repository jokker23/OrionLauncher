/***********************************************************************************
**
** OrionLauncherWindow.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QApplication>
#include <QtWidgets>

#include "orionlauncherwindow.h"
#include "proxylistitem.h"
#include "serverlistitem.h"
#include "qzipreader_p.h"
#include "ui_orionlauncherwindow.h"

#define LAUNCHER_VERSION "1.3.0"
#define LAUNCHER_TITLE "Orion Launcher " LAUNCHER_VERSION
#define UPDATER_HOST "http://www.orionuo.com/"

#if _WINDOWS
#define EXE_EXTENSION ".exe"
#else
#define EXE_EXTENSION ""
#endif

OrionLauncherWindow::OrionLauncherWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::OrionLauncherWindow)
{
    ui->setupUi(this);

    m_ChangelogForm = new ChangelogForm(nullptr);

    m_UpdateManager = new UpdateManager(UPDATER_HOST, LAUNCHER_TITLE, this);
    m_UpdateManager->setCacheDirectory(qApp->applicationDirPath() + "/cache");
    connect(
        m_UpdateManager,
        &UpdateManager::changelogReceived,
        m_ChangelogForm,
        &ChangelogForm::onChangelogReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::backupsListReceived,
        this,
        &OrionLauncherWindow::onBackupsListReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::updatesListReceived,
        this,
        &OrionLauncherWindow::onUpdatesListReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::downloadProgress,
        this,
        &OrionLauncherWindow::onDownloadProgress);

    connect(
        this,
        SIGNAL(updatesListReceived(const QList<CUpdateInfo> &)),
        this,
        SLOT(onUpdatesListReceived(const QList<CUpdateInfo> &)));
    connect(
        this,
        SIGNAL(backupsListReceived(const QList<CBackupInfo> &)),
        this,
        SLOT(onBackupsListReceived(const QList<CBackupInfo> &)));
    connect(this, SIGNAL(fileReceived(const QDir &)), this, SLOT(onFileReceived(const QDir &)));
    connect(&m_UpdatesTimer, SIGNAL(timeout()), this, SLOT(onUpdatesTimer()));

    setFixedSize(size());

    loadProxyList();
    loadServerList();

#if defined(QT_NO_DEBUG)
    ui->tw_Main->removeTab(5);
#else

#endif

#if !_WINDOWS
    ui->cb_LaunchRunUOAM->setEnabled(false);
    ui->cb_LaunchRunUOAM->setVisible(false);
    ui->cb_LaunchSaveAero->setEnabled(false);
    ui->cb_LaunchSaveAero->setVisible(false);
#endif

    ui->tw_Main->setCurrentIndex(0);
    ui->tw_Server->setCurrentIndex(0);

    updateOAFeaturesCode();
    updateOrionFeaturesCode();

    setWindowTitle(LAUNCHER_TITLE);
    m_Loading = false;

    if (!ui->cb_OrionPath->currentText().length())
        on_tb_SetOrionPath_clicked();

    on_cb_OrionPath_currentIndexChanged(ui->cb_OrionPath->currentIndex());
    m_UpdatesTimer.start(15 * 60 * 1000);
}

OrionLauncherWindow::~OrionLauncherWindow()
{
    delete ui;

    delete m_ChangelogForm;
    m_ChangelogForm = nullptr;

    delete m_UpdateManager;
    m_UpdateManager = nullptr;

    m_UpdatesTimer.stop();
}

void OrionLauncherWindow::onUpdatesTimer()
{
    if (ui->cb_CheckUpdates->isChecked())
        on_pb_CheckUpdates_clicked();
}

void OrionLauncherWindow::closeEvent(QCloseEvent *event)
{
    saveServerList();
    saveProxyList();
    if (m_ChangelogForm != nullptr)
        m_ChangelogForm->close();

    event->accept();
}

void OrionLauncherWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    if (event->key() == Qt::Key_Delete)
    {
        QWidget *focused = QApplication::focusWidget();

        if (focused == ui->lw_ServerList) // Servers list
            on_pb_ServerRemove_clicked();
        else if (focused == ui->lw_ProxyList) // Proxy list
            on_pb_ProxyRemove_clicked();
    }

    event->accept();
}

void OrionLauncherWindow::on_lw_ServerList_clicked(const QModelIndex &index)
{
    updateServerFields(index.row());
}

void OrionLauncherWindow::updateServerFields(const int &index)
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->item(index));
    if (item != nullptr)
    {
        ui->le_ServerName->setText(item->text());
        ui->le_ServerAddress->setText(item->GetAddress());
        ui->le_ServerAccount->setText(item->GetAccount());
        ui->le_ServerPassword->setText(item->GetPassword());
        ui->le_ServerCharacter->setText(item->GetCharacter());
        ui->le_CommandLine->setText(item->GetCommand());

        ui->le_ServerClientVersion->setText(item->GetClientVersion());
        ui->le_ServerClientPath->setText(item->GetClientPath());
        ui->cb_ServerClientType->setCurrentIndex(item->GetClientType());
        ui->cb_ServerUseCrypt->setChecked(item->GetUseCrypt());

        ui->cb_LaunchAutologin->setChecked(item->GetOptionAutologin());
        ui->cb_LaunchSavePassword->setChecked(item->GetOptionSavePassword());
        ui->cb_LaunchSaveAero->setChecked(item->GetOptionSaveAero());
        ui->cb_LaunchFastLogin->setChecked(item->GetOptionFastLogin());
        ui->cb_LaunchRunUOAM->setChecked(item->GetOptionRunUOAM());

        ui->cb_ServerUseProxy->setChecked(item->GetUseProxy());
        ui->cb_ServerProxy->setCurrentText(item->GetProxy());
    }
}

void OrionLauncherWindow::on_lw_ServerList_doubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);

    on_pb_Launch_clicked();
}

void OrionLauncherWindow::on_cb_ServerShowPassword_clicked()
{
    if (ui->cb_ServerShowPassword->isChecked())
        ui->le_ServerPassword->setEchoMode(QLineEdit::EchoMode::Normal);
    else
        ui->le_ServerPassword->setEchoMode(QLineEdit::EchoMode::Password);
}

void OrionLauncherWindow::on_pb_ServerAdd_clicked()
{
    QString name = ui->le_ServerName->text().toLower();

    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the server name!");
        return;
    }

    for (int i = 0; i < ui->lw_ServerList->count(); i++)
    {
        auto item = ui->lw_ServerList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            QMessageBox::critical(this, "Name is already exists", "Server name is already exists!");
            return;
        }
    }

    auto item = new CServerListItem(
        ui->le_ServerName->text(),
        ui->le_ServerAddress->text(),
        ui->le_ServerAccount->text(),
        ui->le_ServerPassword->text(),
        ui->le_ServerCharacter->text());
    item->SetUseProxy(ui->cb_ServerUseProxy->isChecked());
    item->SetProxy(ui->cb_ServerProxy->currentText());

    ui->lw_ServerList->addItem(item);
    ui->lw_ServerList->setCurrentRow(ui->lw_ServerList->count() - 1);
    saveServerList();

    updateServerFields(ui->lw_ServerList->count() - 1);
}

void OrionLauncherWindow::on_pb_ServerSave_clicked()
{
    QString name = ui->le_ServerName->text().toLower();

    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the server name!");
        return;
    }

    auto selected = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());

    if (selected == nullptr)
    {
        QMessageBox::critical(this, "No selected item", "No selected server!");
        return;
    }

    for (int i = 0; i < ui->lw_ServerList->count(); i++)
    {
        auto item = ui->lw_ServerList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            if (item != nullptr && item != selected)
            {
                QMessageBox::critical(
                    this,
                    "Name is already exists",
                    "Server name is already exists (not this item)!");
                return;
            }

            break;
        }
    }

    selected->setText(ui->le_ServerName->text());
    selected->SetAddress(ui->le_ServerAddress->text());
    selected->SetAccount(ui->le_ServerAccount->text());
    selected->SetCharacter(ui->le_ServerCharacter->text());
    selected->SetPassword(ui->le_ServerPassword->text());
    selected->SetClientVersion(ui->le_ServerClientVersion->text());
    selected->SetClientPath(ui->le_ServerClientPath->text());
    selected->SetClientType(ui->cb_ServerClientType->currentIndex());
    selected->SetProxy(ui->cb_ServerProxy->currentText());
    selected->SetUseProxy(ui->cb_ServerUseProxy->isChecked());
    selected->SetUseCrypt(ui->cb_ServerUseCrypt->isChecked());

    saveServerList();
}

void OrionLauncherWindow::on_pb_ServerRemove_clicked()
{
    QListWidgetItem *item = ui->lw_ServerList->currentItem();

    if (item != nullptr)
    {
        item = ui->lw_ServerList->takeItem(ui->lw_ServerList->row(item));

        if (item != nullptr)
        {
            delete item;

            saveServerList();
        }
    }
}

void OrionLauncherWindow::on_le_CommandLine_textChanged(const QString &arg1)
{
    auto selected = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());

    if (selected != nullptr)
        selected->SetCommand(arg1);
}

void OrionLauncherWindow::on_lw_ProxyList_clicked(const QModelIndex &index)
{
    auto item = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(index.row()));
    if (item != nullptr)
    {
        ui->le_ProxyName->setText(item->text());
        ui->le_ProxyAddress->setText(item->GetAddress());
        ui->le_ProxyPort->setText(item->GetProxyPort());
        ui->gb_ProxySocks5->setChecked(item->GetSocks5());
        ui->le_ProxyAccount->setText(item->GetAccount());
        ui->le_ProxyPassword->setText(item->GetPassword());
    }
}

void OrionLauncherWindow::on_cb_ProxyShowPassword_clicked()
{
    if (ui->cb_ProxyShowPassword->isChecked())
        ui->le_ProxyPassword->setEchoMode(QLineEdit::EchoMode::Normal);
    else
        ui->le_ProxyPassword->setEchoMode(QLineEdit::EchoMode::Password);
}

void OrionLauncherWindow::on_pb_ProxyAdd_clicked()
{
    auto name = ui->le_ProxyName->text().toLower();
    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the proxy server name!");
        return;
    }

    for (int i = 0; i < ui->lw_ProxyList->count(); i++)
    {
        auto item = ui->lw_ProxyList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            QMessageBox::critical(
                this, "Name is already exists", "Proxy server name is already exists!");
            return;
        }
    }

    ui->lw_ProxyList->addItem(new CProxyListItem(
        ui->le_ProxyName->text(),
        ui->le_ProxyAddress->text(),
        ui->le_ProxyPort->text(),
        ui->gb_ProxySocks5->isChecked(),
        ui->le_ProxyAccount->text(),
        ui->le_ProxyPassword->text()));

    ui->lw_ProxyList->setCurrentRow(ui->lw_ProxyList->count() - 1);

    saveProxyList();

    ui->cb_ServerProxy->addItem(ui->le_ProxyName->text());
}

void OrionLauncherWindow::on_pb_ProxySave_clicked()
{
    QString name = ui->le_ProxyName->text().toLower();

    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the proxy server name!");
        return;
    }

    auto selected = static_cast<CProxyListItem *>(ui->lw_ProxyList->currentItem());

    if (selected == nullptr)
    {
        QMessageBox::critical(this, "No selected item", "No selected proxy!");
        return;
    }

    for (int i = 0; i < ui->lw_ProxyList->count(); i++)
    {
        auto item = ui->lw_ProxyList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            if (item != nullptr && item != selected)
            {
                QMessageBox::critical(
                    this,
                    "Name is already exists",
                    "Proxy server name is already exists (not this item)!");
                return;
            }

            break;
        }
    }

    selected->setText(ui->le_ProxyName->text());
    selected->SetAddress(ui->le_ProxyAddress->text());
    selected->SetProxyPort(ui->le_ProxyPort->text());
    selected->SetSocks5(ui->gb_ProxySocks5->isChecked());
    selected->SetAccount(ui->le_ProxyAccount->text());
    selected->SetPassword(ui->le_ProxyPassword->text());
    saveProxyList();

    ui->cb_ServerProxy->setItemText(ui->lw_ProxyList->currentRow(), ui->le_ProxyName->text());
}

void OrionLauncherWindow::on_pb_ProxyRemove_clicked()
{
    auto item = ui->lw_ProxyList->currentItem();
    if (item != nullptr)
    {
        int index = ui->lw_ProxyList->row(item);
        item = ui->lw_ProxyList->takeItem(index);

        if (item != nullptr)
        {
            ui->cb_ServerProxy->removeItem(index);
            delete item;
            saveProxyList();
        }
    }
}

QString OrionLauncherWindow::boolToText(const bool &value)
{
    return value ? "true" : "false";
}

bool OrionLauncherWindow::rawStringToBool(QString value)
{
    value = value.toLower();
    bool result = false;

    if (value == "true" || value == "on")
        result = true;
    else
        result = (value.toInt() != 0);

    return result;
}

void OrionLauncherWindow::writeCfg()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item == nullptr)
    {
        return;
    }

    QFile file(QDir::currentPath() + "/OrionUO.cfg");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&file);
        stream << "AcctID=" << item->GetAccount() << endl;
        if (item->GetOptionSavePassword())
        {
            stream << "AcctPassword=" << item->GetPassword() << endl;
            stream << "RememberActPW=yes" << endl;
        }
        stream << "AutoLogin=" << (item->GetOptionFastLogin() ? "yes" : "no") << endl;
        stream << "ClientType=" << item->GetClientTypeString() << endl;
        stream << "Crypt=" << (item->GetUseCrypt() ? "yes" : "no") << endl;
        stream << "ClientVersion=" << item->GetClientVersion() << endl;
        stream << "CustomPath=" << item->GetClientPath() << endl;
        if (!item->GetAddress().isEmpty())
        {
            stream << "LoginServer=" << item->GetAddress() << endl;
        }
    }
}

void OrionLauncherWindow::saveProxyList()
{
    QFile file(QDir::currentPath() + "/proxy.xml");

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QXmlStreamWriter writer(&file);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        int count = ui->lw_ProxyList->count();
        writer.writeStartElement("proxylist");
        writer.writeAttribute("version", "0");
        writer.writeAttribute("size", QString::number(count));

        for (int i = 0; i < count; i++)
        {
            auto item = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(i));
            if (item != nullptr)
            {
                writer.writeStartElement("proxy");
                writer.writeAttribute("name", item->text());
                writer.writeAttribute("address", item->GetAddress());
                writer.writeAttribute("port", item->GetProxyPort());
                writer.writeAttribute("socks5", boolToText(item->GetSocks5()));
                writer.writeAttribute("account", item->GetAccount());
                writer.writeAttribute("password", item->GetPassword());
                writer.writeEndElement(); // proxy
            }
        }
        writer.writeEndElement(); // proxylist
        writer.writeEndDocument();
        file.close();
    }
}

void OrionLauncherWindow::saveServerList()
{
    QFile file(QDir::currentPath() + "/server.xml");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QXmlStreamWriter writer(&file);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        int count = ui->lw_ServerList->count();
        writer.writeStartElement("serverlist");
        writer.writeAttribute("version", "0");
        writer.writeAttribute("size", QString::number(count));
        writer.writeAttribute(
            "closeafterlaunch", boolToText(ui->cb_LaunchCloseAfterLaunch->isChecked()));
        writer.writeAttribute("lastserver", QString::number(ui->lw_ServerList->currentRow()));
        writer.writeAttribute("checkupdates", boolToText(ui->cb_CheckUpdates->isChecked()));
        writer.writeAttribute(
            "noclientwarnings", boolToText(ui->cb_NoClientWarnings->isChecked()));

        for (int i = 0; i < ui->cb_OrionPath->count(); i++)
        {
            writer.writeStartElement("clientpath");
            writer.writeAttribute("path", ui->cb_OrionPath->itemText(i));
            writer.writeEndElement(); // clientpath
        }

        for (int i = 0; i < count; i++)
        {
            auto item = static_cast<CServerListItem *>(ui->lw_ServerList->item(i));
            if (item != nullptr)
            {
                writer.writeStartElement("server");
                writer.writeAttribute("name", item->text());
                writer.writeAttribute("address", item->GetAddress());
                writer.writeAttribute("account", item->GetAccount());
                writer.writeAttribute("password", item->GetPassword());
                writer.writeAttribute("character", item->GetCharacter());
                writer.writeAttribute("command", item->GetCommand());
                writer.writeAttribute("useproxy", boolToText(item->GetUseProxy()));
                writer.writeAttribute("proxyname", item->GetProxy());
                writer.writeAttribute("optionautologin", boolToText(item->GetOptionAutologin()));
                writer.writeAttribute(
                    "optionsavepassword", boolToText(item->GetOptionSavePassword()));
                writer.writeAttribute("optionsaveaero", boolToText(item->GetOptionSaveAero()));
                writer.writeAttribute("optionfastlogin", boolToText(item->GetOptionFastLogin()));
                writer.writeAttribute("optionrunuoam", boolToText(item->GetOptionRunUOAM()));

                writer.writeAttribute("clientversion", item->GetClientVersion());
                writer.writeAttribute("clientpath", item->GetClientPath());
                writer.writeAttribute("clienttype", item->GetClientTypeString());
                writer.writeAttribute("usecrypt", boolToText(item->GetUseCrypt()));

                writer.writeEndElement(); // server
            }
        }
        writer.writeEndElement(); // serverlist
        writer.writeEndDocument();
        file.close();
    }
}

void OrionLauncherWindow::loadProxyList()
{
    ui->lw_ProxyList->clear();

    QFile file(QDir::currentPath() + "/proxy.xml");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QXmlStreamReader reader(&file);
        int version = 0;
        int count = 0;
        while (!reader.atEnd() && !reader.hasError())
        {
            if (reader.isStartElement())
            {
                auto attributes = reader.attributes();
                if (reader.name() == "proxylist")
                {
                    if (attributes.hasAttribute("version"))
                        version = attributes.value("version").toInt();

                    if (attributes.hasAttribute("size"))
                        count = attributes.value("size").toInt();
                }
                else if (reader.name() == "proxy")
                {
                    if (attributes.hasAttribute("name"))
                    {
                        auto item = new CProxyListItem(attributes.value("name").toString());

                        if (attributes.hasAttribute("address"))
                            item->SetAddress(attributes.value("address").toString());

                        if (attributes.hasAttribute("port"))
                            item->SetProxyPort(attributes.value("port").toString());

                        if (attributes.hasAttribute("socks5"))
                            item->SetSocks5(rawStringToBool(attributes.value("socks5").toString()));

                        if (attributes.hasAttribute("account"))
                            item->SetAccount(attributes.value("account").toString());

                        if (attributes.hasAttribute("password"))
                            item->SetPassword(attributes.value("password").toString());

                        ui->lw_ProxyList->addItem(item);
                        ui->cb_ServerProxy->addItem(attributes.value("name").toString());
                    }
                }
            }
            reader.readNext();
        }
        file.close();
    }
}

void OrionLauncherWindow::loadServerList()
{
    ui->lw_ServerList->clear();

    QFile file(QDir::currentPath() + "/server.xml");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QXmlStreamReader reader(&file);

        int version = 0;
        int count = 0;
        int clientindex = -1;
        int lastServer = -1;

        ui->cb_OrionPath->clear();
        while (!reader.atEnd() && !reader.hasError())
        {
            if (reader.isStartElement())
            {
                auto attributes = reader.attributes();
                if (reader.name() == "serverlist")
                {
                    if (attributes.hasAttribute("version"))
                        version = attributes.value("version").toInt();

                    if (attributes.hasAttribute("size"))
                        count = attributes.value("size").toInt();

                    if (attributes.hasAttribute("clientindex"))
                        clientindex = attributes.value("clientindex").toInt();

                    if (attributes.hasAttribute("closeafterlaunch"))
                        ui->cb_LaunchCloseAfterLaunch->setChecked(
                            rawStringToBool(attributes.value("closeafterlaunch").toString()));

                    if (attributes.hasAttribute("lastserver"))
                        lastServer = attributes.value("lastserver").toInt();

                    if (attributes.hasAttribute("path"))
                    {
                        auto path = attributes.value("path").toString().trimmed();
                        QFileInfo fi(path);
                        ui->cb_OrionPath->addItem(fi.absoluteFilePath());
                        clientindex = 0;
                    }

                    if (attributes.hasAttribute("checkupdates"))
                        ui->cb_CheckUpdates->setChecked(
                            rawStringToBool(attributes.value("checkupdates").toString()));

                    if (attributes.hasAttribute("noclientwarnings"))
                        ui->cb_NoClientWarnings->setChecked(
                            rawStringToBool(attributes.value("noclientwarnings").toString()));
                }
                else if (reader.name() == "clientpath")
                {
                    if (attributes.hasAttribute("path"))
                    {
                        auto path = attributes.value("path").toString().trimmed();
                        QFileInfo fi(path);
                        ui->cb_OrionPath->addItem(fi.absoluteFilePath());
                        path = fi.absoluteFilePath();
                        bool found = false;
                        for (int i = 0; i < ui->cb_OrionPath->count(); i++)
                        {
                            if (path == ui->cb_OrionPath->itemText(i))
                            {
                                found = true;
                                break;
                            }
                        }

                        if (!found)
                            ui->cb_OrionPath->addItem(path);
                    }
                }
                else if (reader.name() == "server")
                {
                    if (attributes.hasAttribute("name"))
                    {
                        auto item = new CServerListItem(attributes.value("name").toString());

                        if (attributes.hasAttribute("address"))
                            item->SetAddress(attributes.value("address").toString());

                        if (attributes.hasAttribute("account"))
                            item->SetAccount(attributes.value("account").toString());

                        if (attributes.hasAttribute("password"))
                            item->SetPassword(attributes.value("password").toString());

                        if (attributes.hasAttribute("character"))
                            item->SetCharacter(attributes.value("character").toString());

                        if (attributes.hasAttribute("clientversion"))
                            item->SetClientVersion(attributes.value("clientversion").toString());

                        if (attributes.hasAttribute("clientpath"))
                            item->SetClientPath(attributes.value("clientpath").toString());

                        if (attributes.hasAttribute("clienttype"))
                            item->SetClientTypeFromString(attributes.value("clienttype").toString());

                        if (attributes.hasAttribute("command"))
                            item->SetCommand(attributes.value("command").toString());

                        if (attributes.hasAttribute("usecrypt"))
                            item->SetUseCrypt(
                                rawStringToBool(attributes.value("usecrypt").toString()));

                        if (attributes.hasAttribute("useproxy"))
                            item->SetUseProxy(
                                rawStringToBool(attributes.value("useproxy").toString()));

                        if (attributes.hasAttribute("proxyname"))
                            item->SetProxy(attributes.value("proxyname").toString());

                        if (attributes.hasAttribute("optionautologin"))
                            item->SetOptionAutologin(
                                rawStringToBool(attributes.value("optionautologin").toString()));

                        if (attributes.hasAttribute("optionsavepassword"))
                            item->SetOptionSavePassword(
                                rawStringToBool(attributes.value("optionsavepassword").toString()));

                        if (attributes.hasAttribute("optionsaveaero"))
                            item->SetOptionSaveAero(
                                rawStringToBool(attributes.value("optionsaveaero").toString()));

                        if (attributes.hasAttribute("optionfastlogin"))
                            item->SetOptionFastLogin(
                                rawStringToBool(attributes.value("optionfastlogin").toString()));

                        if (attributes.hasAttribute("optionrunuoam"))
                            item->SetOptionRunUOAM(
                                rawStringToBool(attributes.value("optionrunuoam").toString()));

                        ui->lw_ServerList->addItem(item);
                    }
                }
            }
            reader.readNext();
        }

        if (clientindex >= 0 && clientindex < ui->cb_OrionPath->count())
            ui->cb_OrionPath->setCurrentIndex(clientindex);

        if (lastServer >= 0 && lastServer < ui->lw_ServerList->count())
        {
            ui->lw_ServerList->setCurrentRow(lastServer);
            updateServerFields(lastServer);
        }
        file.close();
    }
}

void OrionLauncherWindow::on_tb_SetClientPath_clicked()
{
    QString clientPath = ui->le_ServerClientPath->text();
    if (!clientPath.length())
        clientPath = QDir::currentPath();

    QString path =
        QFileDialog::getExistingDirectory(nullptr, tr("Select UO directory"), clientPath);

    if (path.length())
    {
        ui->le_ServerClientPath->setText(path);
    }
}

void OrionLauncherWindow::on_tb_SetManifestPath_clicked()
{
    auto path = ui->le_ManifestFile->text();
    if (!path.length())
        path = QDir::currentPath();

    path = QFileDialog::getExistingDirectory(nullptr, tr("Select Cache directory"), path);
    if (path.length())
    {
        auto fullname = path + "/OrionUpdate.html";
        auto fi = QFileInfo(fullname);
        if (fi.exists())
        {
            ui->le_ManifestFile->setText(fullname);
            ui->pb_Process->setEnabled(true);
        }
        else
        {
            ui->pb_Process->setEnabled(false);
        }
    }
}

void OrionLauncherWindow::on_pb_Process_clicked()
{
    if (ui->pb_Process->isEnabled())
    {
        // FIXME: dirty work for now to get it done
        // get this properly done in the background
        ui->pb_Process->setEnabled(false);
        m_UpdateManager->writeManifest(ui->le_ManifestFile->text());
        QMessageBox msgBox;
        msgBox.setText("Done.");
        msgBox.exec();
        ui->pb_Process->setEnabled(true);
        //m_UpdateManager->generateManifestData(path);
    }
}

void OrionLauncherWindow::on_tb_SetOrionPath_clicked()
{
    QString startPath = ui->cb_OrionPath->currentText();
    if (!startPath.length())
        startPath = QDir::currentPath();

    QString path =
        QFileDialog::getExistingDirectory(nullptr, tr("Select OrionUO directory"), startPath);

    if (path.length())
    {
        for (int i = 0; i < ui->cb_OrionPath->count(); i++)
        {
            if (path == ui->cb_OrionPath->itemText(i))
            {
                ui->cb_OrionPath->setCurrentIndex(i);
                return;
            }
        }

        ui->cb_OrionPath->addItem(path);
        ui->cb_OrionPath->setCurrentIndex(ui->cb_OrionPath->count() - 1);
    }
}

QString OrionLauncherWindow::decodeArgumentString(const char *text, const int &length)
{
    QString result;
    for (int i = 0; i < length; i += 2)
    {
        char buf[3] = { text[i], text[i + 1], 0 };
        result += char(QString(buf).toInt(nullptr, 16));
    }
    return result;
}

QString OrionLauncherWindow::encodeArgumentString(const char *text, const int &length)
{
    QString result;
    for (int i = 0; i < length; i++)
    {
        QString buf;
        buf.sprintf("%02X", text[i]);
        result += buf;
    }
    return result;
}

void OrionLauncherWindow::runProgram(const QString &exePath, const QString &directory)
{
    // FIXME This will leak
    auto *process = new QProcess(this);
    process->setWorkingDirectory(directory);
    process->start(exePath);
}

void OrionLauncherWindow::on_pb_GenerateConfig_clicked()
{
    writeCfg();
}

void OrionLauncherWindow::on_pb_Launch_clicked()
{
    const auto directoryPath = ui->cb_OrionPath->currentText();
    if (!ui->lw_ServerList->count())
    {
        QMessageBox::critical(this, "Error", tr("Configuration not found."));
        ui->tw_Server->setCurrentIndex(1);
        return;
    }

    auto serverItem = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (serverItem == nullptr)
    {
        QMessageBox::critical(this, tr("Error"), tr("Please select a configuration profile!"));
        return;
    }

    if (!QFile::exists(directoryPath + "/OrionUO.cfg"))
    {
        on_pb_GenerateConfig_clicked();
        return;
    }

    auto program = ui->cb_OrionPath->currentText() + "/orionuo" EXE_EXTENSION;
    auto command = ui->le_CommandLine->text();
    if (ui->cb_LaunchFastLogin->isChecked())
        command += " -fastlogin";

    if (ui->cb_LaunchAutologin->isChecked())
        command += " -autologin";
    else
        command += " -autologin:0";

    if (ui->cb_LaunchSavePassword->isChecked())
        command += " -savepassword";
    else
        command += " -savepassword:0";

    if (ui->cb_LaunchSaveAero->isChecked())
        command += " -aero";

    command += " \"-login:" + serverItem->GetAddress() + "\"";

    QString account = serverItem->GetAccount();
    QString password = serverItem->GetPassword();
    command +=
        " -account:" + encodeArgumentString(account.toStdString().c_str(), account.length()) + "," +
        encodeArgumentString(password.toStdString().c_str(), password.length());

    QString character = serverItem->GetCharacter();
    if (character.length())
        command += " -autologinname:" +
                   encodeArgumentString(character.toStdString().c_str(), character.length());

    if (ui->cb_NoClientWarnings->isChecked())
        command += "-nowarnings";

    if (serverItem->GetUseProxy())
    {
        QString proxyName = serverItem->GetProxy().toLower();
        for (int i = 0; i < ui->lw_ProxyList->count(); i++)
        {
            auto proxy = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(i));
            if (proxy != nullptr && proxy->text().toLower() == proxyName)
            {
                command += " -proxyhost:" + proxy->GetAddress() + "," + proxy->GetProxyPort();

                if (proxy->GetSocks5())
                {
                    QString proxyAccount = proxy->GetAccount();
                    QString proxyPassword = proxy->GetPassword();
                    command += " -proxyaccount:" +
                               encodeArgumentString(
                                   proxyAccount.toStdString().c_str(), proxyAccount.length()) +
                               "," +
                               encodeArgumentString(
                                   proxyPassword.toStdString().c_str(), proxyPassword.length());
                }
                break;
            }
        }
    }

    if (command.length())
        program += " " + command;
    runProgram(program, directoryPath);

#if _WINDOWS
    if (ui->cb_LaunchRunUOAM->isChecked())
    {
        directoryPath += "/map";
        runProgram(directoryPath + "/enhancedmap" EXE_EXTENSION, directoryPath);
    }
#endif

    if (ui->cb_LaunchCloseAfterLaunch->isChecked())
    {
        saveServerList();
        saveProxyList();
        qApp->exit(0);
    }
}

void OrionLauncherWindow::on_cb_LaunchAutologin_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionAutologin(ui->cb_LaunchAutologin->isChecked());
}

void OrionLauncherWindow::on_cb_LaunchSavePassword_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionSavePassword(ui->cb_LaunchSavePassword->isChecked());
}

void OrionLauncherWindow::on_cb_LaunchSaveAero_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionSaveAero(ui->cb_LaunchSaveAero->isChecked());
}

void OrionLauncherWindow::on_cb_LaunchFastLogin_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionFastLogin(ui->cb_LaunchFastLogin->isChecked());
}

void OrionLauncherWindow::on_cb_LaunchRunUOAM_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionRunUOAM(ui->cb_LaunchRunUOAM->isChecked());
}

void OrionLauncherWindow::on_lw_OAFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateOAFeaturesCode();
}

void OrionLauncherWindow::on_lw_OAFeaturesScripts_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateOAFeaturesCode();
}

void OrionLauncherWindow::on_rb_OAFeaturesSphere_clicked()
{
    updateOAFeaturesCode();
}

void OrionLauncherWindow::on_rb_OAFeaturesRunUO_clicked()
{
    updateOAFeaturesCode();
}

void OrionLauncherWindow::on_rb_OAFeaturesPOL_clicked()
{
    updateOAFeaturesCode();
}

void OrionLauncherWindow::updateOAFeaturesCode()
{
    quint64 featuresFlags = 0;
    quint64 scriptGroupsFlags = 0;

    for (auto i = 0; i < ui->lw_OAFeaturesOptions->count(); i++)
    {
        auto item = ui->lw_OAFeaturesOptions->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            featuresFlags |= (quint64(1) << i);
    }

    for (auto i = 0; i < ui->lw_OAFeaturesScripts->count(); i++)
    {
        auto item = ui->lw_OAFeaturesScripts->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            scriptGroupsFlags |= (quint64(1) << i);
    }

    QString code;
    if (ui->rb_OAFeaturesSphere->isChecked())
    {
        code.sprintf(
            "//data for sendpacket\nB0FC W015 W0A001 D0%08X D0%08X D0%08X D0%08X",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    else if (ui->rb_OAFeaturesRunUO->isChecked())
    {
        code.sprintf(
            "public sealed class OAFeatures : Packet\n"
            "{\n"
            "public OAFeatures() : base(0xFC)\n"
            "{\n"
            "EnsureCapacity(21);\n"
            "m_Stream.Write((ushort)0xA001);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "}\n"
            "}",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    else if (ui->rb_OAFeaturesPOL->isChecked())
    {
        code.sprintf(
            "program oafeatures_sendpacket(who)\n"
            "var res := SendPacket(who, \"FC0015A001%08X%08X%08X%08X\");\n"
            "if (!res)\n"
            "print(\"SendPacket error: \" + res.errortext );\n"
            "endif\n"
            "endprogram",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    ui->pte_OAFeaturesCode->setPlainText(code);
}

void OrionLauncherWindow::on_cb_OrionPath_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    if (!m_Loading)
    {
        if (ui->cb_CheckUpdates->isChecked())
            on_pb_CheckUpdates_clicked();
    }
}

void OrionLauncherWindow::onUpdatesListReceived(const QList<CFileInfo> &list)
{
    ui->lw_AvailableUpdates->clear();
    const auto directoryPath = ui->cb_OrionPath->currentText();

    ui->pb_UpdateProgress->setValue(0);
    int n = 0;
    for (const auto &info : list)
    {
        const auto fullPath =
            (info.UODir ? directoryPath : qApp->applicationDirPath()) + "/" + info.Name;
        const auto hash = m_UpdateManager->getHash(fullPath);
        const bool wantUpdate = (info.Hash.length() && !hash.isEmpty() && info.Hash != hash);
        if (wantUpdate)
            ui->lw_AvailableUpdates->addItem(new CUpdateInfoListWidgetItem(info));
        n++;
        ui->pb_UpdateProgress->setValue(100 * n / list.size());
    }

    if (ui->lw_AvailableUpdates->count())
        ui->tw_Main->setCurrentIndex(1);

    ui->pb_CheckUpdates->setEnabled(true);
    ui->pb_ApplyUpdates->setEnabled(true);
    ui->lw_Backups->setEnabled(true);
    ui->pb_RestoreSelectedVersion->setEnabled(true);
    ui->pb_ShowChangelog->setEnabled(true);
    ui->pb_UpdateProgress->setValue(100);
}

void OrionLauncherWindow::onBackupsListReceived(const QList<CFileInfo> &list)
{
    ui->lw_Backups->clear();
    for (const auto &info : list)
        ui->lw_Backups->addItem(new CBackupInfoListWidgetItem(info));
}

void OrionLauncherWindow::on_pb_CheckUpdates_clicked()
{
    if (!ui->pb_CheckUpdates->isEnabled())
        return;

    ui->pb_CheckUpdates->setEnabled(false);
    ui->pb_ApplyUpdates->setEnabled(false);
    ui->lw_Backups->setEnabled(false);
    ui->pb_RestoreSelectedVersion->setEnabled(false);
    ui->pb_ShowChangelog->setEnabled(false);
    ui->pb_UpdateProgress->setValue(0);
    ui->lw_AvailableUpdates->clear();
    ui->lw_Backups->clear();
    m_UpdateManager->getManifest("Downloads/OrionUpdate.html");
}

void OrionLauncherWindow::onFileReceived(const QString &name)
{
    Q_UNUSED(name);

    m_FilesToUpdateCount--;
    if (m_FilesToUpdateCount <= 0 || ui->lw_AvailableUpdates->count() <= 0)
    {
        ui->pb_CheckUpdates->setEnabled(true);
        ui->pb_ApplyUpdates->setEnabled(true);
        ui->lw_Backups->setEnabled(true);
        ui->pb_RestoreSelectedVersion->setEnabled(true);
        ui->pb_ShowChangelog->setEnabled(true);
        ui->pb_UpdateProgress->setValue(100);
        m_FilesToUpdateCount = 0;
#if _WINDOWS
        if (m_LauncherFoundInUpdates &&
            QFile::exists(qApp->applicationDirPath() + "/olupd" EXE_EXTENSION))
        {
            saveServerList();
            saveProxyList();
            runProgram(
                qApp->applicationDirPath() + "/olupd" EXE_EXTENSION " /orionlauncher_udate.zip",
                qApp->applicationDirPath());
            qApp->exit(0);
        }
        else
#endif
            on_pb_CheckUpdates_clicked();
    }
    else
    {
        ui->pb_UpdateProgress->setValue(
            ((ui->lw_AvailableUpdates->count() - m_FilesToUpdateCount) * 100) /
            ui->lw_AvailableUpdates->count());
    }
}

void OrionLauncherWindow::on_pb_ApplyUpdates_clicked()
{
    if (!ui->pb_CheckUpdates->isEnabled())
        return;

    if (ui->lw_AvailableUpdates->count() < 1)
    {
        ui->pb_UpdateProgress->setValue(100);
        return;
    }

    ui->pb_UpdateProgress->setValue(0);

    if (QMessageBox::question(
            this,
            "Updates notification",
            "Close all OrionUO instances and press 'Yes'.\nPress "
            "'No' for cancel.") != QMessageBox::Yes)
        return;

    ui->pb_CheckUpdates->setEnabled(false);
    ui->pb_ApplyUpdates->setEnabled(false);
    ui->lw_Backups->setEnabled(false);
    ui->pb_RestoreSelectedVersion->setEnabled(false);
    ui->pb_ShowChangelog->setEnabled(false);

    const auto directoryPath = ui->cb_OrionPath->currentText();
    m_LauncherFoundInUpdates = false;
    m_FilesToUpdateCount = 0;

    QList<CUpdateInfoListWidgetItem *> updateList;
    for (int i = 0; i < ui->lw_AvailableUpdates->count(); i++)
    {
        auto item = static_cast<CUpdateInfoListWidgetItem *>(ui->lw_AvailableUpdates->item(i));
        if (item == nullptr)
            continue;

        m_FilesToUpdateCount++;
        updateList.push_back(item);
    }

    if (m_FilesToUpdateCount)
    {
        for (auto item : updateList)
        {
            auto path = directoryPath;
            if (!item->m_Info.UODir)
            {
                if (item->text() == "orionlauncher" EXE_EXTENSION)
                {
                    m_LauncherFoundInUpdates = true;
                }
                path = qApp->applicationDirPath();
            }

            ui->pb_UpdateProgress->setValue(0);
            const auto src = item->m_Info.ZipFileName;
            const auto dst = ui->cb_OrionPath->currentText();
            auto cb = [this, dst](const QString &f) {
                unzipPackage(f, dst);
                onFileReceived(f);
            };
            const bool silent = true;
            m_UpdateManager->getFile("Downloads", src, cb, this, silent);
        }
    }
    else
    {
        ui->pb_CheckUpdates->setEnabled(true);
        ui->pb_ApplyUpdates->setEnabled(true);
        ui->lw_Backups->setEnabled(true);
        ui->pb_RestoreSelectedVersion->setEnabled(true);
        ui->pb_ShowChangelog->setEnabled(true);
        ui->pb_UpdateProgress->setValue(100);
    }
}

void OrionLauncherWindow::on_pb_RestoreSelectedVersion_clicked()
{
    const auto item = static_cast<CBackupInfoListWidgetItem *>(ui->lw_Backups->currentItem());
    if (item == nullptr)
        return;

    ui->pb_UpdateProgress->setValue(0);
    const auto src = item->m_Backup.ZipFileName;
    const auto dst = ui->cb_OrionPath->currentText();
    auto cb = [this, dst](const QString &f) {
        unzipPackage(f, dst);
        onFileReceived(f);
    };
    m_UpdateManager->getFile("Downloads", src, cb, this);
}

void OrionLauncherWindow::unzipPackage(const QString &filename, const QString &toPath)
{
    QZipReader zipReader{ filename };
    QDir dir{ toPath };
    const auto path = dir.canonicalPath();
    if (!zipReader.extractAll(path))
        qDebug() << "Failed to unpack file:" << filename;
    zipReader.close();
}

void OrionLauncherWindow::onDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    ui->pb_UpdateProgress->setValue(100 * int(bytesRead / float(totalBytes)));
}

void OrionLauncherWindow::on_pb_ShowChangelog_clicked()
{
    assert(m_ChangelogForm != nullptr);

    if (m_ChangelogForm->isVisible())
        m_ChangelogForm->activateWindow();
    else
        m_ChangelogForm->show();

    emit m_ChangelogForm->changelogReceived("Loading...");

    m_UpdateManager->getChangelog("Downloads/OrionChangelogEN.html");
}

void OrionLauncherWindow::on_lw_Backups_doubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    on_pb_RestoreSelectedVersion_clicked();
}

void OrionLauncherWindow::on_lw_OrionFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateOrionFeaturesCode();
}

void OrionLauncherWindow::on_rb_OrionFeaturesSphere_clicked()
{
    updateOrionFeaturesCode();
}

void OrionLauncherWindow::on_rb_OrionFeaturesRunUO_clicked()
{
    updateOrionFeaturesCode();
}

void OrionLauncherWindow::on_rb_OrionFeaturesPOL_clicked()
{
    updateOrionFeaturesCode();
}

void OrionLauncherWindow::updateOrionFeaturesCode()
{
    uint featuresFlags = 0;

    for (int i = 0; i < ui->lw_OrionFeaturesOptions->count(); i++)
    {
        const auto item = ui->lw_OrionFeaturesOptions->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            featuresFlags |= (1 << i);
    }

    QString code;
    if (ui->rb_OrionFeaturesSphere->isChecked())
    {
        code.sprintf("//data for sendpacket\nB0FC W0009 W0032 D0%08X", featuresFlags);
    }
    else if (ui->rb_OrionFeaturesRunUO->isChecked())
    {
        code.sprintf(
            "public sealed class OAFeatures : Packet\n"
            "{\n"
            "public OAFeatures() : base(0xFC)\n"
            "{\n"
            "EnsureCapacity(9);\n"
            "m_Stream.Write((ushort)0x0032);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "}\n"
            "}",
            featuresFlags);
    }
    else if (ui->rb_OrionFeaturesPOL->isChecked())
    {
        code.sprintf(
            "program oafeatures_sendpacket(who)\n"
            "var res := SendPacket(who, \"FC00090032%08X\");\n"
            "if (!res)\n"
            "print(\"SendPacket error: \" + res.errortext );\n"
            "endif\n"
            "endprogram",
            featuresFlags);
    }

    ui->pte_OrionFeaturesCode->setPlainText(code);
}
