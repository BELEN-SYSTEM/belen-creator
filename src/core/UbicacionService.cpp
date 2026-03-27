#include "UbicacionService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

UbicacionService::UbicacionService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void UbicacionService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("ubicacion"), QStringLiteral("select=id,nombre&order=nombre.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Ubicacion> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            Ubicacion u;
            u.id = o.value(QStringLiteral("id")).toInt();
            u.nombre = o.value(QStringLiteral("nombre")).toString();
            out.append(u);
        }
        emit dataReady(out);
    });
}

void UbicacionService::create(const QString& nombre)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    QNetworkReply* reply = m_client->post(QStringLiteral("ubicacion"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void UbicacionService::update(int id, const QString& nombre)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    QNetworkReply* reply = m_client->patch(
        QStringLiteral("ubicacion"),
        QStringLiteral("id=eq.%1").arg(id), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void UbicacionService::remove(int id)
{
    QNetworkReply* reply = m_client->del(
        QStringLiteral("ubicacion"), QStringLiteral("id=eq.%1").arg(id));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        emit mutationDone();
    });
}
