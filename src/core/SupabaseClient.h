#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class SupabaseClient : public QObject
{
    Q_OBJECT

public:
    explicit SupabaseClient(QObject* parent = nullptr);

    QString url() const;
    QString anonKey() const;

    void setAccessToken(const QString& token);
    QString accessToken() const;
    void setRefreshToken(const QString& token);
    QString refreshToken() const;
    bool isAuthenticated() const;

    // REST primitives — all async, caller connects to QNetworkReply::finished
    QNetworkReply* get(const QString& table, const QString& queryParams = {});
    QNetworkReply* post(const QString& table, const QJsonObject& body);
    QNetworkReply* post(const QString& table, const QJsonArray& body);
    QNetworkReply* patch(const QString& table, const QString& filter, const QJsonObject& body);
    QNetworkReply* del(const QString& table, const QString& filter);
    QNetworkReply* rpc(const QString& function, const QJsonObject& args = {});

    // Auth endpoints
    QNetworkReply* authSignUp(const QString& email, const QString& password,
                              const QJsonObject& metadata = {});
    QNetworkReply* authSignIn(const QString& email, const QString& password);
    QNetworkReply* authRefresh(const QString& refreshToken);
    QNetworkReply* authGetUser();

    // Helpers
    static QJsonObject parseReply(QNetworkReply* reply, bool& ok);
    static QJsonArray  parseArrayReply(QNetworkReply* reply, bool& ok);
    static QString     errorString(QNetworkReply* reply);

private:
    QNetworkRequest buildRestRequest(const QString& path) const;
    QNetworkRequest buildAuthRequest(const QString& path) const;
    void loadConfig();

    QNetworkAccessManager* m_nam;
    QString m_url;
    QString m_anonKey;
    QString m_accessToken;
    QString m_refreshToken;
};
