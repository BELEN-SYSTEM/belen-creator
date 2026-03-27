#include "SupabaseClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

static const char* kDefaultUrl = "https://hpjjeoinbohzsmjhznwh.supabase.co";
static const char* kDefaultAnonKey =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imhwamplb2luYm9oenNtamh6bndoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzM1MDM0NTQsImV4cCI6MjA4OTA3OTQ1NH0."
    "KqrgtQGbyQ8FuSFsfPy_Na5eLlUml3zUmpDKzC9Zyf0";

SupabaseClient::SupabaseClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    loadConfig();
    qDebug() << "[Supabase] URL:" << m_url;
}

void SupabaseClient::loadConfig()
{
    m_url = QString::fromUtf8(qgetenv("SUPABASE_URL").trimmed());
    m_anonKey = QString::fromUtf8(qgetenv("SUPABASE_ANON_KEY").trimmed());

    if (!m_url.isEmpty() && !m_anonKey.isEmpty())
        return;

    auto tryFile = [this](const QString& path) -> bool {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')) || line.isEmpty())
                continue;
            int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) continue;
            QString key = line.left(eq).trimmed();
            QString val = line.mid(eq + 1).trimmed();
            if (key == QLatin1String("SUPABASE_URL") && m_url.isEmpty())
                m_url = val;
            else if (key == QLatin1String("SUPABASE_ANON_KEY") && m_anonKey.isEmpty())
                m_anonKey = val;
        }
        return !m_url.isEmpty() && !m_anonKey.isEmpty();
    };

    QString base = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        base + QStringLiteral("/supabase.ini"),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/supabase.ini"),
    };
    for (const QString& c : candidates) {
        if (tryFile(QDir::cleanPath(c)))
            return;
    }

    if (m_url.isEmpty())
        m_url = QString::fromUtf8(kDefaultUrl);
    if (m_anonKey.isEmpty())
        m_anonKey = QString::fromUtf8(kDefaultAnonKey);
}

QString SupabaseClient::url() const { return m_url; }
QString SupabaseClient::anonKey() const { return m_anonKey; }

void SupabaseClient::setAccessToken(const QString& token) { m_accessToken = token; }
QString SupabaseClient::accessToken() const { return m_accessToken; }
void SupabaseClient::setRefreshToken(const QString& token) { m_refreshToken = token; }
QString SupabaseClient::refreshToken() const { return m_refreshToken; }
bool SupabaseClient::isAuthenticated() const { return !m_accessToken.isEmpty(); }

// ---------------------------------------------------------------------------
// Request builders
// ---------------------------------------------------------------------------

QNetworkRequest SupabaseClient::buildRestRequest(const QString& path) const
{
    QNetworkRequest req(QUrl(m_url + QStringLiteral("/rest/v1/") + path));
    req.setRawHeader("apikey", m_anonKey.toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    req.setRawHeader("Prefer", "return=representation");
    if (!m_accessToken.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    else
        req.setRawHeader("Authorization", ("Bearer " + m_anonKey).toUtf8());
    return req;
}

QNetworkRequest SupabaseClient::buildAuthRequest(const QString& path) const
{
    QNetworkRequest req(QUrl(m_url + QStringLiteral("/auth/v1/") + path));
    req.setRawHeader("apikey", m_anonKey.toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    return req;
}

// ---------------------------------------------------------------------------
// REST primitives
// ---------------------------------------------------------------------------

QNetworkReply* SupabaseClient::get(const QString& table, const QString& queryParams)
{
    QString path = table;
    if (!queryParams.isEmpty())
        path += QLatin1Char('?') + queryParams;
    return m_nam->get(buildRestRequest(path));
}

QNetworkReply* SupabaseClient::post(const QString& table, const QJsonObject& body)
{
    return m_nam->post(buildRestRequest(table),
                       QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::post(const QString& table, const QJsonArray& body)
{
    return m_nam->post(buildRestRequest(table),
                       QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::patch(const QString& table, const QString& filter,
                                     const QJsonObject& body)
{
    QString path = table;
    if (!filter.isEmpty())
        path += QLatin1Char('?') + filter;
    return m_nam->sendCustomRequest(buildRestRequest(path), "PATCH",
                                    QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::del(const QString& table, const QString& filter)
{
    QString path = table;
    if (!filter.isEmpty())
        path += QLatin1Char('?') + filter;
    return m_nam->deleteResource(buildRestRequest(path));
}

QNetworkReply* SupabaseClient::rpc(const QString& function, const QJsonObject& args)
{
    QNetworkRequest req = buildRestRequest(QStringLiteral("rpc/") + function);
    return m_nam->post(req, QJsonDocument(args).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------------------
// Auth endpoints
// ---------------------------------------------------------------------------

QNetworkReply* SupabaseClient::authSignUp(const QString& email, const QString& password,
                                          const QJsonObject& metadata)
{
    QJsonObject body;
    body[QStringLiteral("email")] = email;
    body[QStringLiteral("password")] = password;
    if (!metadata.isEmpty())
        body[QStringLiteral("data")] = metadata;
    return m_nam->post(buildAuthRequest(QStringLiteral("signup")),
                       QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::authSignIn(const QString& email, const QString& password)
{
    QJsonObject body;
    body[QStringLiteral("email")] = email;
    body[QStringLiteral("password")] = password;
    return m_nam->post(
        buildAuthRequest(QStringLiteral("token?grant_type=password")),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::authRefresh(const QString& refreshTok)
{
    QJsonObject body;
    body[QStringLiteral("refresh_token")] = refreshTok;
    return m_nam->post(
        buildAuthRequest(QStringLiteral("token?grant_type=refresh_token")),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QNetworkReply* SupabaseClient::authGetUser()
{
    QNetworkRequest req = buildAuthRequest(QStringLiteral("user"));
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    return m_nam->get(req);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QJsonObject SupabaseClient::parseReply(QNetworkReply* reply, bool& ok)
{
    ok = false;
    if (!reply) return {};
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Supabase]" << reply->error() << data;
        return doc.isObject() ? doc.object() : QJsonObject();
    }
    ok = true;
    return doc.object();
}

QJsonArray SupabaseClient::parseArrayReply(QNetworkReply* reply, bool& ok)
{
    ok = false;
    if (!reply) return {};
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Supabase]" << reply->error() << data;
        return {};
    }
    ok = true;
    return doc.array();
}

QString SupabaseClient::errorString(QNetworkReply* reply)
{
    if (!reply) return QStringLiteral("No reply");
    if (reply->error() == QNetworkReply::NoError) return {};
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("error_description")))
            return obj.value(QStringLiteral("error_description")).toString();
        if (obj.contains(QStringLiteral("message")))
            return obj.value(QStringLiteral("message")).toString();
        if (obj.contains(QStringLiteral("msg")))
            return obj.value(QStringLiteral("msg")).toString();
    }
    return reply->errorString();
}
