#pragma once

#include <QString>
#include <QDate>

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
};
