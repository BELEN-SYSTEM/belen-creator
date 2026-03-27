#pragma once

#include "../models/Ubicacion.h"
#include <QObject>
#include <QVector>

class SupabaseClient;

class UbicacionService : public QObject
{
    Q_OBJECT

public:
    explicit UbicacionService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchAll();
    void create(const QString& nombre);
    void update(int id, const QString& nombre);
    void remove(int id);

signals:
    void dataReady(const QVector<Ubicacion>& items);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
};
