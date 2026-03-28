#include "HistorialService.h"
#include "SupabaseClient.h"
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QSet>
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>

namespace {

QString trH(const char* text)
{
    return QCoreApplication::translate("HistorialService", text);
}

QString truncField(const QString& s, int maxLen = 64)
{
    if (s.size() <= maxLen)
        return s;
    return s.left(qMax(1, maxLen - 1)) + QChar(0x2026);
}

QString jsonValToText(const QJsonValue& v)
{
    if (v.isNull() || v.isUndefined())
        return QStringLiteral("—");
    if (v.isBool())
        return v.toBool() ? QStringLiteral("s\u00ed") : QStringLiteral("no");
    if (v.isDouble()) {
        double d = v.toDouble();
        if (d == std::floor(d))
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'f', 2);
    }
    if (v.isString())
        return truncField(v.toString(), 80);
    if (v.isArray())
        return trH("{lista}");
    if (v.isObject())
        return trH("{objeto}");
    return QStringLiteral("?");
}

QString etiquetaCampo(const QString& key)
{
    static const QHash<QString, QString> map = {
        { QStringLiteral("nombre"), QStringLiteral("nombre") },
        { QStringLiteral("codigo"), QStringLiteral("c\u00f3digo") },
        { QStringLiteral("notas"), QStringLiteral("notas") },
        { QStringLiteral("fecha"), QStringLiteral("fecha") },
        { QStringLiteral("precio"), QStringLiteral("precio") },
        { QStringLiteral("propietario_id"), QStringLiteral("propietario") },
        { QStringLiteral("ubicacion_id"), QStringLiteral("ubicaci\u00f3n") },
        { QStringLiteral("tipo_id"), QStringLiteral("tipo") },
        { QStringLiteral("pin"), QStringLiteral("pin") },
        { QStringLiteral("params"), QStringLiteral("params") },
        { QStringLiteral("acciones"), QStringLiteral("acciones") },
        { QStringLiteral("id"), QStringLiteral("id") },
    };
    return map.value(key, key);
}

QString diffObjetosJson(const QJsonObject& a, const QJsonObject& b)
{
    QSet<QString> keys;
    for (auto it = a.begin(); it != a.end(); ++it)
        keys.insert(it.key());
    for (auto it = b.begin(); it != b.end(); ++it)
        keys.insert(it.key());

    QStringList parts;
    for (const QString& k : keys) {
        if (k == QStringLiteral("id"))
            continue;
        QJsonValue va = a.value(k);
        QJsonValue vb = b.value(k);
        if (va != vb) {
            parts.append(QStringLiteral("%1: %2 \u2192 %3")
                             .arg(etiquetaCampo(k), jsonValToText(va), jsonValToText(vb)));
        }
    }
    return parts.join(QStringLiteral("; "));
}

QString resumenFilaSimple(const QJsonObject& row)
{
    QStringList bits;
    if (row.contains(QStringLiteral("id")))
        bits.append(trH("id %1").arg(row.value(QStringLiteral("id")).toVariant().toString()));
    const QString nom = row.value(QStringLiteral("nombre")).toString();
    if (!nom.isEmpty())
        bits.append(trH("nombre \u00ab%1\u00bb").arg(nom));
    const QString cod = row.value(QStringLiteral("codigo")).toString();
    if (!cod.isEmpty())
        bits.append(trH("c\u00f3digo %1").arg(cod));
    const QString acc = row.value(QStringLiteral("acciones")).toString();
    if (!acc.isEmpty())
        bits.append(trH("acciones %1").arg(truncField(acc, 48)));
    const QString par = row.value(QStringLiteral("params")).toString();
    if (!par.isEmpty())
        bits.append(trH("params %1").arg(truncField(par, 48)));
    if (row.contains(QStringLiteral("tipo_id")))
        bits.append(trH("tipo id %1").arg(row.value(QStringLiteral("tipo_id")).toVariant().toString()));
    if (row.contains(QStringLiteral("pin")))
        bits.append(trH("pin %1").arg(row.value(QStringLiteral("pin")).toVariant().toString()));
    if (bits.isEmpty())
        return trH("(sin detalle)");
    return bits.join(QStringLiteral(", "));
}

bool valorAusente(const QJsonValue& v)
{
    return v.isNull() || v.isUndefined()
        || (v.isObject() && v.toObject().isEmpty());
}

QString describePiezaFila(const QJsonObject& p)
{
    QStringList bits;
    if (p.contains(QStringLiteral("id")))
        bits.append(trH("id %1").arg(p.value(QStringLiteral("id")).toVariant().toString()));
    const QString c = p.value(QStringLiteral("codigo")).toString();
    if (!c.isEmpty())
        bits.append(trH("c\u00f3digo %1").arg(c));
    const QString n = p.value(QStringLiteral("nombre")).toString();
    if (!n.isEmpty())
        bits.append(trH("\u00ab%1\u00bb").arg(n));
    return bits.isEmpty() ? trH("(sin datos)") : bits.join(QStringLiteral(", "));
}

QString textoPieza(const QJsonValue& va, const QJsonValue& vb)
{
    const QJsonObject a = va.isObject() ? va.toObject() : QJsonObject();
    const QJsonObject b = vb.isObject() ? vb.toObject() : QJsonObject();
    const QJsonObject pa = a.value(QStringLiteral("pieza")).toObject();
    const QJsonObject pb = b.value(QStringLiteral("pieza")).toObject();
    const QJsonArray tta = a.value(QStringLiteral("pieza_tipo")).toArray();
    const QJsonArray ttb = b.value(QStringLiteral("pieza_tipo")).toArray();
    const QJsonArray ga = a.value(QStringLiteral("galeria")).toArray();
    const QJsonArray gb = b.value(QStringLiteral("galeria")).toArray();

    const bool alta = valorAusente(va) && !pb.isEmpty();
    const bool baja = !pa.isEmpty() && valorAusente(vb);

    if (alta) {
        return trH("Alta de pieza: %1; %2 tipos asociados; %3 im\u00e1genes en galer\u00eda")
            .arg(describePiezaFila(pb))
            .arg(ttb.size())
            .arg(gb.size());
    }
    if (baja) {
        return trH("Baja de pieza: %1; se eliminaron %2 tipos y %3 im\u00e1genes asociadas")
            .arg(describePiezaFila(pa))
            .arg(tta.size())
            .arg(ga.size());
    }

    QStringList partes;
    const QString diffP = diffObjetosJson(pa, pb);
    if (!diffP.isEmpty())
        partes.append(trH("datos: %1").arg(diffP));
    if (tta.size() != ttb.size())
        partes.append(trH("n\u00famero de tipos asociados: %1 \u2192 %2").arg(tta.size()).arg(ttb.size()));
    if (ga.size() != gb.size())
        partes.append(trH("im\u00e1genes en galer\u00eda: %1 \u2192 %2").arg(ga.size()).arg(gb.size()));

    const QString ref = !pb.isEmpty() ? describePiezaFila(pb) : describePiezaFila(pa);
    if (partes.isEmpty())
        return trH("Actualizaci\u00f3n de pieza %1 (sin cambios detectados en el resumen)").arg(ref);
    return trH("Actualizaci\u00f3n de pieza %1: %2").arg(ref, partes.join(QStringLiteral("; ")));
}

} // namespace

// -----------------------------------------------------------------------------

HistorialService::HistorialService(SupabaseClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void HistorialService::setUsuario(const Usuario& usuario) { m_usuario = usuario; }
const Usuario& HistorialService::usuario() const { return m_usuario; }

QString HistorialService::textoResumenCambio(const QString& tabla, const QJsonValue& antes,
                                            const QJsonValue& despues)
{
    const QString t = tabla.trimmed();

    if (t == QStringLiteral("pieza"))
        return textoPieza(antes, despues);

    const QJsonObject oa = antes.isObject() ? antes.toObject() : QJsonObject();
    const QJsonObject ob = despues.isObject() ? despues.toObject() : QJsonObject();

    const QString ent = etiquetaTablaUsuario(t);

    if (valorAusente(antes) && !ob.isEmpty()) {
        return trH("Alta en %1: %2").arg(ent, resumenFilaSimple(ob));
    }
    if (!oa.isEmpty() && valorAusente(despues)) {
        return trH("Baja en %1: %2").arg(ent, resumenFilaSimple(oa));
    }

    const QString d = diffObjetosJson(oa, ob);
    if (d.isEmpty())
        return trH("Edici\u00f3n en %1 (sin diferencias de campos en el resumen)").arg(ent);
    return trH("Edici\u00f3n en %1: %2").arg(ent, d);
}

HistorialTipoCambio HistorialService::clasificarTipo(const QString& tabla, const QJsonValue& antes,
                                                      const QJsonValue& despues)
{
    auto ausente = [](const QJsonValue& v) {
        return v.isNull() || v.isUndefined()
            || (v.isObject() && v.toObject().isEmpty());
    };

    const QString t = tabla.trimmed();
    if (t == QStringLiteral("pieza")) {
        const QJsonObject a = antes.isObject() ? antes.toObject() : QJsonObject();
        const QJsonObject b = despues.isObject() ? despues.toObject() : QJsonObject();
        const QJsonObject pa = a.value(QStringLiteral("pieza")).toObject();
        const QJsonObject pb = b.value(QStringLiteral("pieza")).toObject();
        if (ausente(antes) && !pb.isEmpty())
            return HistorialTipoCambio::Alta;
        if (!pa.isEmpty() && ausente(despues))
            return HistorialTipoCambio::Baja;
        return HistorialTipoCambio::Edicion;
    }

    const QJsonObject oa = antes.isObject() ? antes.toObject() : QJsonObject();
    const QJsonObject ob = despues.isObject() ? despues.toObject() : QJsonObject();
    if (ausente(antes) && !ob.isEmpty())
        return HistorialTipoCambio::Alta;
    if (!oa.isEmpty() && ausente(despues))
        return HistorialTipoCambio::Baja;
    return HistorialTipoCambio::Edicion;
}

void HistorialService::registrar(const QString& tabla, const QJsonValue& antes, const QJsonValue& despues)
{
    if (!m_client)
        return;
    const QString texto = textoResumenCambio(tabla, antes, despues);

    QJsonObject body;
    body.insert(QStringLiteral("tabla"), tabla);
    body.insert(QStringLiteral("texto"), texto);
    if (antes.isNull() || antes.isUndefined())
        body.insert(QStringLiteral("antes"), QJsonValue());
    else
        body.insert(QStringLiteral("antes"), antes);
    if (despues.isNull() || despues.isUndefined())
        body.insert(QStringLiteral("despues"), QJsonValue());
    else
        body.insert(QStringLiteral("despues"), despues);

    if (!m_usuario.id.trimmed().isEmpty())
        body.insert(QStringLiteral("usuario_id"), m_usuario.id.trimmed());
    else
        body.insert(QStringLiteral("usuario_id"), QJsonValue());

    body.insert(QStringLiteral("fecha"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QNetworkReply* reply = m_client->post(QStringLiteral("historial"), body);
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            qWarning() << "[Historial]" << SupabaseClient::errorString(reply);
    });
}

void HistorialService::fetchAll(const QString& filtroTabla, const QString& filtroUsuarioId)
{
    QString q = QStringLiteral(
        "select=id,tabla,texto,antes,despues,usuario_id,fecha,usuario:usuario_id(nombre,correo)"
        "&order=fecha.desc,id.desc");
    if (!filtroTabla.trimmed().isEmpty())
        q += QStringLiteral("&tabla=eq.%1").arg(filtroTabla.trimmed());
    if (!filtroUsuarioId.trimmed().isEmpty())
        q += QStringLiteral("&usuario_id=eq.%1").arg(filtroUsuarioId.trimmed());

    QNetworkReply* reply = m_client->get(QStringLiteral("historial"), q);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        if (!ok) {
            emit errorOccurred(SupabaseClient::errorString(reply));
            return;
        }
        QVector<HistorialEntry> out;
        out.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            QJsonObject o = v.toObject();
            HistorialEntry e;
            e.id = o.value(QStringLiteral("id")).toVariant().toLongLong();
            e.tabla = o.value(QStringLiteral("tabla")).toString();
            e.texto = o.value(QStringLiteral("texto")).toString();
            e.antes = o.value(QStringLiteral("antes"));
            e.despues = o.value(QStringLiteral("despues"));
            e.usuarioId = o.value(QStringLiteral("usuario_id")).toString();
            {
                const QString fs = o.value(QStringLiteral("fecha")).toString();
                if (!fs.isEmpty()) {
                    e.fecha = QDateTime::fromString(fs, Qt::ISODateWithMs);
                    if (!e.fecha.isValid())
                        e.fecha = QDateTime::fromString(fs, Qt::ISODate);
                }
            }
            QJsonObject uo = o.value(QStringLiteral("usuario")).toObject();
            const QString unom = uo.value(QStringLiteral("nombre")).toString().trimmed();
            const QString ucor = uo.value(QStringLiteral("correo")).toString().trimmed();
            if (!unom.isEmpty())
                e.usuarioEtiqueta = unom;
            else if (!ucor.isEmpty())
                e.usuarioEtiqueta = ucor;
            else if (!e.usuarioId.isEmpty())
                e.usuarioEtiqueta = e.usuarioId.left(8) + QStringLiteral("\u2026");
            e.tipoCambio = clasificarTipo(e.tabla, e.antes, e.despues);
            out.append(e);
        }
        emit dataReady(out);
    });
}

void HistorialService::fetchUsuariosParaFiltro()
{
    if (!m_client) {
        emit usuariosFiltroReady({});
        return;
    }
    QNetworkReply* reply = m_client->get(
        QStringLiteral("usuario"),
        QStringLiteral("select=id,nombre,correo&order=nombre.asc"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool ok = false;
        QJsonArray arr = SupabaseClient::parseArrayReply(reply, ok);
        QVector<QPair<QString, QString>> pairs;
        if (ok) {
            for (const QJsonValue& v : arr) {
                QJsonObject o = v.toObject();
                const QString id = o.value(QStringLiteral("id")).toString();
                if (id.isEmpty())
                    continue;
                const QString nom = o.value(QStringLiteral("nombre")).toString().trimmed();
                const QString cor = o.value(QStringLiteral("correo")).toString().trimmed();
                QString lab = nom.isEmpty() ? cor : nom;
                if (lab.isEmpty())
                    lab = id.left(8) + QStringLiteral("\u2026");
                pairs.append({ id, lab });
            }
        }
        emit usuariosFiltroReady(pairs);
    });
}

QString HistorialService::etiquetaTablaUsuario(const QString& tablaDb)
{
    static const QHash<QString, QString> map = {
        { QStringLiteral("pieza"), QStringLiteral("Piezas") },
        { QStringLiteral("pieza_tipo"), QStringLiteral("Piezas (tipos)") },
        { QStringLiteral("galeria"), QStringLiteral("Galer\u00eda") },
        { QStringLiteral("propietario"), QStringLiteral("Propietarios") },
        { QStringLiteral("ubicacion"), QStringLiteral("Ubicaciones") },
        { QStringLiteral("tipo"), QStringLiteral("Tipos") },
        { QStringLiteral("puerto"), QStringLiteral("Puertos") },
        { QStringLiteral("historial"), QStringLiteral("Historial") },
        { QStringLiteral("timeline"), QStringLiteral("Timelines") },
        { QStringLiteral("usada"), QStringLiteral("Usadas") },
        { QStringLiteral("bluetooth"), QStringLiteral("Bluetooth") },
        { QStringLiteral("usuario"), QStringLiteral("Usuarios") },
    };
    const QString t = tablaDb.trimmed();
    if (map.contains(t))
        return map.value(t);
    if (t.isEmpty())
        return t;
    QString s = t;
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

QVector<QPair<QString, QString>> HistorialService::opcionesFiltroTabla()
{
    static const QStringList keys = {
        QStringLiteral("pieza"),
        QStringLiteral("propietario"),
        QStringLiteral("ubicacion"),
        QStringLiteral("tipo"),
        QStringLiteral("puerto"),
    };
    QVector<QPair<QString, QString>> out;
    out.append({ QString(),
                 QCoreApplication::translate("HistorialService", "Todas las tablas") });
    for (const QString& k : keys)
        out.append({ k, etiquetaTablaUsuario(k) });
    return out;
}
