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
    /// Medidas en centímetros; `has*` false = NULL en BD
    bool hasLargoCm = false;
    double largoCm = 0;
    bool hasAnchoCm = false;
    double anchoCm = 0;
    bool hasAltoCm = false;
    double altoCm = 0;
    /// Agregado en cliente desde filas `pieza_tipo.params`: por cada tipo (clave string id), variables -> valor
    QString paramsJson;
};
