#include "DashboardWidget.h"
#include "core/SupabaseClient.h"
#include "pages/PiezasPage.h"
#include "pages/TimelinesPage.h"
#include "pages/PropietariosPage.h"
#include "pages/UbicacionesPage.h"
#include "pages/PuertosPage.h"
#include "pages/DispositivosPage.h"
#include "pages/TiposPage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>

static const char* sidebarStyle = R"(
    #sidebar { background-color: #2c3e50; }
    #sidebarTitle {
        font-size: 18px; font-weight: bold; color: white;
        padding: 24px 24px 8px 24px;
    }
    #userLabel {
        font-size: 12px; color: #bdc3c7;
        padding: 8px 24px;
    }
    #sidebar QPushButton {
        text-align: left;
        padding: 12px 24px;
        border: none;
        color: #ecf0f1;
        font-size: 13px;
        background-color: transparent;
        border-radius: 0;
    }
    #sidebar QPushButton:hover { background-color: #34495e; }
    #sidebar QPushButton:checked {
        background-color: #4a6fa5;
        color: #e8eef4;
        font-weight: bold;
    }
    #logoutBtn { color: #e74c3c; }
    #logoutBtn:hover { background-color: #34495e; color: #ff6b6b; }
)";

DashboardWidget::DashboardWidget(const Usuario& user, SupabaseClient* supabase, QWidget* parent)
    : QWidget(parent)
    , m_user(user)
    , m_supabase(supabase)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(220);
    sidebar->setStyleSheet(QString::fromUtf8(sidebarStyle));

    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    setupSidebar(sidebarLayout);
    mainLayout->addWidget(sidebar);

    auto* content = new QWidget(this);
    content->setStyleSheet(QStringLiteral("background-color: #f5f6fa;"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(content);
    contentLayout->addWidget(m_stack);
    mainLayout->addWidget(content, 1);

    setupPages();

    if (!m_navButtons.isEmpty()) {
        m_navButtons[0]->setChecked(true);
        m_stack->setCurrentIndex(0);
    }
}

void DashboardWidget::setupSidebar(QVBoxLayout* layout)
{
    auto* title = new QLabel(tr("Bel\u00e9n Creator"), this);
    title->setObjectName(QStringLiteral("sidebarTitle"));
    layout->addWidget(title);

    layout->addSpacing(16);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    connect(m_navGroup, &QButtonGroup::idClicked,
            this, &DashboardWidget::onNavButtonClicked);

    int idx = 0;
    addNavButton(layout, tr("Piezas"),        idx++);
    addNavButton(layout, tr("Timelines"),     idx++);
    addNavButton(layout, tr("Propietarios"),  idx++);
    addNavButton(layout, tr("Ubicaciones"),   idx++);
    addNavButton(layout, tr("Dispositivos"),  idx++);
    addNavButton(layout, tr("Tipos"), idx++);
    addNavButton(layout, tr("Puertos"), idx++);

    layout->addStretch(1);

    auto* userLabel = new QLabel(m_user.nombre, this);
    userLabel->setObjectName(QStringLiteral("userLabel"));
    layout->addWidget(userLabel);

    auto* logoutBtn = new QPushButton(tr("Cerrar sesi\u00f3n"), this);
    logoutBtn->setObjectName(QStringLiteral("logoutBtn"));
    logoutBtn->setCursor(Qt::PointingHandCursor);
    connect(logoutBtn, &QPushButton::clicked, this, &DashboardWidget::logoutRequested);
    layout->addWidget(logoutBtn);
}

QPushButton* DashboardWidget::addNavButton(QVBoxLayout* layout, const QString& text, int index)
{
    auto* btn = new QPushButton(text, this);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    m_navGroup->addButton(btn, index);
    m_navButtons.append(btn);
    layout->addWidget(btn);
    return btn;
}

void DashboardWidget::setupPages()
{
    m_stack->addWidget(new PiezasPage(m_supabase, this));
    m_stack->addWidget(new TimelinesPage(this));
    m_stack->addWidget(new PropietariosPage(m_supabase, this));
    m_stack->addWidget(new UbicacionesPage(m_supabase, this));
    m_stack->addWidget(new DispositivosPage(this));

    const bool tiposPuertosViewOnly = (m_user.permiso != QStringLiteral("admin"));
    m_stack->addWidget(new TiposPage(m_supabase, tiposPuertosViewOnly, this));
    m_stack->addWidget(new PuertosPage(m_supabase, tiposPuertosViewOnly, this));
}

void DashboardWidget::onNavButtonClicked(int index)
{
    m_stack->setCurrentIndex(index);
}
