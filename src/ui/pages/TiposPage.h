#pragma once

#include <QWidget>

class SupabaseClient;
class TipoService;
class LoadingOverlay;
class QTableWidget;
class QTreeWidget;
class QPushButton;
class QLabel;

class TiposPage : public QWidget
{
    Q_OBJECT
public:
    /** Si viewOnly es true, solo se lista; sin añadir/editar/eliminar (permiso user). */
    explicit TiposPage(SupabaseClient* supabase, bool viewOnly, QWidget* parent = nullptr);

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
    static QString accionesToSummary(const QString& accionesJson);
    static QString buildAccionesJson(QTreeWidget* accTree);
    static void fillAccionesTree(const QString& accionesJson, QTreeWidget* accTree);

    bool m_viewOnly;
    SupabaseClient* m_supabase;
    TipoService* m_service;
    LoadingOverlay* m_overlay;
    QTableWidget* m_table;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QLabel* m_countLabel;
};
