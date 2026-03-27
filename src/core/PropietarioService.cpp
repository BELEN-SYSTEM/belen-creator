#include "PropietarioService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

PropietarioService::PropietarioService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void PropietarioService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("propietario"), QStringLiteral("select=id,nombre&order=nombre.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Propietario> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            Propietario p;
            p.id = o.value(QStringLiteral("id")).toInt();
            p.nombre = o.value(QStringLiteral("nombre")).toString();
            out.append(p);
        }
        emit dataReady(out);
    });
}

void PropietarioService::create(const QString& nombre)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("user_id")] = m_client->accessToken().isEmpty() ? QString() :
        QString(); // user_id set by default via auth.uid() in PostgREST
    QNetworkReply* reply = m_client->post(QStringLiteral("propietario"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void PropietarioService::update(int id, const QString& nombre)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    QNetworkReply* reply = m_client->patch(
        QStringLiteral("propietario"),
        QStringLiteral("id=eq.%1").arg(id), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void PropietarioService::remove(int id)
{
    QNetworkReply* reply = m_client->del(
        QStringLiteral("propietario"), QStringLiteral("id=eq.%1").arg(id));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        emit mutationDone();
    });
}
