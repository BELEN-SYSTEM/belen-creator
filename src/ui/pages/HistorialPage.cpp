#include "HistorialPage.h"
#include "core/HistorialService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QDateTime>
#include <QLocale>
#include <QColor>
#include <QBrush>

HistorialPage::HistorialPage(HistorialService* historial, QWidget* parent)
    : QWidget(parent)
    , m_historial(historial)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(tr("Historial"), this);
    title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold; color: #2c3e50;"));
    layout->addWidget(title);

    auto* desc = new QLabel(tr("Registro de cambios en la base de datos"), this);
    desc->setStyleSheet(QStringLiteral("font-size: 13px; color: #7f8c8d;"));
    layout->addWidget(desc);
    layout->addSpacing(12);

    auto* bar = new QHBoxLayout();
    bar->addWidget(new QLabel(tr("Tabla:"), this));
    m_filterCombo = new QComboBox(this);
    const auto opts = HistorialService::opcionesFiltroTabla();
    for (const auto& p : opts)
        m_filterCombo->addItem(p.second, p.first);
    m_filterCombo->setMinimumWidth(180);
    bar->addWidget(m_filterCombo);

    bar->addSpacing(16);
    bar->addWidget(new QLabel(tr("Usuario:"), this));
    m_userFilterCombo = new QComboBox(this);
    m_userFilterCombo->addItem(tr("Todos los usuarios"), QString());
    m_userFilterCombo->setMinimumWidth(200);
    bar->addWidget(m_userFilterCombo);

    auto* refreshBtn = new QPushButton(tr("Actualizar"), this);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    bar->addWidget(refreshBtn);
    bar->addStretch(1);
    layout->addLayout(bar);
    layout->addSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        { tr("Id"), tr("Fecha"), tr("Secci\u00f3n"), tr("Usuario"), tr("Descripci\u00f3n") });
    m_table->setColumnHidden(0, true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setWordWrap(true);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { gridline-color: #e8e8e8; outline: none; }"
        "QTableWidget::item { padding: 10px 14px; color: #1a1a1a; border: none; outline: none; }"
        "QTableWidget::item:hover { background-color: #f0f0f0; border: none; outline: none; }"
        "QTableWidget::item:selected { background-color: #e4e4e4; color: #1a1a1a; border: none; outline: none; }"
        "QTableWidget::item:selected:active { background-color: #dcdcdc; border: none; outline: none; }"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QHeaderView::section { padding: 10px 14px; background-color: #ecf0f1; color: #2c3e50; }"));
    layout->addWidget(m_table, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    layout->addWidget(m_statusLabel);

    connect(refreshBtn, &QPushButton::clicked, this, &HistorialPage::reload);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistorialPage::reload);
    connect(m_userFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistorialPage::reload);

    connect(m_historial, &HistorialService::dataReady, this, [this](const QVector<HistorialEntry>& items) {
        m_table->setRowCount(items.size());
        for (int i = 0; i < items.size(); ++i) {
            const HistorialEntry& e = items[i];
            QColor bg;
            switch (e.tipoCambio) {
            case HistorialTipoCambio::Alta:
                bg = QColor(214, 242, 214);
                break;
            case HistorialTipoCambio::Edicion:
                bg = QColor(255, 232, 209);
                break;
            case HistorialTipoCambio::Baja:
                bg = QColor(255, 218, 218);
                break;
            }
            const QBrush brush(bg);

            auto* idIt = new QTableWidgetItem(QString::number(e.id));
            idIt->setBackground(brush);
            m_table->setItem(i, 0, idIt);

            QString fechaTxt;
            if (e.fecha.isValid()) {
                const QDateTime local = e.fecha.toLocalTime();
                fechaTxt = QLocale::system().toString(local, QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            } else {
                fechaTxt = QStringLiteral("\u2014");
            }
            auto* fIt = new QTableWidgetItem(fechaTxt);
            fIt->setBackground(brush);
            m_table->setItem(i, 1, fIt);

            auto* secIt = new QTableWidgetItem(HistorialService::etiquetaTablaUsuario(e.tabla));
            secIt->setBackground(brush);
            m_table->setItem(i, 2, secIt);

            const QString uet = e.usuarioEtiqueta.isEmpty() ? QStringLiteral("\u2014") : e.usuarioEtiqueta;
            auto* uIt = new QTableWidgetItem(uet);
            uIt->setBackground(brush);
            m_table->setItem(i, 3, uIt);

            auto* tIt = new QTableWidgetItem(e.texto);
            tIt->setBackground(brush);
            m_table->setItem(i, 4, tIt);
        }
        m_statusLabel->setText(tr("%1 entradas").arg(items.size()));
    });
    connect(m_historial, &HistorialService::errorOccurred, this, [this](const QString& msg) {
        m_statusLabel->setText(tr("Error: %1").arg(msg));
    });

    connect(m_historial, &HistorialService::usuariosFiltroReady, this,
            [this](const QVector<QPair<QString, QString>>& pairs) {
                const QSignalBlocker b(m_userFilterCombo);
                m_userFilterCombo->clear();
                m_userFilterCombo->addItem(tr("Todos los usuarios"), QString());
                for (const auto& p : pairs)
                    m_userFilterCombo->addItem(p.second, p.first);
            });

    m_historial->fetchUsuariosParaFiltro();
    reload();
}

void HistorialPage::reload()
{
    m_statusLabel->setText(tr("Cargando\u2026"));
    const QString filtroTabla = m_filterCombo->currentData().toString();
    const QString filtroUsuario = m_userFilterCombo->currentData().toString();
    m_historial->fetchAll(filtroTabla, filtroUsuario);
}
