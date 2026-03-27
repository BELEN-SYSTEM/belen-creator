#pragma once

#include "../models/Usada.h"
#include <QObject>
#include <QVector>

class SupabaseClient;

class UsadaService : public QObject
{
    Q_OBJECT

public:
    explicit UsadaService(SupabaseClient* client, QObject* parent = nullptr);

    void fetchAll();

signals:
    void dataReady(const QVector<Usada>& items);
    void errorOccurred(const QString& msg);

private:
    SupabaseClient* m_client;
};
