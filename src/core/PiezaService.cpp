#include "PiezaService.h"
#include "SupabaseClient.h"
#include "HistorialService.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDate>
#include <QLocale>
#include <QVariant>
#include <climits>
#include <memory>

static bool jsonValueToDoubleCm(const QJsonValue& v, double* out)
{
    if (!out)
        return false;
    if (v.isNull() || v.isUndefined())
        return false;
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        if (ok) {
            *out = d;
            return true;
        }
    }
    return false;
}

static bool accionesJsonTieneAlgunaClave(const QString& accionesJson)
{
    if (accionesJson.isEmpty())
        return false;
    const QByteArray utf8 = accionesJson.trimmed().toUtf8();
    if (utf8 == "{}" || utf8 == "null")
        return false;
    QJsonParseError err{};
    const QJsonDocument d = QJsonDocument::fromJson(utf8, &err);
    if (err.error != QJsonParseError::NoError || !d.isObject())
        return false;
    return !d.object().isEmpty();
}

/** PostgREST puede devolver `acciones` como objeto JSON o como cadena en columna text. */
static bool tipoAccionesValueTieneClaves(const QJsonValue& v)
{
    if (v.isObject())
        return !v.toObject().isEmpty();
    if (v.isString())
        return accionesJsonTieneAlgunaClave(v.toString());
    return false;
}

template<typename Fn>
static void forEachPiezaTipoLink(const QJsonObject& piezaRow, Fn fn)
{
    const QJsonValue ptv = piezaRow.value(QStringLiteral("pieza_tipo"));
    if (ptv.isArray()) {
        for (const QJsonValue& pt : ptv.toArray())
            fn(pt.toObject());
    } else if (ptv.isObject()) {
        fn(ptv.toObject());
    }
}

static QJsonObject paramsJsonToObject(const QString& json)
{
    if (json.isEmpty() || json == QLatin1String("{}")) return {};
    QJsonParseError e;
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8(), &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) return {};
    return d.object();
}

static QJsonObject piezaTipoRowParamsToObject(const QJsonObject& ptRow)
{
    const QJsonValue pv = ptRow.value(QStringLiteral("params"));
    if (pv.isObject())
        return pv.toObject();
    if (pv.isString())
        return paramsJsonToObject(pv.toString());
    return {};
}

/** Reconstruye el JSON agregado que usa la UI: { \"tipoId\": { var: valor, ... }, ... }. */
static QString aggregatedPiezaParamsJsonFromPiezaRow(const QJsonObject& piezaRow)
{
    QJsonObject out;
    forEachPiezaTipoLink(piezaRow, [&out](const QJsonObject& pt) {
        const QJsonValue tv = pt.value(QStringLiteral("tipo_id"));
        const int tid = tv.isDouble() ? static_cast<int>(tv.toDouble()) : tv.toInt();
        if (tid <= 0)
            return;
        const QJsonObject po = piezaTipoRowParamsToObject(pt);
        if (!po.isEmpty())
            out.insert(QString::number(tid), po);
    });
    if (out.isEmpty())
        return QStringLiteral("{}");
    return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
}

static bool piezaRowTieneAccionesDisponibles(const QJsonObject& o)
{
    bool found = false;
    forEachPiezaTipoLink(o, [&found](const QJsonObject& pt) {
        if (found)
            return;
        const QJsonObject tipo = pt.value(QStringLiteral("tipo")).toObject();
        if (tipo.isEmpty())
            return;
        if (tipoAccionesValueTieneClaves(tipo.value(QStringLiteral("acciones"))))
            found = true;
    });
    return found;
}

static void fillPiezaDimensionsFromRow(Pieza& pieza, const QJsonObject& o)
{
    double d = 0;
    pieza.hasLargoCm = jsonValueToDoubleCm(o.value(QStringLiteral("largo_cm")), &d);
    pieza.largoCm = d;
    pieza.hasAnchoCm = jsonValueToDoubleCm(o.value(QStringLiteral("ancho_cm")), &d);
    pieza.anchoCm = d;
    pieza.hasAltoCm = jsonValueToDoubleCm(o.value(QStringLiteral("alto_cm")), &d);
    pieza.altoCm = d;
}

static bool variantToDoubleCm(const QVariant& v, double* out)
{
    if (!out || !v.isValid() || v.isNull())
        return false;
    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok)
        return false;
    *out = d;
    return true;
}

static void insertOptionalDimension(QJsonObject& body, const QString& key, const QVariant& v)
{
    double d = 0;
    if (variantToDoubleCm(v, &d))
        body.insert(key, d);
    else
        body.insert(key, QJsonValue(QJsonValue::Null));
}

static QString formatDimCmForTooltip(bool has, double v)
{
    if (!has)
        return QStringLiteral("\u2014");
    return QLocale().toString(v, 'f', 2);
}

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
    pieza.insert(QStringLiteral("largo_cm"), row.value(QStringLiteral("largo_cm")));
    pieza.insert(QStringLiteral("ancho_cm"), row.value(QStringLiteral("ancho_cm")));
    pieza.insert(QStringLiteral("alto_cm"), row.value(QStringLiteral("alto_cm")));

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

static QString piezaRowTooltip(const PiezaService* self, const QJsonObject& pieza, int piezaId)
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
    double ld = 0, ad = 0, hd = 0;
    const bool hl = jsonValueToDoubleCm(pieza.value(QStringLiteral("largo_cm")), &ld);
    const bool ha = jsonValueToDoubleCm(pieza.value(QStringLiteral("ancho_cm")), &ad);
    const bool hh = jsonValueToDoubleCm(pieza.value(QStringLiteral("alto_cm")), &hd);
    if (hl || ha || hh) {
        lines.append(self->tr("Medidas (cm): %1 \u00d7 %2 \u00d7 %3")
                         .arg(formatDimCmForTooltip(hl, ld), formatDimCmForTooltip(ha, ad),
                              formatDimCmForTooltip(hh, hd)));
    }
    return lines.join(QLatin1Char('\n'));
}

void PiezaService::fetchPiezaComposite(int piezaId, std::function<void(QJsonObject)> onDone)
{
    QString query = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,largo_cm,ancho_cm,alto_cm,"
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
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,largo_cm,ancho_cm,alto_cm,"
        "propietario:propietario_id(nombre),"
        "ubicacion:ubicacion_id(nombre),"
        "pieza_tipo(tipo_id,params,tipo:tipo_id(nombre)),"
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
            fillPiezaDimensionsFromRow(card.pieza, o);
            card.pieza.paramsJson = aggregatedPiezaParamsJsonFromPiezaRow(o);

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

static void appendTipoIdsFromPiezaTipoJson(const QJsonObject& piezaRow, QVariantList* rowTids)
{
    if (!rowTids)
        return;
    const QJsonValue ptv = piezaRow.value(QStringLiteral("pieza_tipo"));
    auto appendOne = [rowTids](const QJsonObject& po) {
        const QJsonValue tv = po.value(QStringLiteral("tipo_id"));
        int tid = 0;
        if (tv.isDouble())
            tid = static_cast<int>(tv.toDouble());
        else
            tid = tv.toInt();
        if (tid > 0)
            rowTids->append(tid);
    };
    if (ptv.isArray()) {
        for (const QJsonValue& pt : ptv.toArray())
            appendOne(pt.toObject());
    } else if (ptv.isObject()) {
        appendOne(ptv.toObject());
    }
}

void PiezaService::fetchTodosIdNombre(int timelineContext)
{
    const QString query = QStringLiteral(
        "select=id,nombre,codigo,notas,precio,fecha,largo_cm,ancho_cm,alto_cm,"
        "propietario:propietario_id(nombre),ubicacion:ubicacion_id(nombre),"
        "pieza_tipo(tipo_id)&order=nombre.asc");
    QNetworkReply* reply = m_client->get(QStringLiteral("pieza"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, timelineContext]() {
        reply->deleteLater();
        bool ok = false;
        const QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            emit todosPiezasIdNombreReady(timelineContext, {}, {}, {}, {}, QVariantList());
            return;
        }
        QVector<int> ids;
        QStringList nombres;
        QStringList codigos;
        QStringList tooltips;
        QVariantList tipoIdsPorPieza;
        ids.reserve(arr.size());
        codigos.reserve(arr.size());
        tipoIdsPorPieza.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            const QJsonValue idV = o.value(QStringLiteral("id"));
            const int id = idV.isDouble() ? static_cast<int>(idV.toDouble()) : idV.toInt();
            QString nombre = o.value(QStringLiteral("nombre")).toString().trimmed();
            if (nombre.isEmpty())
                nombre = QStringLiteral("#%1").arg(id);
            ids.append(id);
            nombres.append(nombre);
            codigos.append(o.value(QStringLiteral("codigo")).toString().trimmed());
            tooltips.append(piezaRowTooltip(this, o, id));
            QVariantList rowTids;
            appendTipoIdsFromPiezaTipoJson(o, &rowTids);
            tipoIdsPorPieza.append(rowTids);
        }
        emit todosPiezasIdNombreReady(timelineContext, ids, nombres, codigos, tooltips, tipoIdsPorPieza);
    });
}

void PiezaService::fetchPiezaPlacementPreview(int piezaId)
{
    if (piezaId <= 0)
        return;
    const QString query = QStringLiteral(
        "select=id,largo_cm,ancho_cm,galeria(base64,orden),pieza_tipo(tipo(acciones))&id=eq.%1")
        .arg(piezaId);
    QNetworkReply* reply = m_client->get(QStringLiteral("pieza"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply, piezaId]() {
        reply->deleteLater();
        bool ok = false;
        const QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok || arr.isEmpty()) {
            emit piezaPlacementPreviewReady(piezaId, QString(), 0, 0, false, false, false);
            return;
        }
        const QJsonObject o = arr.first().toObject();
        const bool tieneAcc = piezaRowTieneAccionesDisponibles(o);
        Pieza p;
        fillPiezaDimensionsFromRow(p, o);
        const QJsonArray gal = o.value(QStringLiteral("galeria")).toArray();
        QString firstB64;
        int bestOrder = INT_MAX;
        for (const QJsonValue& gv : gal) {
            const QJsonObject go = gv.toObject();
            const QString b64 = go.value(QStringLiteral("base64")).toString();
            if (b64.isEmpty())
                continue;
            const int ord = go.value(QStringLiteral("orden")).toInt();
            if (firstB64.isEmpty() || ord < bestOrder) {
                firstB64 = b64;
                bestOrder = ord;
            }
        }
        if (firstB64.isEmpty() && !gal.isEmpty())
            firstB64 = gal.first().toObject().value(QStringLiteral("base64")).toString();
        emit piezaPlacementPreviewReady(piezaId, firstB64, p.largoCm, p.anchoCm, p.hasLargoCm, p.hasAnchoCm,
                                        tieneAcc);
    });
}

void PiezaService::fetchPiezaForEdit(int id)
{
    if (id <= 0) return;
    QString query = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,largo_cm,ancho_cm,alto_cm,"
        "propietario:propietario_id(nombre),"
        "ubicacion:ubicacion_id(nombre),"
        "pieza_tipo(tipo_id,params,tipo:tipo_id(nombre)),"
        "galeria(base64)"
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
        fillPiezaDimensionsFromRow(card.pieza, o);
        card.pieza.paramsJson = aggregatedPiezaParamsJsonFromPiezaRow(o);

        card.propietarioNombre = o.value(QStringLiteral("propietario")).toObject().value(QStringLiteral("nombre")).toString();
        card.ubicacionNombre = o.value(QStringLiteral("ubicacion")).toObject().value(QStringLiteral("nombre")).toString();

        forEachPiezaTipoLink(o, [&card](const QJsonObject& ptObj) {
            const QJsonValue tv = ptObj.value(QStringLiteral("tipo_id"));
            const int tid = tv.isDouble() ? static_cast<int>(tv.toDouble()) : tv.toInt();
            card.tipoIds.append(tid);
            card.tipos.append(
                ptObj.value(QStringLiteral("tipo")).toObject().value(QStringLiteral("nombre")).toString());
        });

        for (const QJsonValue& g : o.value(QStringLiteral("galeria")).toArray())
            card.imagenesBase64.append(g.toObject().value(QStringLiteral("base64")).toString());

        emit piezaForEditReady(card);
    });
}

void PiezaService::create(const QString& codigo, const QString& nombre, const QString& notas,
                           const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                           const QVariant& largoCm, const QVariant& anchoCm, const QVariant& altoCm,
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
    insertOptionalDimension(body, QStringLiteral("largo_cm"), largoCm);
    insertOptionalDimension(body, QStringLiteral("ancho_cm"), anchoCm);
    insertOptionalDimension(body, QStringLiteral("alto_cm"), altoCm);

    QNetworkReply* reply = m_client->post(QStringLiteral("pieza"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tipoIds, imagenesBase64, piezaParamsJson, codigo, nombre]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok || arr.isEmpty()) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        int piezaId = arr.first().toObject().value(QStringLiteral("id")).toInt();
        if (piezaId <= 0) { emit errorOccurred(QStringLiteral("No se pudo obtener id de pieza")); return; }

        saveTiposAndImages(piezaId, tipoIds, imagenesBase64, piezaParamsJson, [this, piezaId]() {
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
                           const QVariant& largoCm, const QVariant& anchoCm, const QVariant& altoCm,
                           const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                           const QString& piezaParamsJson)
{
    QString snapQuery = QStringLiteral(
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,largo_cm,ancho_cm,alto_cm,"
        "pieza_tipo(id,tipo_id,params),galeria(id,nombre,formato,base64,orden,pieza_id)"
        "&id=eq.%1").arg(id);

    QNetworkReply* snapReply = m_client->get(QStringLiteral("pieza"), snapQuery);
    connect(snapReply, &QNetworkReply::finished, this, [this, snapReply, id, codigo, nombre, notas, fecha,
                                                          precio, propietarioId, ubicacionId, largoCm, anchoCm,
                                                          altoCm, tipoIds, imagenesBase64, piezaParamsJson]() {
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
        insertOptionalDimension(body, QStringLiteral("largo_cm"), largoCm);
        insertOptionalDimension(body, QStringLiteral("ancho_cm"), anchoCm);
        insertOptionalDimension(body, QStringLiteral("alto_cm"), altoCm);

        QNetworkReply* reply = m_client->patch(
            QStringLiteral("pieza"), QStringLiteral("id=eq.%1").arg(id), body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, tipoIds, imagenesBase64, antesComp,
                                                          codigo, nombre, piezaParamsJson]() {
            reply->deleteLater();
            bool ok = false;
            SupabaseClient::parseArrayReply(reply, ok);
            if (!ok) { emit errorOccurred(SupabaseClient::errorString(reply)); return; }

            QNetworkReply* delPt = m_client->del(
                QStringLiteral("pieza_tipo"), QStringLiteral("pieza_id=eq.%1").arg(id));
            connect(delPt, &QNetworkReply::finished, this, [this, delPt, id, tipoIds, imagenesBase64, antesComp,
                                                              codigo, nombre, piezaParamsJson]() {
                delPt->deleteLater();
                QNetworkReply* delGal = m_client->del(
                    QStringLiteral("galeria"), QStringLiteral("pieza_id=eq.%1").arg(id));
                connect(delGal, &QNetworkReply::finished, this, [this, delGal, id, tipoIds, imagenesBase64,
                                                                 antesComp, codigo, nombre, piezaParamsJson]() {
                    delGal->deleteLater();
                    saveTiposAndImages(id, tipoIds, imagenesBase64, piezaParamsJson, [this, id, antesComp]() {
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
        "select=id,codigo,nombre,notas,fecha,precio,propietario_id,ubicacion_id,largo_cm,ancho_cm,alto_cm,"
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
                                       const QString& piezaParamsJson,
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
        const QJsonObject paramsPorTipo = paramsJsonToObject(piezaParamsJson);
        QJsonArray ptArr;
        for (int tipoId : tipoIds) {
            if (tipoId <= 0) continue;
            QJsonObject pt;
            pt[QStringLiteral("pieza_id")] = piezaId;
            pt[QStringLiteral("tipo_id")] = tipoId;
            const QJsonValue pv = paramsPorTipo.value(QString::number(tipoId));
            pt[QStringLiteral("params")] = pv.isObject() ? pv.toObject() : QJsonObject{};
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
