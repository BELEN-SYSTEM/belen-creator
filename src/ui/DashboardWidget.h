#pragma once

#include <QWidget>
#include <QVector>
#include "../models/Usuario.h"

class SupabaseClient;
class HistorialService;
class QStackedWidget;
class QPushButton;
class QButtonGroup;
class QLabel;
class QVBoxLayout;

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(const Usuario& user, SupabaseClient* supabase, QWidget* parent = nullptr);

signals:
    void logoutRequested();

private slots:
    void onNavButtonClicked(int index);

private:
    void setupSidebar(QVBoxLayout* sidebarLayout);
    void setupPages();
    QPushButton* addNavButton(QVBoxLayout* layout, const QString& text, int index);

    Usuario m_user;
    SupabaseClient* m_supabase;
    HistorialService* m_historial;
    QStackedWidget* m_stack;
    QButtonGroup* m_navGroup;
    QVector<QPushButton*> m_navButtons;
};
