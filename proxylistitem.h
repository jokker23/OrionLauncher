/***********************************************************************************
**
** ProxyListItem.h
**
** Copyright (C) October 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#pragma once

#include <QListWidgetItem>

class CProxyListItem : public QListWidgetItem
{
private:
    QString m_Address;
    QString m_ProxyPort;
    bool m_Socks5 = false;
    QString m_Account;
    QString m_Password;

public:
    CProxyListItem(const QString &name);
    CProxyListItem(
        const QString &name,
        const QString &address,
        const QString &port,
        const bool &socks5,
        const QString &account,
        const QString &password);
    virtual ~CProxyListItem() {}

    QString GetAddress() const { return m_Address; }
    void SetAddress(const QString &address) { m_Address = address; }

    QString GetProxyPort() const { return m_ProxyPort; }
    void SetProxyPort(const QString &port) { m_ProxyPort = port; }

    bool GetSocks5() const { return m_Socks5; }
    void SetSocks5(const bool &socks5) { m_Socks5 = socks5; }

    QString GetAccount() const { return m_Account; }
    void SetAccount(const QString &account) { m_Account = account; }

    QString GetPassword() const { return m_Password; }
    void SetPassword(const QString &password) { m_Password = password; }
};
