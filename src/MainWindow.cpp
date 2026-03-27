#include "MainWindow.h"
#include "core/SupabaseClient.h"
#include "core/AuthService.h"
#include "ui/LoginWidget.h"
#include "ui/DashboardWidget.h"
#include <QStackedWidget>
#include <QIcon>

MainWindow::MainWindow()
    : m_supabase(new SupabaseClient(this))
    , m_authService(new AuthService(m_supabase, this))
    , m_dashboardWidget(nullptr)
{
    setWindowTitle(tr("Bel\u00e9n Creator"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/logo.ico")));

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_loginWidget = new LoginWidget(m_authService, this);
    m_stack->addWidget(m_loginWidget);

    connect(m_loginWidget, &LoginWidget::loginSuccessful,
            this, &MainWindow::onLoginSuccessful);
}

MainWindow::~MainWindow()
{
}

void MainWindow::onLoginSuccessful(const Usuario& user)
{
    m_dashboardWidget = new DashboardWidget(user, m_supabase, this);
    m_stack->addWidget(m_dashboardWidget);
    m_stack->setCurrentWidget(m_dashboardWidget);

    connect(m_dashboardWidget, &DashboardWidget::logoutRequested,
            this, &MainWindow::onLogout);
}

void MainWindow::onLogout()
{
    m_stack->setCurrentWidget(m_loginWidget);
    if (m_dashboardWidget) {
        m_stack->removeWidget(m_dashboardWidget);
        m_dashboardWidget->deleteLater();
        m_dashboardWidget = nullptr;
    }
    m_authService->logout();
    // No borrar QSettings de auto-login: el usuario pidió entrar solo al abrir la app;
    // cerrar sesión solo invalida tokens en memoria.
}
