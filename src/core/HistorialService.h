#pragma once

#include "../models/Usuario.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonValue>
#include <QVector>

class SupabaseClient;

/** Clasificación del registro para la vista (alta / edición / baja). */
enum class HistorialTipoCambio { Alta, Edicion, Baja };

struct HistorialEntry
{
    qint64 id = 0;
    QString tabla;
    QString texto;
    QJsonValue antes;
    QJsonValue despues;
    QString usuarioId;
    /** Nombre o correo para mostrar; vacío si no hay usuario enlazado. */
    QString usuarioEtiqueta;
    /** Instante del cambio (UTC desde API). */
    QDateTime fecha;
    HistorialTipoCambio tipoCambio = HistorialTipoCambio::Edicion;
};

class HistorialService : public QObject
{
    Q_OBJECT

public:
    explicit HistorialService(SupabaseClient* client, QObject* parent = nullptr);

    void setUsuario(const Usuario& usuario);
    const Usuario& usuario() const;

    /** Genera la descripción del cambio a partir de antes/despues (sin mencionar al usuario). */
    static QString textoResumenCambio(const QString& tabla, const QJsonValue& antes,
                                      const QJsonValue& despues);

    static HistorialTipoCambio clasificarTipo(const QString& tabla, const QJsonValue& antes,
                                              const QJsonValue& despues);

    void registrar(const QString& tabla, const QJsonValue& antes, const QJsonValue& despues);

    /** filtros vacíos = sin filtrar por ese criterio. */
    void fetchAll(const QString& filtroTabla = {}, const QString& filtroUsuarioId = {});

    void fetchUsuariosParaFiltro();

    static QString etiquetaTablaUsuario(const QString& tablaDb);
    static QVector<QPair<QString, QString>> opcionesFiltroTabla();

signals:
    void dataReady(const QVector<HistorialEntry>& items);
    void errorOccurred(const QString& msg);
    void usuariosFiltroReady(const QVector<QPair<QString, QString>>& idToLabel);

private:
    SupabaseClient* m_client;
    Usuario m_usuario;
};
