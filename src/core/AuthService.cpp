#include "AuthService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QDebug>

AuthService::AuthService(SupabaseClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

SupabaseClient* AuthService::client() const { return m_client; }

const Usuario& AuthService::currentUser() const { return m_currentUser; }
bool AuthService::isLoggedIn() const { return m_loggedIn; }

QString AuthService::localIPv4()
{
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if ((iface.flags() & QNetworkInterface::IsUp) == 0)
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            QHostAddress a = entry.ip();
            if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
                return a.toString();
        }
    }
    return {};
}

void AuthService::login(const QString& correo, const QString& contra)
{
    QNetworkReply* reply = m_client->authSignIn(correo, contra);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonObject obj = SupabaseClient::parseReply(reply, ok);
        if (!ok) {
            QString msg = obj.value(QStringLiteral("error_description")).toString();
            if (msg.isEmpty())
                msg = obj.value(QStringLiteral("msg")).toString();
            if (msg.isEmpty())
                msg = reply->errorString();
            qWarning() << "[Auth] Login failed:" << msg;
            emit loginFailed(msg);
            return;
        }

        m_client->setAccessToken(obj.value(QStringLiteral("access_token")).toString());
        m_client->setRefreshToken(obj.value(QStringLiteral("refresh_token")).toString());

        QJsonObject userObj = obj.value(QStringLiteral("user")).toObject();
        m_currentUser.id = userObj.value(QStringLiteral("id")).toString();
        m_currentUser.correo = userObj.value(QStringLiteral("email")).toString();

        fetchProfile();
    });
}

void AuthService::fetchProfile()
{
    QString query = QStringLiteral("id=eq.%1&select=nombre,permiso,ipv4").arg(m_currentUser.id);
    QNetworkReply* reply = m_client->get(QStringLiteral("usuario"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (ok && !arr.isEmpty()) {
            QJsonObject profile = arr.first().toObject();
            m_currentUser.nombre = profile.value(QStringLiteral("nombre")).toString();
            m_currentUser.permiso = profile.value(QStringLiteral("permiso")).toString();
            m_currentUser.ipv4 = profile.value(QStringLiteral("ipv4")).toString();
        }
        m_loggedIn = true;
        qDebug() << "[Auth] Login OK:" << m_currentUser.nombre << m_currentUser.permiso;
        emit loginSuccess(m_currentUser);
    });
}

void AuthService::registerUser(const QString& nombre, const QString& correo, const QString& contra)
{
    QJsonObject metadata;
    metadata[QStringLiteral("nombre")] = nombre;
    metadata[QStringLiteral("ipv4")] = localIPv4();

    QNetworkReply* reply = m_client->authSignUp(correo, contra, metadata);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonObject obj = SupabaseClient::parseReply(reply, ok);
        if (!ok) {
            QString msg = obj.value(QStringLiteral("msg")).toString();
            if (msg.isEmpty())
                msg = obj.value(QStringLiteral("error_description")).toString();
            if (msg.isEmpty())
                msg = reply->errorString();
            qWarning() << "[Auth] Register failed:" << msg;
            emit registerFailed(msg);
            return;
        }
        qDebug() << "[Auth] Registro OK";
        emit registerSuccess();
    });
}

void AuthService::logout()
{
    m_currentUser = Usuario{};
    m_loggedIn = false;
    m_client->setAccessToken({});
    m_client->setRefreshToken({});
}
