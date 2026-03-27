#pragma once

#include <QString>

struct Puerto
{
    int id = 0;
    QString nombre;
    int tipoId = 0;
    int pin = 0;
    QString params;
    QString tipoNombre; // solo para listado (join con tipo)
};
