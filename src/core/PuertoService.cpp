#include "PuertoService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

PuertoService::PuertoService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void PuertoService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("puerto"),
        QStringLiteral("select=id,nombre,tipo_id,pin,params,tipo:tipo_id(nombre)&order=nombre.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Puerto> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            Puerto p;
            p.id = o.value(QStringLiteral("id")).toInt();
            p.nombre = o.value(QStringLiteral("nombre")).toString();
            p.tipoId = o.value(QStringLiteral("tipo_id")).toInt();
            p.pin = o.value(QStringLiteral("pin")).toInt();
            p.params = o.value(QStringLiteral("params")).toString();
            QJsonObject tipoObj = o.value(QStringLiteral("tipo")).toObject();
            p.tipoNombre = tipoObj.value(QStringLiteral("nombre")).toString();
            out.append(p);
        }
        emit dataReady(out);
    });
}

void PuertoService::create(const QString& nombre, int tipoId, int pin, const QString& params)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("tipo_id")] = tipoId;
    body[QStringLiteral("pin")] = pin;
    body[QStringLiteral("params")] = params.isEmpty() ? QStringLiteral("{}") : params;
    QNetworkReply* reply = m_client->post(QStringLiteral("puerto"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void PuertoService::update(int id, const QString& nombre, int tipoId, int pin, const QString& params)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("tipo_id")] = tipoId;
    body[QStringLiteral("pin")] = pin;
    body[QStringLiteral("params")] = params.isEmpty() ? QStringLiteral("{}") : params;
    QNetworkReply* reply = m_client->patch(
        QStringLiteral("puerto"), QStringLiteral("id=eq.%1").arg(id), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void PuertoService::remove(int id)
{
    QNetworkReply* reply = m_client->del(
        QStringLiteral("puerto"), QStringLiteral("id=eq.%1").arg(id));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        emit mutationDone();
    });
}
