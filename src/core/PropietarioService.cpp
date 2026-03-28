#include "PropietarioService.h"
#include "SupabaseClient.h"
#include "HistorialService.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

PropietarioService::PropietarioService(SupabaseClient* client, HistorialService* historial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_historial(historial)
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
    QNetworkReply* reply = m_client->post(QStringLiteral("propietario"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nombre]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        if (m_historial && !arr.isEmpty()) {
            m_historial->registrar(QStringLiteral("propietario"), QJsonValue(),
                                   QJsonValue(arr.first().toObject()));
        }
        emit mutationDone();
    });
}

void PropietarioService::update(int id, const QString& nombre)
{
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("propietario"),
        QStringLiteral("select=id,nombre&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id, nombre]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QJsonObject body;
        body[QStringLiteral("nombre")] = nombre;
        QNetworkReply* reply = m_client->patch(
            QStringLiteral("propietario"),
            QStringLiteral("id=eq.%1").arg(id), body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, nombre, antes]() {
            reply->deleteLater();
            bool ok = false;
            QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
            if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
            if (m_historial) {
                QJsonObject despues = !arr.isEmpty() ? arr.first().toObject() : antes;
                despues[QStringLiteral("nombre")] = nombre;
                m_historial->registrar(QStringLiteral("propietario"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue(despues));
            }
            emit mutationDone();
        });
    });
}

void PropietarioService::remove(int id)
{
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("propietario"),
        QStringLiteral("select=id,nombre&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QNetworkReply* reply = m_client->del(
            QStringLiteral("propietario"), QStringLiteral("id=eq.%1").arg(id));
        connect(reply, &QNetworkReply::finished, this, [this, reply, antes, id]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(SupabaseClient::errorString(reply));
                return;
            }
            if (m_historial) {
                m_historial->registrar(QStringLiteral("propietario"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue());
            }
            emit mutationDone();
        });
    });
}
