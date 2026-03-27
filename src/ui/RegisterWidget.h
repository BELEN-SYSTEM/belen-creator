#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class AuthService;

class RegisterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RegisterWidget(AuthService* authService, QWidget* parent = nullptr);
    void resetForm();

signals:
    void backToLoginRequested();

private slots:
    void onRegisterClicked();
    void onRegisterSuccess();
    void onRegisterFailed(const QString& error);

private:
    void setLoading(bool loading);
    QWidget* buildFormCard();
    QWidget* buildSuccessCard();

    AuthService* m_authService;
    QStackedWidget* m_stack;
    QLineEdit* m_nombreEdit;
    QLineEdit* m_correoEdit;
    QLineEdit* m_contraEdit;
    QLabel* m_errorLabel;
    QPushButton* m_registerBtn;
};
