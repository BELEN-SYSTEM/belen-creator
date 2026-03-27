#include "TipoService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

TipoService::TipoService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void TipoService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("tipo"),
        QStringLiteral("select=id,nombre,acciones,params&order=nombre.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Tipo> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            Tipo t;
            t.id = o.value(QStringLiteral("id")).toInt();
            t.nombre = o.value(QStringLiteral("nombre")).toString();
            t.acciones = o.value(QStringLiteral("acciones")).toString();
            t.params = o.value(QStringLiteral("params")).toString();
            out.append(t);
        }
        emit dataReady(out);
    });
}

void TipoService::create(const QString& nombre, const QString& acciones, const QString& params)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("acciones")] = acciones;
    body[QStringLiteral("params")] = params;
    QNetworkReply* reply = m_client->post(QStringLiteral("tipo"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void TipoService::update(int id, const QString& nombre, const QString& acciones, const QString& params)
{
    QJsonObject body;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("acciones")] = acciones;
    body[QStringLiteral("params")] = params;
    QNetworkReply* reply = m_client->patch(
        QStringLiteral("tipo"), QStringLiteral("id=eq.%1").arg(id), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        emit mutationDone();
    });
}

void TipoService::remove(int id)
{
    QNetworkReply* reply = m_client->del(
        QStringLiteral("tipo"), QStringLiteral("id=eq.%1").arg(id));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        emit mutationDone();
    });
}
