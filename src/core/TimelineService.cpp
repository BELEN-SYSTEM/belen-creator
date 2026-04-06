#include "TimelineService.h"
#include "SupabaseClient.h"
#include "HistorialService.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>

namespace {

constexpr double kDefaultLongitudCm = 300.0;
constexpr double kDefaultAnchuraCm = 200.0;
constexpr double kMinBoardCm = 1.0;
constexpr double kMaxBoardCm = 50000.0;

double jsonParamDouble(const QJsonObject& o, const QString& key, double defaultVal)
{
    const QJsonValue v = o.value(key);
    if (v.isNull() || v.isUndefined())
        return defaultVal;
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok ? d : defaultVal;
    }
    return defaultVal;
}

void timelineFromRow(const QJsonObject& o, Timeline& t)
{
    t.id = o.value(QStringLiteral("id")).toInt();
    t.name = o.value(QStringLiteral("name")).toString();
    const QJsonObject params = o.value(QStringLiteral("params")).toObject();
    t.modo = params.value(QStringLiteral("modo")).toString();
    const QJsonValue ciclosV = params.value(QStringLiteral("ciclos"));
    if (ciclosV.isDouble())
        t.ciclos = static_cast<int>(ciclosV.toDouble());
    else
        t.ciclos = ciclosV.toInt();

    const QJsonObject dim = params.value(QStringLiteral("dimensiones")).toObject();
    double L = jsonParamDouble(dim, QStringLiteral("longitud"), 0);
    double W = jsonParamDouble(dim, QStringLiteral("anchura"), 0);
    if (L <= 0 || W <= 0) {
        L = kDefaultLongitudCm;
        W = kDefaultAnchuraCm;
    }
    t.dimensionesLongitudCm = qBound(kMinBoardCm, L, kMaxBoardCm);
    t.dimensionesAnchuraCm = qBound(kMinBoardCm, W, kMaxBoardCm);
}

double clampBoardCm(double v, double defaultVal)
{
    if (v <= 0)
        return defaultVal;
    return qBound(kMinBoardCm, v, kMaxBoardCm);
}

QJsonObject mergeParamsWithDimensiones(QJsonObject paramsObj, double longitudCm, double anchuraCm)
{
    QJsonObject dim;
    dim[QStringLiteral("longitud")] = longitudCm;
    dim[QStringLiteral("anchura")] = anchuraCm;
    paramsObj[QStringLiteral("dimensiones")] = dim;
    paramsObj.remove(QStringLiteral("canvas_superior_ancho"));
    paramsObj.remove(QStringLiteral("canvas_superior_alto"));
    return paramsObj;
}

} // namespace

TimelineService::TimelineService(SupabaseClient* client, HistorialService* historial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_historial(historial)
{
}

void TimelineService::fetchAll()
{
    QNetworkReply* reply = m_client->get(
        QStringLiteral("timeline"), QStringLiteral("select=id,name,params&order=name.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        QVector<Timeline> out;
        for (const QJsonValue& v : arr) {
            Timeline t;
            timelineFromRow(v.toObject(), t);
            out.append(t);
        }
        emit dataReady(out);
    });
}

void TimelineService::create(const QString& name, double longitudCm, double anchuraCm)
{
    const double L = clampBoardCm(longitudCm, kDefaultLongitudCm);
    const double W = clampBoardCm(anchuraCm, kDefaultAnchuraCm);
    QJsonObject params = mergeParamsWithDimensiones(QJsonObject(), L, W);
    QJsonObject body;
    body[QStringLiteral("name")] = name;
    body[QStringLiteral("params")] = params;
    QNetworkReply* reply = m_client->post(QStringLiteral("timeline"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
        if (m_historial && !arr.isEmpty()) {
            m_historial->registrar(QStringLiteral("timeline"), QJsonValue(),
                                   QJsonValue(arr.first().toObject()));
        }
        emit mutationDone();
    });
}

void TimelineService::update(int id, const QString& name, double longitudCm, double anchuraCm)
{
    const double L = clampBoardCm(longitudCm, kDefaultLongitudCm);
    const double W = clampBoardCm(anchuraCm, kDefaultAnchuraCm);
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("timeline"),
        QStringLiteral("select=id,name,params&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id, name, L, W]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QJsonObject paramsObj = antes.value(QStringLiteral("params")).toObject();
        paramsObj = mergeParamsWithDimensiones(paramsObj, L, W);
        QJsonObject body;
        body[QStringLiteral("name")] = name;
        body[QStringLiteral("params")] = paramsObj;
        QNetworkReply* reply = m_client->patch(
            QStringLiteral("timeline"),
            QStringLiteral("id=eq.%1").arg(id), body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, name, antes, L, W]() {
            reply->deleteLater();
            bool ok = false;
            QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
            if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }
            if (m_historial) {
                QJsonObject despues = !arr.isEmpty() ? arr.first().toObject() : antes;
                despues[QStringLiteral("name")] = name;
                QJsonObject p = despues.value(QStringLiteral("params")).toObject();
                p = mergeParamsWithDimensiones(p, L, W);
                despues[QStringLiteral("params")] = p;
                m_historial->registrar(QStringLiteral("timeline"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue(despues));
            }
            emit mutationDone();
        });
    });
}

void TimelineService::remove(int id)
{
    QNetworkReply* getReply = m_client->get(
        QStringLiteral("timeline"),
        QStringLiteral("select=id,name,params&id=eq.%1").arg(id));
    connect(getReply, &QNetworkReply::finished, this, [this, getReply, id]() {
        getReply->deleteLater();
        bool getOk = false;
        QJsonArray prevArr = SupabaseClient::parseArrayReply(getReply, getOk);
        QJsonObject antes = (getOk && !prevArr.isEmpty()) ? prevArr.first().toObject() : QJsonObject{};

        QNetworkReply* reply = m_client->del(
            QStringLiteral("timeline"), QStringLiteral("id=eq.%1").arg(id));
        connect(reply, &QNetworkReply::finished, this, [this, reply, antes]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(SupabaseClient::errorString(reply));
                return;
            }
            if (m_historial) {
                m_historial->registrar(QStringLiteral("timeline"),
                                       antes.isEmpty() ? QJsonValue() : QJsonValue(antes),
                                       QJsonValue());
            }
            emit mutationDone();
        });
    });
}
