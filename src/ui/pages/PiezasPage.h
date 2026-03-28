#pragma once

#include "core/PiezaCardData.h"
#include "models/Propietario.h"
#include "models/Ubicacion.h"
#include "models/Tipo.h"
#include <functional>
#include <QVector>
#include <QWidget>

class SupabaseClient;
class HistorialService;
class PiezaService;
class PropietarioService;
class UbicacionService;
class TipoService;
class LoadingOverlay;
class QScrollArea;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTimer;

class PiezasPage : public QWidget
{
    Q_OBJECT
public:
    explicit PiezasPage(SupabaseClient* supabase, HistorialService* historial,
                        QWidget* parent = nullptr);

private slots:
    void refreshCards();
    void onAdd();
    void onEdit(int piezaId);
    void onDelete(int piezaId);
    void onDuplicate(int piezaId);
    void onCardSelected(int piezaId);
    void onHoverTimerFired();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void rebuildGrid();
    QWidget* createCard(QWidget* parent, const PiezaCardData& data, int index);
    PiezaListParams currentParams() const;
    void updateSelectionStyle();
    void showHoverPopup(const PiezaCardData& data, const QPoint& globalPos);
    void hideHoverPopup();
    void withTiposRefreshed(std::function<void()> body);

    SupabaseClient* m_supabase;
    PiezaService* m_piezaService;
    PropietarioService* m_propService;
    UbicacionService* m_ubicService;
    TipoService* m_tipoService;
    LoadingOverlay* m_overlay;
    QScrollArea* m_scroll;
    QWidget* m_cardsContainer;
    QLabel* m_countLabel;
    QLineEdit* m_searchEdit;
    QComboBox* m_tipoFilterCombo;
    QComboBox* m_propietarioFilterCombo;
    QComboBox* m_ubicacionFilterCombo;
    QComboBox* m_sortCombo;
    QComboBox* m_sortOrderCombo;
    QPushButton* m_editBtn;
    QPushButton* m_dupBtn;
    QPushButton* m_delBtn;
    QWidget* m_hoverPopup;
    QTimer* m_hoverTimer;
    QWidget* m_hoverCard;
    int m_selectedId = 0;
    QVector<PiezaCardData> m_cardsData;
    QVector<Propietario> m_lastPropietarios;
    QVector<Ubicacion> m_lastUbicaciones;
    QVector<Tipo> m_lastTipos;
};
