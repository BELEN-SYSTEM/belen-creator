#pragma once

#include <QWidget>
#include "../models/Usuario.h"

class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class AuthService;
class RegisterWidget;

class LoginWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWidget(AuthService* authService, QWidget* parent = nullptr);

signals:
    void loginSuccessful(const Usuario& user);

private slots:
    void onLoginClicked();
    void onLoginSuccess(const Usuario& user);
    void onLoginFailed(const QString& error);
    void showRegisterView();
    void showLoginView();

private:
    void loadSavedAuth();
    void saveAuthData();
    void clearSavedAuth();
    void tryAutoLogin();
    void setLoading(bool loading);
    QWidget* buildLoginCard();

    AuthService* m_authService;
    QStackedWidget* m_stack;
    RegisterWidget* m_registerWidget;
    QLineEdit* m_correoEdit;
    QLineEdit* m_contraEdit;
    QCheckBox* m_rememberCheck;
    QLabel* m_errorLabel;
    QPushButton* m_loginBtn;
    bool m_autoLoginInProgress = false;
};
