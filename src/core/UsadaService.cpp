#include "UsadaService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

UsadaService::UsadaService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void UsadaService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("usada"),
        QStringLiteral("select=id,time,accion,params,pieza_id,timeline_id,puerto_id&order=id.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Usada> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            Usada u;
            u.id = o.value(QStringLiteral("id")).toInt();
            u.time = o.value(QStringLiteral("time")).toString();
            u.accion = o.value(QStringLiteral("accion")).toString();
            u.params = o.value(QStringLiteral("params")).toString();
            u.piezaId = o.value(QStringLiteral("pieza_id")).toInt();
            u.timelineId = o.value(QStringLiteral("timeline_id")).toInt();
            u.puertoId = o.value(QStringLiteral("puerto_id")).toInt();
            out.append(u);
        }
        emit dataReady(out);
    });
}
