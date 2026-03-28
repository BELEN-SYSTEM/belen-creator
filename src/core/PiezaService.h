#pragma once

#include "PiezaCardData.h"
#include <QObject>
#include <QString>
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
    void fetchPiezaForEdit(int id);
    void create(const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                const QString& piezaParamsJson);
    void update(int id, const QString& codigo, const QString& nombre, const QString& notas,
                const QDate& fecha, double precio, int propietarioId, int ubicacionId,
                const QVector<int>& tipoIds, const QVector<QString>& imagenesBase64,
                const QString& piezaParamsJson);
    void remove(int id);

signals:
    void piezasFetched(const QVector<PiezaCardData>& cards);
    void piezaForEditReady(const PiezaCardData& card);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    void saveTiposAndImages(int piezaId, const QVector<int>& tipoIds,
                            const QVector<QString>& imagenesBase64,
                            std::function<void()> onAllDone = {});
    void fetchPiezaComposite(int piezaId, std::function<void(QJsonObject)> onDone);
    SupabaseClient* m_client;
    HistorialService* m_historial;
};
