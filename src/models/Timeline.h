#pragma once

#include <QString>

struct Timeline
{
    int id = 0;
    QString name;
    QString modo;
    int ciclos = 0;
    /// Tablero canvas superior en cm (persistido en params.dimensiones.longitud / anchura).
    double dimensionesLongitudCm = 0;
    double dimensionesAnchuraCm = 0;
};
