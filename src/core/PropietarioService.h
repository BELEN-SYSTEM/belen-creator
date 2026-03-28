#pragma once

#include "../models/Propietario.h"
#include <QObject>
#include <QVector>

class SupabaseClient;
class HistorialService;

class PropietarioService : public QObject
{
    Q_OBJECT

public:
    explicit PropietarioService(SupabaseClient* client, HistorialService* historial = nullptr,
                                QObject* parent = nullptr);

    void fetchAll();
    void create(const QString& nombre);
    void update(int id, const QString& nombre);
    void remove(int id);

signals:
    void dataReady(const QVector<Propietario>& items);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
    HistorialService* m_historial;
};
