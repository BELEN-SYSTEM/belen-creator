#pragma once

#include "../models/Tipo.h"
#include <QObject>
#include <QVector>

class SupabaseClient;

class TipoService : public QObject
{
    Q_OBJECT

public:
    explicit TipoService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchAll();
    void create(const QString& nombre, const QString& acciones, const QString& params);
    void update(int id, const QString& nombre, const QString& acciones, const QString& params);
    void remove(int id);

signals:
    void dataReady(const QVector<Tipo>& items);
    void mutationDone();
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
};
