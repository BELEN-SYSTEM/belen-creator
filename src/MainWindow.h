#pragma once

#include <QMainWindow>
#include "models/Usuario.h"

class SupabaseClient;
class AuthService;
class LoginWidget;
class DashboardWidget;
class QStackedWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

private slots:
    void onLoginSuccessful(const Usuario& user);
    void onLogout();

private:
    SupabaseClient* m_supabase;
    AuthService* m_authService;
    QStackedWidget* m_stack;
    LoginWidget* m_loginWidget;
    DashboardWidget* m_dashboardWidget;
};
