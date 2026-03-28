#include "PiezaService.h"
#include "SupabaseClient.h"
#include "HistorialService.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDate>
#include <memory>

static QJsonObject buildPiezaComposite(const QJsonObject& row)
{
    QJsonObject pieza;
    pieza.insert(QStringLiteral("id"), row.value(QStringLiteral("id")));
    pieza.insert(QStringLiteral("codigo"), row.value(QStringLiteral("codigo")));
    pieza.insert(QStringLiteral("nombre"), row.value(QStringLiteral("nombre")));
    pieza.insert(QStringLiteral("notas"), row.value(QStringLiteral("notas")));
    pieza.insert(QStringLiteral("fecha"), row.value(QStringLiteral("fecha")));
    pieza.insert(QStringLiteral("precio"), row.value(QStringLiteral("precio")));
    pieza.insert(QStringLiteral("propietario_id"), row.value(QStringLiteral("propietario_id")));
    pieza.insert(QStringLiteral("ubicacion_id"), row.value(QStringLiteral("ubicacion_id")));
    pieza.insert(QStringLiteral("params"), row.value(QStringLiteral("params")));

    QJsonArray ptArr;
    for (const QJsonValue& v : row.value(QStringLiteral("pieza_tipo")).toArray())
        ptArr.append(v.toObject());
    QJsonArray galArr;
    for (const QJsonValue& v : row.value(QStringLiteral("galeria")).toArray())
        galArr.append(v.toObject());

    QJsonObject out;
    out.insert(QStringLiteral("pieza"), pieza);
    out.insert(QStringLiteral("pieza_tipo"), ptArr);
    out.insert(QStringLiteral("galeria"), galArr);
    return out;
}

PiezaService::PiezaService(SupabaseClient* client, HistorialService* historial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_historial(historial)
{
}

void PiezaService::fetchPiezaComposite(int piezaId, std::function<void(QJsonObject)> onDone)
{
    QString query = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,params,"
        "pieza_tipo(id,tipo_id,params),galeria(id,nombre,formato,base64,orden,pieza_id)"
        "&id=eq.%1").arg(piezaId);

    QNetworkReply* reply = m_client->get(QStringLiteral("pieza"), query);
    connect(reply, &QNetworkReply::finished, this, [reply, onDone = std::move(onDone)]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok || arr.isEmpty()) {
            onDone({});
            return;
        }
        onDone(buildPiezaComposite(arr.first().toObject()));
    });
}

void PiezaService::fetchPiezasForCards(const PiezaListParams& params)
{
    QString select = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,params,"
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
            {
                QJsonValue pv = o.value(QStringLiteral("params"));
                if (pv.isObject())
                    card.pieza.paramsJson = QString::fromUtf8(
                        QJsonDocument(pv.toObject()).toJson(QJsonDocument::Compact));
                else if (pv.isString())
                    card.pieza.paramsJson = pv.toString();
                else
                    card.pieza.paramsJson = QStringLiteral("{}");
            }

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
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,params,"
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
        {
            QJsonValue pv = o.value(QStringLiteral("params"));
            if (pv.isObject())
                card.pieza.paramsJson = QString::fromUtf8(
                    QJsonDocument(pv.toObject()).toJson(QJsonDocument::Compact));
            else if (pv.isString())
                card.pieza.paramsJson = pv.toString();
            else
                card.pieza.paramsJson = QStringLiteral("{}");
        }

        for (const QJsonValue& pt : o.value(QStringLiteral("pieza_tipo")).toArray())
            card.tipoIds.append(pt.toObject().value(QStringLiteral("tipo_id")).toInt());
        for (const QJsonValue& g : o.value(QStringLiteral("galeria")).toArray())
            card.imagenesBase64.append(g.toObject().value(QStringLiteral("base64")).toString());

        emit piezaForEditReady(card);
    });
}

static QJsonObject paramsJsonToObject(const QString& json)
{
    if (json.isEmpty() || json == QLatin1String("{}")) return {};
    QJsonParseError e;
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8(), &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) return {};
    return d.object();
}

void PiezaService::create(const QString& codigo, const QString& nombre, const QString& notas,
                           const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                           const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                           const QString& piezaParamsJson)
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
    body[QStringLiteral("params")] = paramsJsonToObject(piezaParamsJson);

    QNetworkReply* reply = m_client->post(QStringLiteral("pieza"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tipoIds, imagenesBase64, codigo, nombre]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok || arr.isEmpty()) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        int piezaId = arr.first().toObject().value(QStringLiteral("id")).toInt();
        if (piezaId <= 0) { emit errorOccurred(QStringLiteral("No se pudo obtener id de pieza")); return; }

        saveTiposAndImages(piezaId, tipoIds, imagenesBase64, [this, piezaId]() {
            if (!m_historial) {
                emit mutationDone();
                return;
            }
            fetchPiezaComposite(piezaId, [this](QJsonObject comp) {
                m_historial->registrar(QStringLiteral("pieza"), QJsonValue(),
                                       comp.isEmpty() ? QJsonValue() : QJsonValue(comp));
                emit mutationDone();
            });
        });
    });
}

void PiezaService::update(int id, const QString& codigo, const QString& nombre, const QString& notas,
                           const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                           const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                           const QString& piezaParamsJson)
{
    QString snapQuery = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,params,"
        "pieza_tipo(id,tipo_id,params),galeria(id,nombre,formato,base64,orden,pieza_id)"
        "&id=eq.%1").arg(id);

    QNetworkReply* snapReply = m_client->get(QStringLiteral("pieza"), snapQuery);
    connect(snapReply, &QNetworkReply::finished, this, [this, snapReply, id, codigo, nombre, notas, fecha,
                                                          precio, propietarioId, ubicacionId, tipoIds,
                                                          imagenesBase64, piezaParamsJson]() {
        snapReply->deleteLater();
        bool snapOk = false;
        QJsonArray snapArr = SupabaseClient::parseArrayReply(snapReply, snapOk);
        QJsonObject antesComp = (snapOk && !snapArr.isEmpty())
            ? buildPiezaComposite(snapArr.first().toObject())
            : QJsonObject{};

        QJsonObject body;
        body[QStringLiteral("codigo")] = codigo;
        body[QStringLiteral("nombre")] = nombre;
        body[QStringLiteral("notas")] = notas;
        body[QStringLiteral("fecha")] = fecha.isValid() ? fecha.toString(Qt::ISODate) : QJsonValue(QJsonValue::Null);
        body[QStringLiteral("precio")] = precio;
        body[QStringLiteral("propietario_id")] = propietarioId;
        body[QStringLiteral("ubicacion_id")] = ubicacionId > 0 ? QJsonValue(ubicacionId) : QJsonValue(QJsonValue::Null);
        body[QStringLiteral("params")] = paramsJsonToObject(piezaParamsJson);

        QNetworkReply* reply = m_client->patch(
            QStringLiteral("pieza"), QStringLiteral("id=eq.%1").arg(id), body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, tipoIds, imagenesBase64, antesComp,
                                                          codigo, nombre]() {
            reply->deleteLater();
            bool ok = false;
            SupabaseClient::parseArrayReply(reply, ok);
            if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }

            QNetworkReply* delPt = m_client->del(
                QStringLiteral("pieza_tipo"), QStringLiteral("pieza_id=eq.%1").arg(id));
            connect(delPt, &QNetworkReply::finished, this, [this, delPt, id, tipoIds, imagenesBase64, antesComp,
                                                              codigo, nombre]() {
                delPt->deleteLater();
                QNetworkReply* delGal = m_client->del(
                    QStringLiteral("galeria"), QStringLiteral("pieza_id=eq.%1").arg(id));
                connect(delGal, &QNetworkReply::finished, this, [this, delGal, id, tipoIds, imagenesBase64,
                                                                 antesComp, codigo, nombre]() {
                    delGal->deleteLater();
                    saveTiposAndImages(id, tipoIds, imagenesBase64, [this, id, antesComp]() {
                        if (!m_historial) {
                            emit mutationDone();
                            return;
                        }
                        fetchPiezaComposite(id, [this, antesComp](QJsonObject despuesComp) {
                            m_historial->registrar(QStringLiteral("pieza"),
                                                     antesComp.isEmpty() ? QJsonValue() : QJsonValue(antesComp),
                                                     despuesComp.isEmpty() ? QJsonValue() : QJsonValue(despuesComp));
                            emit mutationDone();
                        });
                    });
                });
            });
        });
    });
}

void PiezaService::remove(int id)
{
    QString snapQuery = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,params,"
        "pieza_tipo(id,tipo_id,params),galeria(id,nombre,formato,base64,orden,pieza_id)"
        "&id=eq.%1").arg(id);

    QNetworkReply* snapReply = m_client->get(QStringLiteral("pieza"), snapQuery);
    connect(snapReply, &QNetworkReply::finished, this, [this, snapReply, id]() {
        snapReply->deleteLater();
        bool snapOk = false;
        QJsonArray snapArr = SupabaseClient::parseArrayReply(snapReply, snapOk);
        QJsonObject antesComp;
        if (snapOk && !snapArr.isEmpty())
            antesComp = buildPiezaComposite(snapArr.first().toObject());

        QNetworkReply* reply = m_client->del(
            QStringLiteral("pieza"), QStringLiteral("id=eq.%1").arg(id));
        connect(reply, &QNetworkReply::finished, this, [this, reply, antesComp]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit errorOccurred(SupabaseClient::errorString(reply));
                return;
            }
            if (m_historial) {
                m_historial->registrar(QStringLiteral("pieza"),
                                       antesComp.isEmpty() ? QJsonValue() : QJsonValue(antesComp),
                                       QJsonValue());
            }
            emit mutationDone();
        });
    });
}

void PiezaService::saveTiposAndImages(int piezaId, const QVector<int>& tipoIds,
                                       const QVector<QString>& imagenesBase64,
                                       std::function<void()> onAllDone)
{
    // pending y el callback deben vivir hasta los QNetworkReply::finished (no capturar por & a locales).
    auto pendingCount = std::make_shared<int>(0);
    auto userDone = std::make_shared<std::function<void()>>();
    if (onAllDone)
        *userDone = std::move(onAllDone);

    auto runFinish = [this, userDone]() {
        if (*userDone)
            (*userDone)();
        else
            emit mutationDone();
    };

    auto checkDone = [pendingCount, runFinish]() {
        if (--(*pendingCount) <= 0)
            runFinish();
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
            ++(*pendingCount);
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
        ++(*pendingCount);
        QNetworkReply* r = m_client->post(QStringLiteral("galeria"), galArr);
        connect(r, &QNetworkReply::finished, this, [r, checkDone]() {
            r->deleteLater();
            checkDone();
        });
    }

    if (*pendingCount == 0)
        runFinish();
}
