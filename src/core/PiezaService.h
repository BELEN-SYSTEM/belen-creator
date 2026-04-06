#pragma once

#include "PiezaCardData.h"
#include <QDate>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <functional>

class SupabaseClient;
class HistorialService;

class PiezaService : public QObject
{
    Q_OBJECT

public:
    explicit PiezaService(SupabaseClient* client, HistorialService* historial = nullptr,
                          QObject* parent = nullptr);

    void fetchPiezasForCards(const PiezaListParams& params = PiezaListParams());
    /** Listado ligero `id`+`nombre` ordenado por nombre; `timelineContext` se devuelve en la señal (p. ej. timeline abierto). */
    void fetchTodosIdNombre(int timelineContext);
    /** Primera imagen de galería + largo/ancho cm para colocación en canvas del timeline. */
    void fetchPiezaPlacementPreview(int piezaId);
    void fetchPiezaForEdit(int id);
    /// largoCm/anchoCm/altoCm: QVariant nulo o invalido = NULL en BD; numero = valor en cm.
    void create(const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVariant& largoCm, const QVariant& anchoCm, const QVariant& altoCm,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                const QString& piezaParamsJson);
    void update(int id, const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVariant& largoCm, const QVariant& anchoCm, const QVariant& altoCm,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                const QString& piezaParamsJson);
    void remove(int id);

signals:
    void piezasFetched(const QVector<PiezaCardData>& cards);
    void todosPiezasIdNombreReady(int timelineContext, const QVector<int>& ids, const QStringList& nombres,
                                  const QStringList& codigos, const QStringList& tooltips,
                                  const QVariantList& tipoIdsPorPieza);
    /** `tieneAccionesDisponibles`: algún tipo de la pieza tiene `acciones` JSON con al menos una clave. */
    void piezaPlacementPreviewReady(int piezaId, const QString& primeraImagenBase64, double largoCm,
                                    double anchoCm, bool hasLargoCm, bool hasAnchoCm, bool tieneAccionesDisponibles);
    void piezaForEditReady(const PiezaCardData& card);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    void saveTiposAndImages(int piezaId, const QVector<int>& tipoIds,
                            const QVector<QString>& imagenesBase64,
                            const QString& piezaParamsJson,
                            std::function<void()> onAllDone = {});
    void fetchPiezaComposite(int piezaId, std::function<void(QJsonObject)> onDone);
    SupabaseClient* m_client;
    HistorialService* m_historial;
};
