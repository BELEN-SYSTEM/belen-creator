#pragma once

#include "../models/Usada.h"
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class SupabaseClient;

class UsadaService : public QObject
{
    Q_OBJECT

public:
    explicit UsadaService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchAll();
    /** Filas de `usada` con `timeline_id`; nombres desde `pieza`, sin duplicar `pieza_id` (orden por primera aparición). */
    void fetchPiezasNombresPorTimeline(int timelineId);
    /** Filas con `time` NULL y `params.posicion` {x,y} en cm (escena del tablero). */
    void fetchCanvasPlacementsForTimeline(int timelineId);
    /** Inserta `usada` con `time` NULL, params JSON posición; `accion` y `puerto_id` nulos. */
    void insertCanvasPlacement(int timelineId, int piezaId, double xCm, double yCm);
    /** Actualiza solo `params.posicion` de la fila `usada` (tablero superior). */
    void updateCanvasPlacementPosition(int usadaId, double xCm, double yCm);
    /** Elimina la fila `usada` del tablero (`time` NULL). No borra usadas con tiempo en el timeline. */
    void deleteCanvasPlacement(int usadaId);

signals:
    void dataReady(const QVector<Usada>& items);
    /** `piezaIdsUsadas` y `tooltips` alineados con `nombres` (ids únicos por fila usada). */
    void piezasUsadasPorTimelineReady(int timelineId, const QStringList& nombres,
                                      const QVector<int>& piezaIdsUsadas, const QStringList& tooltips);
    /** Cada elemento: mapa `usadaId`, `piezaId`, `xCm`, `yCm`. */
    void canvasPlacementsReady(int timelineId, const QVariantList& placements);
    /** Si ok, `newUsadaId` es el id devuelto por el insert. */
    void canvasPlacementInsertFinished(bool ok, const QString& error, int newUsadaId);
    void canvasPlacementDeleteFinished(bool ok, const QString& error, int usadaId);
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
};
