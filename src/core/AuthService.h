#pragma once

#include "../models/Usuario.h"
#include <QObject>
#include <QString>

class SupabaseClient;

class AuthService : public QObject
{
    Q_OBJECT

public:
    explicit AuthService(SupabaseClient* client, QObject* parent = nullptr);

    void login(const QString& correo, const QString& contra);
    void registerUser(const QString& nombre, const QString& correo, const QString& contra);
    void logout();

    const Usuario& currentUser() const;
    bool isLoggedIn() const;
    SupabaseClient* client() const;

    static QString localIPv4();

signals:
    void loginSuccess(const Usuario& user);
    void loginFailed(const QString& error);
    void registerSuccess();
    void registerFailed(const QString& error);

private:
    void fetchProfile();

    SupabaseClient* m_client;
    Usuario m_currentUser;
    bool m_loggedIn = false;
};
