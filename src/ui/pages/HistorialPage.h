#pragma once

#include <QWidget>

class HistorialService;
class QTableWidget;
class QComboBox;
class QLabel;

class HistorialPage : public QWidget
{
    Q_OBJECT

public:
    explicit HistorialPage(HistorialService* historial, QWidget* parent = nullptr);

private slots:
    void reload();

private:
    HistorialService* m_historial;
    QComboBox* m_filterCombo;
    QComboBox* m_userFilterCombo;
    QTableWidget* m_table;
    QLabel* m_statusLabel;
};
