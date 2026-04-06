#pragma once

#include "models/Timeline.h"
#include <QHash>
#include <QWidget>

class SupabaseClient;
class HistorialService;
class TimelineService;
class UsadaService;
class PiezaService;
class TipoService;
class LoadingOverlay;
class QTableWidget;
class QPushButton;
class QLabel;
class QStackedWidget;

class TimelinesPage : public QWidget
{
    Q_OBJECT
public:
    explicit TimelinesPage(SupabaseClient* supabase, HistorialService* historial,
                           QWidget* parent = nullptr);

private slots:
    void refreshTable();
    void onAdd();
    void onEdit();
    void onDelete();

private:
    int selectedId() const;
    void openTimelineEditor(const Timeline& timeline);

    SupabaseClient* m_supabase;
    TimelineService* m_service;
    UsadaService* m_usadaService;
    PiezaService* m_piezaService;
    TipoService* m_tipoService;
    LoadingOverlay* m_overlay;
    QStackedWidget* m_stack;
    QWidget* m_listPage;
    QWidget* m_editorPage;
    QTableWidget* m_table;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;
    QLabel* m_countLabel;
    QHash<int, Timeline> m_timelineCache;
};
