#pragma once

#include "../models/Timeline.h"
#include <QObject>
#include <QVector>

class SupabaseClient;
class HistorialService;

class TimelineService : public QObject
{
    Q_OBJECT

public:
    explicit TimelineService(SupabaseClient* client, HistorialService* historial = nullptr,
                             QObject* parent = nullptr);

    void fetchAll();
    void create(const QString& name, double longitudCm, double anchuraCm);
    void update(int id, const QString& name, double longitudCm, double anchuraCm);
    void remove(int id);

signals:
    void dataReady(const QVector<Timeline>& items);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
    HistorialService* m_historial;
};
