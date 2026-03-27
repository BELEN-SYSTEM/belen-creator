#pragma once

#include "../models/Pieza.h"
#include <QStringList>
#include <QVector>
#include <Qt>

struct PiezaCardData
{
    Pieza pieza;
    QStringList tipos;
    QVector<int> tipoIds;
    QVector<QString> imagenesBase64;
    QString propietarioNombre;
    QString ubicacionNombre;
};

struct PiezaListParams
{
    QString searchNombre;
    int filterTipoId = 0;
    int filterPropietarioId = 0;
    int filterUbicacionId = 0;
    QString sortBy = QStringLiteral("codigo");
    Qt::SortOrder sortOrder = Qt::DescendingOrder;
};
