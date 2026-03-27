#pragma once

#include <QWidget>

class SupabaseClient;
class PuertoService;
class TipoService;
class LoadingOverlay;
class QTableWidget;
class QPushButton;
class QComboBox;
class QLabel;

class PuertosPage : public QWidget
{
    Q_OBJECT
public:
    /** Si viewOnly es true, solo se lista; sin añadir/editar/eliminar (permiso user). */
    explicit PuertosPage(SupabaseClient* supabase, bool viewOnly, QWidget* parent = nullptr);

private slots:
    void refreshTable();
    void onAdd();
    void onEdit();
    void onDelete();

private:
    int selectedId() const;
    static QString paramsToSummary(const QString& paramsJson);
    static QString buildParamsJson(QTableWidget* varsTable);
    static void fillParamsTable(const QString& paramsJson, QTableWidget* varsTable);

    bool m_viewOnly;
    SupabaseClient* m_supabase;
    PuertoService* m_service;
    TipoService* m_tipoService;
    LoadingOverlay* m_overlay;
    QTableWidget* m_table;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QComboBox* m_tipoFilterCombo;
    QComboBox* m_sortCombo;
    QLabel* m_countLabel;
};
