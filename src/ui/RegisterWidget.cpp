#include "RegisterWidget.h"
#include "core/AuthService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>

RegisterWidget::RegisterWidget(AuthService* authService, QWidget* parent)
    : QWidget(parent)
    , m_authService(authService)
    , m_stack(nullptr)
    , m_nombreEdit(nullptr)
    , m_correoEdit(nullptr)
    , m_contraEdit(nullptr)
    , m_errorLabel(nullptr)
    , m_registerBtn(nullptr)
{
    auto* outer = new QVBoxLayout(this);
    outer->setAlignment(Qt::AlignCenter);
    outer->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildFormCard());
    m_stack->addWidget(buildSuccessCard());
    outer->addWidget(m_stack, 1);

    connect(m_authService, &AuthService::registerSuccess, this, &RegisterWidget::onRegisterSuccess);
    connect(m_authService, &AuthService::registerFailed, this, &RegisterWidget::onRegisterFailed);
}

QWidget* RegisterWidget::buildFormCard()
{
    auto* wrap = new QWidget(this);
    auto* outer = new QVBoxLayout(wrap);
    outer->setAlignment(Qt::AlignCenter);

    auto* card = new QWidget(wrap);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedWidth(380);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(36, 40, 36, 36);
    lay->setSpacing(16);

    auto* title = new QLabel(tr("Bel\u00e9n Creator"), card);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    auto* subtitle = new QLabel(tr("Crear cuenta"), card);
    subtitle->setObjectName(QStringLiteral("subtitleLabel"));
    subtitle->setAlignment(Qt::AlignCenter);
    lay->addWidget(subtitle);

    lay->addSpacing(8);

    m_nombreEdit = new QLineEdit(card);
    m_nombreEdit->setPlaceholderText(tr("Nombre"));
    lay->addWidget(m_nombreEdit);

    m_correoEdit = new QLineEdit(card);
    m_correoEdit->setPlaceholderText(tr("Correo electr\u00f3nico"));
    lay->addWidget(m_correoEdit);

    m_contraEdit = new QLineEdit(card);
    m_contraEdit->setPlaceholderText(tr("Contrase\u00f1a"));
    m_contraEdit->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_contraEdit);

    m_errorLabel = new QLabel(card);
    m_errorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    lay->addWidget(m_errorLabel);

    m_registerBtn = new QPushButton(tr("Registrarse"), card);
    m_registerBtn->setObjectName(QStringLiteral("loginBtn"));
    m_registerBtn->setCursor(Qt::PointingHandCursor);
    connect(m_registerBtn, &QPushButton::clicked, this, &RegisterWidget::onRegisterClicked);
    lay->addWidget(m_registerBtn);

    auto* backLink = new QLabel(tr("<a href=\"#\">Iniciar sesi\u00f3n</a>"), card);
    backLink->setObjectName(QStringLiteral("linkLabel"));
    backLink->setAlignment(Qt::AlignCenter);
    backLink->setOpenExternalLinks(false);
    backLink->setCursor(Qt::PointingHandCursor);
    connect(backLink, &QLabel::linkActivated, this, [this]() { emit backToLoginRequested(); });
    lay->addWidget(backLink);

    outer->addWidget(card);
    return wrap;
}

QWidget* RegisterWidget::buildSuccessCard()
{
    auto* wrap = new QWidget(this);
    auto* outer = new QVBoxLayout(wrap);
    outer->setAlignment(Qt::AlignCenter);

    auto* card = new QWidget(wrap);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedWidth(380);
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(36, 40, 36, 36);
    lay->setSpacing(16);

    auto* successTitle = new QLabel(tr("Registro completado."), card);
    successTitle->setObjectName(QStringLiteral("titleLabel"));
    successTitle->setAlignment(Qt::AlignCenter);
    successTitle->setWordWrap(true);
    lay->addWidget(successTitle);

    auto* successSub = new QLabel(tr("Ya puedes iniciar sesi\u00f3n con tu cuenta."), card);
    successSub->setObjectName(QStringLiteral("subtitleLabel"));
    successSub->setAlignment(Qt::AlignCenter);
    lay->addWidget(successSub);

    lay->addSpacing(8);

    auto* loginLink = new QLabel(tr("<a href=\"#\">Iniciar sesi\u00f3n</a>"), card);
    loginLink->setObjectName(QStringLiteral("linkLabel"));
    loginLink->setAlignment(Qt::AlignCenter);
    loginLink->setOpenExternalLinks(false);
    loginLink->setCursor(Qt::PointingHandCursor);
    connect(loginLink, &QLabel::linkActivated, this, [this]() { emit backToLoginRequested(); });
    lay->addWidget(loginLink);

    outer->addWidget(card);
    return wrap;
}

void RegisterWidget::resetForm()
{
    if (m_nombreEdit) m_nombreEdit->clear();
    if (m_correoEdit) m_correoEdit->clear();
    if (m_contraEdit) m_contraEdit->clear();
    if (m_errorLabel) m_errorLabel->hide();
    if (m_stack) m_stack->setCurrentIndex(0);
}

void RegisterWidget::setLoading(bool loading)
{
    m_registerBtn->setEnabled(!loading);
    m_registerBtn->setText(loading ? tr("Registrando...") : tr("Registrarse"));
    m_nombreEdit->setEnabled(!loading);
    m_correoEdit->setEnabled(!loading);
    m_contraEdit->setEnabled(!loading);
}

void RegisterWidget::onRegisterClicked()
{
    QString nombre = m_nombreEdit->text().trimmed();
    QString correo = m_correoEdit->text().trimmed();
    QString contra = m_contraEdit->text();

    if (nombre.isEmpty() || correo.isEmpty() || contra.isEmpty()) {
        m_errorLabel->setText(tr("Rellena nombre, correo y contrase\u00f1a"));
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();
    setLoading(true);
    m_authService->registerUser(nombre, correo, contra);
}

void RegisterWidget::onRegisterSuccess()
{
    setLoading(false);
    m_errorLabel->hide();
    m_stack->setCurrentIndex(1);
}

void RegisterWidget::onRegisterFailed(const QString& error)
{
    setLoading(false);
    m_errorLabel->setText(error.isEmpty() ? tr("Error al registrar (el correo puede estar en uso)") : error);
    m_errorLabel->show();
}
