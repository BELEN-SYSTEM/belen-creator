#pragma once

#include <QWidget>

class SupabaseClient;
class UbicacionService;
class LoadingOverlay;
class QTableWidget;
class QPushButton;
class QLabel;

class UbicacionesPage : public QWidget
{
    Q_OBJECT
public:
    explicit UbicacionesPage(SupabaseClient* supabase, QWidget* parent = nullptr);

private slots:
    void refreshTable();
    void onAdd();
    void onEdit();
    void onDelete();

private:
    int selectedId() const;

    SupabaseClient* m_supabase;
    UbicacionService* m_service;
    LoadingOverlay* m_overlay;
    QTableWidget* m_table;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QLabel* m_countLabel;
};
