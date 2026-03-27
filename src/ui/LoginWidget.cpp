#include "LoginWidget.h"
#include "RegisterWidget.h"
#include "core/AuthService.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>

LoginWidget::LoginWidget(AuthService* authService, QWidget* parent)
    : QWidget(parent)
    , m_authService(authService)
    , m_stack(nullptr)
    , m_registerWidget(nullptr)
    , m_correoEdit(nullptr)
    , m_contraEdit(nullptr)
    , m_rememberCheck(nullptr)
    , m_errorLabel(nullptr)
    , m_loginBtn(nullptr)
{
    setObjectName(QStringLiteral("loginWidget"));
    setStyleSheet(QStringLiteral(R"(
        #loginWidget { background-color: #f0f2f5; }
        #loginCard {
            background-color: white;
            border-radius: 12px;
            border: 1px solid #e4e7ed;
        }
        QLineEdit {
            padding: 10px 14px;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            font-size: 13px;
            background-color: #fafafa;
            min-height: 20px;
        }
        QLineEdit:focus {
            border: 1px solid #409eff;
            background-color: white;
        }
        #loginBtn {
            background-color: #409eff;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px;
            font-size: 14px;
            font-weight: bold;
            min-height: 20px;
        }
        #loginBtn:hover  { background-color: #66b1ff; }
        #loginBtn:pressed { background-color: #3a8ee6; }
        #loginBtn:disabled { background-color: #a0cfff; }
        #titleLabel { font-size: 24px; font-weight: bold; color: #2c3e50; }
        #subtitleLabel { font-size: 13px; color: #909399; }
        #errorLabel { color: #f56c6c; font-size: 12px; }
        #linkLabel { font-size: 13px; }
        #linkLabel a { color: #409eff; text-decoration: none; }
        QCheckBox { font-size: 12px; color: #606266; }
    )"));

    auto* outer = new QVBoxLayout(this);
    outer->setAlignment(Qt::AlignCenter);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildLoginCard());
    m_registerWidget = new RegisterWidget(m_authService, this);
    m_stack->addWidget(m_registerWidget);
    outer->addWidget(m_stack, 1);

    connect(m_registerWidget, &RegisterWidget::backToLoginRequested, this, &LoginWidget::showLoginView);
    connect(m_contraEdit, &QLineEdit::returnPressed, this, &LoginWidget::onLoginClicked);
    connect(m_correoEdit, &QLineEdit::returnPressed, m_contraEdit, qOverload<>(&QWidget::setFocus));

    connect(m_authService, &AuthService::loginSuccess, this, &LoginWidget::onLoginSuccess);
    connect(m_authService, &AuthService::loginFailed, this, &LoginWidget::onLoginFailed);

    loadSavedAuth();
    tryAutoLogin();
}

QWidget* LoginWidget::buildLoginCard()
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

    auto* subtitle = new QLabel(tr("Inicia sesi\u00f3n para continuar"), card);
    subtitle->setObjectName(QStringLiteral("subtitleLabel"));
    subtitle->setAlignment(Qt::AlignCenter);
    lay->addWidget(subtitle);

    lay->addSpacing(8);

    m_correoEdit = new QLineEdit(card);
    m_correoEdit->setPlaceholderText(tr("Correo electr\u00f3nico"));
    lay->addWidget(m_correoEdit);

    m_contraEdit = new QLineEdit(card);
    m_contraEdit->setPlaceholderText(tr("Contrase\u00f1a"));
    m_contraEdit->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_contraEdit);

    m_rememberCheck = new QCheckBox(tr("Iniciar sesión automáticamente"), card);
    lay->addWidget(m_rememberCheck);

    m_errorLabel = new QLabel(card);
    m_errorLabel->setObjectName(QStringLiteral("errorLabel"));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    lay->addWidget(m_errorLabel);

    m_loginBtn = new QPushButton(tr("Iniciar sesi\u00f3n"), card);
    m_loginBtn->setObjectName(QStringLiteral("loginBtn"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    lay->addWidget(m_loginBtn);

    auto* regLink = new QLabel(tr("<a href=\"#\">Registrarse</a>"), card);
    regLink->setObjectName(QStringLiteral("linkLabel"));
    regLink->setAlignment(Qt::AlignCenter);
    regLink->setOpenExternalLinks(false);
    regLink->setCursor(Qt::PointingHandCursor);
    connect(regLink, &QLabel::linkActivated, this, &LoginWidget::showRegisterView);
    lay->addWidget(regLink);

    outer->addWidget(card);
    return wrap;
}

void LoginWidget::showRegisterView()
{
    if (m_registerWidget) m_registerWidget->resetForm();
    if (m_stack) m_stack->setCurrentIndex(1);
}

void LoginWidget::showLoginView()
{
    if (m_stack) m_stack->setCurrentIndex(0);
}

void LoginWidget::setLoading(bool loading)
{
    m_loginBtn->setEnabled(!loading);
    m_loginBtn->setText(loading ? tr("Iniciando sesi\u00f3n...") : tr("Iniciar sesi\u00f3n"));
    m_correoEdit->setEnabled(!loading);
    m_contraEdit->setEnabled(!loading);
}

void LoginWidget::onLoginClicked()
{
    QString correo = m_correoEdit->text().trimmed();
    QString contra = m_contraEdit->text();

    if (correo.isEmpty() || contra.isEmpty()) {
        m_errorLabel->setText(tr("Introduce correo y contrase\u00f1a"));
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();
    setLoading(true);
    m_authService->login(correo, contra);
}

void LoginWidget::onLoginSuccess(const Usuario& user)
{
    m_autoLoginInProgress = false;
    setLoading(false);
    if (m_rememberCheck->isChecked())
        saveAuthData();
    else
        clearSavedAuth();
    emit loginSuccessful(user);
}

void LoginWidget::onLoginFailed(const QString& error)
{
    if (m_autoLoginInProgress) {
        m_autoLoginInProgress = false;
        setLoading(false);
        QSettings settings(QStringLiteral("BetlemSystem"), QStringLiteral("BelenCreator"));
        m_contraEdit->setText(settings.value(QStringLiteral("auth/contra")).toString());
        m_errorLabel->setText(error.isEmpty() ? tr("No se pudo iniciar sesi\u00f3n autom\u00e1ticamente") : error);
        m_errorLabel->show();
        return;
    }

    setLoading(false);
    m_errorLabel->setText(error.isEmpty() ? tr("Credenciales incorrectas") : error);
    m_errorLabel->show();
}

void LoginWidget::loadSavedAuth()
{
    QSettings settings(QStringLiteral("BetlemSystem"), QStringLiteral("BelenCreator"));
    if (settings.value(QStringLiteral("auth/auto_login"), false).toBool()) {
        m_correoEdit->setText(settings.value(QStringLiteral("auth/correo")).toString());
        m_rememberCheck->setChecked(true);
    }
}

void LoginWidget::saveAuthData()
{
    QSettings settings(QStringLiteral("BetlemSystem"), QStringLiteral("BelenCreator"));
    settings.setValue(QStringLiteral("auth/auto_login"), true);
    settings.setValue(QStringLiteral("auth/correo"), m_correoEdit->text().trimmed());
    // Tras login automático el campo contraseña sigue vacío: no sobrescribir lo ya guardado.
    const QString contra = m_contraEdit->text();
    if (!contra.isEmpty())
        settings.setValue(QStringLiteral("auth/contra"), contra);
}

void LoginWidget::clearSavedAuth()
{
    QSettings settings(QStringLiteral("BetlemSystem"), QStringLiteral("BelenCreator"));
    settings.remove(QStringLiteral("auth/auto_login"));
    settings.remove(QStringLiteral("auth/correo"));
    settings.remove(QStringLiteral("auth/contra"));
}

void LoginWidget::tryAutoLogin()
{
    QSettings settings(QStringLiteral("BetlemSystem"), QStringLiteral("BelenCreator"));
    const bool enabled = settings.value(QStringLiteral("auth/auto_login"), false).toBool();
    const QString correo = settings.value(QStringLiteral("auth/correo")).toString().trimmed();
    const QString contra = settings.value(QStringLiteral("auth/contra")).toString();

    if (!enabled || correo.isEmpty() || contra.isEmpty())
        return;

    m_autoLoginInProgress = true;
    m_errorLabel->hide();
    setLoading(true);
    m_authService->login(correo, contra);
}
