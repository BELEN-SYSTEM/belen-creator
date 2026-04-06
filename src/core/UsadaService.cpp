#include "UsadaService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDate>
#include <QLocale>
#include <QSet>
#include <QVariantMap>

UsadaService::UsadaService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

static QString piezaTooltipLines(const UsadaService* self, const QJsonObject& pieza, int piezaId)
{
    QStringList lines;
    QString nombre = pieza.value(QStringLiteral("nombre")).toString().trimmed();
    if (nombre.isEmpty())
        nombre = QStringLiteral("#%1").arg(piezaId);
    lines.append(nombre);
    const QString codigo = pieza.value(QStringLiteral("codigo")).toString().trimmed();
    if (!codigo.isEmpty())
        lines.append(self->tr("C\u00f3digo: %1").arg(codigo));
    const QString notas = pieza.value(QStringLiteral("notas")).toString().trimmed();
    if (!notas.isEmpty())
        lines.append(self->tr("Notas: %1").arg(notas));
    const double precio = pieza.value(QStringLiteral("precio")).toDouble();
    if (precio > 0.0001 || pieza.contains(QStringLiteral("precio")))
        lines.append(self->tr("Precio: %1").arg(precio, 0, 'f', 2));
    const QString fechaStr = pieza.value(QStringLiteral("fecha")).toString();
    if (!fechaStr.isEmpty()) {
        const QDate d = QDate::fromString(fechaStr, Qt::ISODate);
        if (d.isValid())
            lines.append(self->tr("Fecha: %1").arg(QLocale().toString(d, QLocale::ShortFormat)));
    }
    const QString prop = pieza.value(QStringLiteral("propietario")).toObject().value(QStringLiteral("nombre")).toString().trimmed();
    if (!prop.isEmpty())
        lines.append(self->tr("Propietario: %1").arg(prop));
    const QString ubic = pieza.value(QStringLiteral("ubicacion")).toObject().value(QStringLiteral("nombre")).toString().trimmed();
    if (!ubic.isEmpty())
        lines.append(self->tr("Ubicaci\u00f3n: %1").arg(ubic));
    if (lines.size() <= 1 && pieza.isEmpty())
        lines.append(self->tr("Sin datos adicionales."));
    return lines.join(QLatin1Char('\n'));
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

void UsadaService::fetchPiezasNombresPorTimeline(int timelineId)
{
    if (timelineId <= 0) {
        emit piezasUsadasPorTimelineReady(timelineId, {}, {}, {});
        return;
    }
    const QString query = QStringLiteral(
        "select=id,pieza_id,pieza:pieza_id(nombre,codigo,notas,precio,fecha,"
        "propietario:propietario_id(nombre),ubicacion:ubicacion_id(nombre))"
        "&timeline_id=eq.%1&order=id.asc")
        .arg(timelineId);
    QNetworkReply* reply = m_client->get(QStringLiteral("usada"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, timelineId]() {
        reply->deleteLater();
        bool ok = false;
        const QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            emit piezasUsadasPorTimelineReady(timelineId, {}, {}, {});
            return;
        }
        QSet<int> seen;
        QStringList nombres;
        QVector<int> piezaIds;
        QStringList tooltips;
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            const QJsonValue pidV = o.value(QStringLiteral("pieza_id"));
            const int piezaId = pidV.isDouble() ? static_cast<int>(pidV.toDouble()) : pidV.toInt();
            if (piezaId <= 0 || seen.contains(piezaId))
                continue;
            const QJsonObject pieza = o.value(QStringLiteral("pieza")).toObject();
            QString nombre = pieza.value(QStringLiteral("nombre")).toString().trimmed();
            if (nombre.isEmpty())
                nombre = QStringLiteral("#%1").arg(piezaId);
            seen.insert(piezaId);
            nombres.append(nombre);
            piezaIds.append(piezaId);
            tooltips.append(piezaTooltipLines(this, pieza, piezaId));
        }
        emit piezasUsadasPorTimelineReady(timelineId, nombres, piezaIds, tooltips);
    });
}

static bool parsePosicionParams(const QString& paramsStr, double* xOut, double* yOut)
{
    if (!xOut || !yOut)
        return false;
    QJsonParseError err{};
    QJsonDocument d = QJsonDocument::fromJson(paramsStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !d.isObject())
        return false;
    const QJsonObject root = d.object();
    const QJsonObject pos = root.value(QStringLiteral("posicion")).toObject();
    if (pos.isEmpty())
        return false;
    const QJsonValue xv = pos.value(QStringLiteral("x"));
    const QJsonValue yv = pos.value(QStringLiteral("y"));
    if (!xv.isDouble() && !xv.isString())
        return false;
    if (!yv.isDouble() && !yv.isString())
        return false;
    bool okx = false;
    bool oky = false;
    *xOut = xv.isDouble() ? xv.toDouble() : xv.toString().toDouble(&okx);
    *yOut = yv.isDouble() ? yv.toDouble() : yv.toString().toDouble(&oky);
    if (xv.isString() && !okx)
        return false;
    if (yv.isString() && !oky)
        return false;
    return true;
}

void UsadaService::fetchCanvasPlacementsForTimeline(int timelineId)
{
    if (timelineId <= 0) {
        emit canvasPlacementsReady(timelineId, QVariantList());
        return;
    }
    const QString query = QStringLiteral(
        "select=id,pieza_id,params&timeline_id=eq.%1&time=is.null&order=id.asc")
        .arg(timelineId);
    QNetworkReply* reply = m_client->get(QStringLiteral("usada"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, timelineId]() {
        reply->deleteLater();
        bool ok = false;
        const QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            emit canvasPlacementsReady(timelineId, QVariantList());
            return;
        }
        QVariantList out;
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            const QJsonValue idV = o.value(QStringLiteral("id"));
            const int usadaRowId = idV.isDouble() ? static_cast<int>(idV.toDouble()) : idV.toInt();
            const QJsonValue pidV = o.value(QStringLiteral("pieza_id"));
            const int piezaId = pidV.isDouble() ? static_cast<int>(pidV.toDouble()) : pidV.toInt();
            if (piezaId <= 0 || usadaRowId <= 0)
                continue;
            const QJsonValue pv = o.value(QStringLiteral("params"));
            QString paramsStr;
            if (pv.isString())
                paramsStr = pv.toString();
            else if (pv.isObject())
                paramsStr = QString::fromUtf8(QJsonDocument(pv.toObject()).toJson(QJsonDocument::Compact));
            double x = 0;
            double y = 0;
            if (!parsePosicionParams(paramsStr, &x, &y))
                continue;
            QVariantMap row;
            row[QStringLiteral("usadaId")] = usadaRowId;
            row[QStringLiteral("piezaId")] = piezaId;
            row[QStringLiteral("xCm")] = x;
            row[QStringLiteral("yCm")] = y;
            out.append(row);
        }
        emit canvasPlacementsReady(timelineId, out);
    });
}

void UsadaService::insertCanvasPlacement(int timelineId, int piezaId, double xCm, double yCm)
{
    if (timelineId <= 0 || piezaId <= 0) {
        emit canvasPlacementInsertFinished(false, QStringLiteral("timeline o pieza inv\u00e1lidos"), 0);
        return;
    }
    QJsonObject posObj;
    posObj[QStringLiteral("x")] = xCm;
    posObj[QStringLiteral("y")] = yCm;
    QJsonObject params;
    params[QStringLiteral("posicion")] = posObj;

    QJsonObject body;
    body[QStringLiteral("time")] = QJsonValue(QJsonValue::Null);
    body[QStringLiteral("accion")] = QJsonValue(QJsonValue::Null);
    body[QStringLiteral("params")] = QString::fromUtf8(
        QJsonDocument(params).toJson(QJsonDocument::Compact));
    body[QStringLiteral("pieza_id")] = piezaId;
    body[QStringLiteral("timeline_id")] = timelineId;
    body[QStringLiteral("puerto_id")] = QJsonValue(QJsonValue::Null);

    QNetworkReply* reply = m_client->post(QStringLiteral("usada"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        const QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        const QString err = SupabaseClient::errorString(reply);
        if (ok && !arr.isEmpty()) {
            const int newId = arr.first().toObject().value(QStringLiteral("id")).toInt();
            emit canvasPlacementInsertFinished(true, QString(), newId);
        } else {
            emit canvasPlacementInsertFinished(false,
                                               err.isEmpty() ? QStringLiteral("Error desconocido") : err, 0);
        }
    });
}

void UsadaService::updateCanvasPlacementPosition(int usadaId, double xCm, double yCm)
{
    if (usadaId <= 0)
        return;
    QJsonObject posObj;
    posObj[QStringLiteral("x")] = xCm;
    posObj[QStringLiteral("y")] = yCm;
    QJsonObject params;
    params[QStringLiteral("posicion")] = posObj;
    QJsonObject body;
    body[QStringLiteral("params")] = QString::fromUtf8(
        QJsonDocument(params).toJson(QJsonDocument::Compact));

    QNetworkReply* reply = m_client->patch(QStringLiteral("usada"),
                                           QStringLiteral("id=eq.%1").arg(usadaId), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QNetworkReply::NetworkError ne = reply->error();
        const QString err = SupabaseClient::errorString(reply);
        reply->deleteLater();
        if (ne != QNetworkReply::NoError) {
            emit errorOccurred(err.isEmpty() ? QStringLiteral("No se pudo actualizar la posici\u00f3n en tablero")
                                             : err);
        }
    });
}

void UsadaService::deleteCanvasPlacement(int usadaId)
{
    if (usadaId <= 0) {
        emit canvasPlacementDeleteFinished(false, QStringLiteral("Id inv\u00e1lido"), usadaId);
        return;
    }
    QNetworkReply* reply = m_client->del(
        QStringLiteral("usada"),
        QStringLiteral("id=eq.%1&time=is.null").arg(usadaId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, usadaId]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString err = ok ? QString() : SupabaseClient::errorString(reply);
        emit canvasPlacementDeleteFinished(ok, err, usadaId);
    });
}
