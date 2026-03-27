#pragma once

#include "../models/Puerto.h"
#include <QObject>
#include <QVector>

class SupabaseClient;

class PuertoService : public QObject
{
    Q_OBJECT

public:
    explicit PuertoService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchAll();
    void create(const QString& nombre, int tipoId, int pin, const QString& params);
    void update(int id, const QString& nombre, int tipoId, int pin, const QString& params);
    void remove(int id);

signals:
    void dataReady(const QVector<Puerto>& items);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
};
