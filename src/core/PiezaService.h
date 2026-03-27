#pragma once

#include "PiezaCardData.h"
#include <QObject>
#include <QVector>

class SupabaseClient;

class PiezaService : public QObject
{
    Q_OBJECT

public:
    explicit PiezaService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchPiezasForCards(const PiezaListParams& params = PiezaListParams());
    void fetchPiezaForEdit(int id);
    void create(const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64);
    void update(int id, const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64);
    void remove(int id);

signals:
    void piezasFetched(const QVector<PiezaCardData>& cards);
    void piezaForEditReady(const PiezaCardData& card);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    void saveTiposAndImages(int piezaId, const QVector<int>& tipoIds,
                            const QVector<QString>& imagenesBase64);
    SupabaseClient* m_client;
};
