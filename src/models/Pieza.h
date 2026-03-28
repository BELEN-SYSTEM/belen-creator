#pragma once

#include <QDate>
#include <QString>

struct Pieza
{
    int id = 0;
    QString codigo;
    QString nombre;
    QString notas;
    double precio = 0.0;
    int propietarioId = 0;
    int ubicacionId = 0;
    QDate fecha;
    /// JSON compacto: por cada id de tipo (clave string), objeto con nombreVariable -> valor
    QString paramsJson;
};
