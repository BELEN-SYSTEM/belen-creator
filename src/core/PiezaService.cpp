#include "PiezaService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDate>
#include <QDebug>

PiezaService::PiezaService(SupabaseClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

void PiezaService::fetchPiezasForCards(const PiezaListParams& params)
{
    QString select = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,"
        "propietario:propietario_id(nombre),"
        "ubicacion:ubicacion_id(nombre),"
        "pieza_tipo(tipo_id,tipo:tipo_id(nombre)),"
        "galeria(base64)");

    QStringList filters;
    if (!params.searchNombre.trimmed().isEmpty())
        filters.append(QStringLiteral("nombre=ilike.*%1*").arg(params.searchNombre.trimmed()));
    if (params.filterPropietarioId > 0)
        filters.append(QStringLiteral("propietario_id=eq.%1").arg(params.filterPropietarioId));
    if (params.filterUbicacionId == -1)
        filters.append(QStringLiteral("ubicacion_id=is.null"));
    else if (params.filterUbicacionId > 0)
        filters.append(QStringLiteral("ubicacion_id=eq.%1").arg(params.filterUbicacionId));

    const QStringList allowedSort = {
        QStringLiteral("codigo"), QStringLiteral("nombre"), QStringLiteral("notas"),
        QStringLiteral("fecha"), QStringLiteral("precio"),
        QStringLiteral("propietario_id"), QStringLiteral("ubicacion_id")
    };
    QString sortCol = allowedSort.contains(params.sortBy) ? params.sortBy : QStringLiteral("codigo");
    QString sortDir = (params.sortOrder == Qt::DescendingOrder) ? QStringLiteral("desc") : QStringLiteral("asc");
    QString nulls = (sortCol == QStringLiteral("fecha")) ? QStringLiteral(".nullslast") : QString();

    QString query = select;
    for (const QString& f : filters)
        query += QLatin1Char('&') + f;
    query += QStringLiteral("&order=%1.%2%3").arg(sortCol, sortDir, nulls);

    QNetworkReply* reply = m_client->get(QStringLiteral("pieza"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, params]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }

        QVector<PiezaCardData> out;
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            PiezaCardData card;
            card.pieza.id = o.value(QStringLiteral("id")).toInt();
            card.pieza.codigo = o.value(QStringLiteral("codigo")).toString();
            card.pieza.nombre = o.value(QStringLiteral("nombre")).toString();
            card.pieza.notas = o.value(QStringLiteral("notas")).toString();
            card.pieza.fecha = QDate::fromString(o.value(QStringLiteral("fecha")).toString(), Qt::ISODate);
            card.pieza.precio = o.value(QStringLiteral("precio")).toDouble();
            card.pieza.propietarioId = o.value(QStringLiteral("propietario_id")).toInt();
            card.pieza.ubicacionId = o.value(QStringLiteral("ubicacion_id")).toInt();

            QJsonObject propObj = o.value(QStringLiteral("propietario")).toObject();
            card.propietarioNombre = propObj.value(QStringLiteral("nombre")).toString();

            QJsonObject ubicObj = o.value(QStringLiteral("ubicacion")).toObject();
            card.ubicacionNombre = ubicObj.value(QStringLiteral("nombre")).toString();

            QJsonArray ptArr = o.value(QStringLiteral("pieza_tipo")).toArray();
            for (const QJsonValue& pt : ptArr) {
                QJsonObject ptObj = pt.toObject();
                card.tipoIds.append(ptObj.value(QStringLiteral("tipo_id")).toInt());
                QJsonObject tipoObj = ptObj.value(QStringLiteral("tipo")).toObject();
                card.tipos.append(tipoObj.value(QStringLiteral("nombre")).toString());
            }

            QJsonArray galArr = o.value(QStringLiteral("galeria")).toArray();
            for (const QJsonValue& g : galArr)
                card.imagenesBase64.append(g.toObject().value(QStringLiteral("base64")).toString());

            if (params.filterTipoId > 0 && !card.tipoIds.contains(params.filterTipoId))
                continue;

            out.append(card);
        }
        emit piezasFetched(out);
    });
}

void PiezaService::fetchPiezaForEdit(int id)
{
    if (id <= 0) return;
    QString query = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,"
        "pieza_tipo(tipo_id),galeria(base64)"
        "&id=eq.%1").arg(id);

    QNetworkReply* reply = m_client->get(QStringLiteral("pieza"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        PiezaCardData card;
        if (!ok || arr.isEmpty()) {
            emit piezaForEditReady(card);
            return;
        }
        QJsonObject o = arr.first().toObject();
        card.pieza.id = o.value(QStringLiteral("id")).toInt();
        card.pieza.codigo = o.value(QStringLiteral("codigo")).toString();
        card.pieza.nombre = o.value(QStringLiteral("nombre")).toString();
        card.pieza.notas = o.value(QStringLiteral("notas")).toString();
        card.pieza.fecha = QDate::fromString(o.value(QStringLiteral("fecha")).toString(), Qt::ISODate);
        card.pieza.precio = o.value(QStringLiteral("precio")).toDouble();
        card.pieza.propietarioId = o.value(QStringLiteral("propietario_id")).toInt();
        card.pieza.ubicacionId = o.value(QStringLiteral("ubicacion_id")).toInt();

        for (const QJsonValue& pt : o.value(QStringLiteral("pieza_tipo")).toArray())
            card.tipoIds.append(pt.toObject().value(QStringLiteral("tipo_id")).toInt());
        for (const QJsonValue& g : o.value(QStringLiteral("galeria")).toArray())
            card.imagenesBase64.append(g.toObject().value(QStringLiteral("base64")).toString());

        emit piezaForEditReady(card);
    });
}

void PiezaService::create(const QString& codigo, const QString& nombre, const QString& notas,
                           const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                           const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64)
{
    QJsonObject body;
    body[QStringLiteral("codigo")] = codigo;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("notas")] = notas;
    body[QStringLiteral("fecha")] = fecha.isValid() ? fecha.toString(Qt::ISODate) : QJsonValue(QJsonValue::Null);
    body[QStringLiteral("precio")] = precio;
    body[QStringLiteral("propietario_id")] = propietarioId;
    if (ubicacionId > 0)
        body[QStringLiteral("ubicacion_id")] = ubicacionId;
    else
        body[QStringLiteral("ubicacion_id")] = QJsonValue(QJsonValue::Null);

    QNetworkReply* reply = m_client->post(QStringLiteral("pieza"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tipoIds, imagenesBase64]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok || arr.isEmpty()) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        int piezaId = arr.first().toObject().value(QStringLiteral("id")).toInt();
        if (piezaId <= 0) { emit errorOccurred(QStringLiteral("No se pudo obtener id de pieza")); return; }
        saveTiposAndImages(piezaId, tipoIds, imagenesBase64);
    });
}

void PiezaService::update(int id, const QString& codigo, const QString& nombre, const QString& notas,
                           const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                           const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64)
{
    QJsonObject body;
    body[QStringLiteral("codigo")] = codigo;
    body[QStringLiteral("nombre")] = nombre;
    body[QStringLiteral("notas")] = notas;
    body[QStringLiteral("fecha")] = fecha.isValid() ? fecha.toString(Qt::ISODate) : QJsonValue(QJsonValue::Null);
    body[QStringLiteral("precio")] = precio;
    body[QStringLiteral("propietario_id")] = propietarioId;
    body[QStringLiteral("ubicacion_id")] = ubicacionId > 0 ? QJsonValue(ubicacionId) : QJsonValue(QJsonValue::Null);

    QNetworkReply* reply = m_client->patch(
        QStringLiteral("pieza"), QStringLiteral("id=eq.%1").arg(id), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, tipoIds, imagenesBase64]() {
        reply->deleteLater();
        bool ok = false;
        SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }

        // Delete old pieza_tipo and galeria, then re-insert
        QNetworkReply* delPt = m_client->del(
            QStringLiteral("pieza_tipo"), QStringLiteral("pieza_id=eq.%1").arg(id));
        connect(delPt, &QNetworkReply::finished, this, [this, delPt, id, tipoIds, imagenesBase64]() {
            delPt->deleteLater();
            QNetworkReply* delGal = m_client->del(
                QStringLiteral("galeria"), QStringLiteral("pieza_id=eq.%1").arg(id));
            connect(delGal, &QNetworkReply::finished, this, [this, delGal, id, tipoIds, imagenesBase64]() {
                delGal->deleteLater();
                saveTiposAndImages(id, tipoIds, imagenesBase64);
            });
        });
    });
}

void PiezaService::remove(int id)
{
    QNetworkReply* reply = m_client->del(
        QStringLiteral("pieza"), QStringLiteral("id=eq.%1").arg(id));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        emit mutationDone();
    });
}

void PiezaService::saveTiposAndImages(int piezaId, const QVector<int>& tipoIds,
                                       const QVector<QString>& imagenesBase64)
{
    int pending = 0;
    auto checkDone = [this, &pending]() {
        if (--pending <= 0)
            emit mutationDone();
    };

    if (!tipoIds.isEmpty()) {
        QJsonArray ptArr;
        for (int tipoId : tipoIds) {
            if (tipoId <= 0) continue;
            QJsonObject pt;
            pt[QStringLiteral("pieza_id")] = piezaId;
            pt[QStringLiteral("tipo_id")] = tipoId;
            ptArr.append(pt);
        }
        if (!ptArr.isEmpty()) {
            ++pending;
            QNetworkReply* r = m_client->post(QStringLiteral("pieza_tipo"), ptArr);
            connect(r, &QNetworkReply::finished, this, [r, checkDone]() {
                r->deleteLater();
                checkDone();
            });
        }
    }

    if (!imagenesBase64.isEmpty()) {
        QJsonArray galArr;
        for (int i = 0; i < imagenesBase64.size(); ++i) {
            QJsonObject g;
            g[QStringLiteral("nombre")] = QStringLiteral("Imagen %1").arg(i + 1);
            g[QStringLiteral("formato")] = QStringLiteral("image/png");
            g[QStringLiteral("base64")] = imagenesBase64[i];
            g[QStringLiteral("pieza_id")] = piezaId;
            g[QStringLiteral("orden")] = i;
            galArr.append(g);
        }
        ++pending;
        QNetworkReply* r = m_client->post(QStringLiteral("galeria"), galArr);
        connect(r, &QNetworkReply::finished, this, [r, checkDone]() {
            r->deleteLater();
            checkDone();
        });
    }

    if (pending == 0)
        emit mutationDone();
}
