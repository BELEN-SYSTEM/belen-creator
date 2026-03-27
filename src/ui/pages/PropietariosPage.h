#pragma once

#include <QWidget>

class SupabaseClient;
class PropietarioService;
class LoadingOverlay;
class QTableWidget;
class QPushButton;
class QLabel;

class PropietariosPage : public QWidget
{
    Q_OBJECT
public:
    explicit PropietariosPage(SupabaseClient* supabase, QWidget* parent = nullptr);

private slots:
    void refreshTable();
    void onAdd();
    void onEdit();
    void onDelete();

private:
    int selectedId() const;

    SupabaseClient* m_supabase;
    PropietarioService* m_service;
    LoadingOverlay* m_overlay;
    QTableWidget* m_table;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QLabel* m_countLabel;
};
