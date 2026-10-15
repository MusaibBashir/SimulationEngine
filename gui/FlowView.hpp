// ============================================================================
// gui/FlowView.hpp  --  the model drawn as boxes and arrows
// ============================================================================
// Built from flowOf(), the same list the terminal's Flow tab uses, so the two
// cannot disagree about the wiring. Laid out in columns by distance from the
// arrivals; an arrow back to an earlier column -- rework, a loop -- curves
// underneath. Clicking a box asks for its record in the text.

#pragma once
#include <QGraphicsView>
#include "ModelDocument.hpp"

class FlowView : public QGraphicsView {
    Q_OBJECT
public:
    explicit FlowView(QWidget* parent = nullptr);
    void showModel(const des::ModelDocument& doc);
    // Ctrl+wheel zooms; until then the whole model is fitted to the pane.
    void fitToView();

Q_SIGNALS:
    void blockActivated(int line);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    bool m_zoomed{false};
};
