#include "TipoService.h"
#include "SupabaseClient.h"
#include "HistorialService.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

TipoService::TipoService(SupabaseClient* client, HistorialService* historial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_historial(historial)
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, nombre]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        if (m_historial && !arr.isEmpty()) {
            m_historial->registrar(QStringLiteral("tipo"), QJsonValue(),
                                   QJsonValue(arr.first().toObject()));
        }
        emit mutationDone();
    });
}

void TipoService::update(int id, const QString& nombre, const QString& acciones, const QString& params)
{
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("tipo"),
        QStringLiteral("select=id,nombre,acciones,params&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id, nombre, acciones, params]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QJsonObject body;
        body[QStringLiteral("nombre")] = nombre;
        body[QStringLiteral("acciones")] = acciones;
        body[QStringLiteral("params")] = params;
        QNetworkReply* reply = m_client->patch(
            QStringLiteral("tipo"), QStringLiteral("id=eq.%1").arg(id), body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, nombre, acciones, params, antes]() {
            reply->deleteLater();
            bool ok = false;
            QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
            if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
            if (m_historial) {
                QJsonObject despues = !arr.isEmpty() ? arr.first().toObject() : antes;
                despues[QStringLiteral("nombre")] = nombre;
                despues[QStringLiteral("acciones")] = acciones;
                despues[QStringLiteral("params")] = params;
                m_historial->registrar(QStringLiteral("tipo"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue(despues));
            }
            emit mutationDone();
        });
    });
}

void TipoService::remove(int id)
{
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("tipo"),
        QStringLiteral("select=id,nombre,acciones,params&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QNetworkReply* reply = m_client->del(
            QStringLiteral("tipo"), QStringLiteral("id=eq.%1").arg(id));
        connect(reply, &QNetworkReply::finished, this, [this, reply, antes, id]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(SupabaseClient::errorString(reply));
                return;
            }
            if (m_historial) {
                m_historial->registrar(QStringLiteral("tipo"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue());
            }
            emit mutationDone();
        });
    });
}
