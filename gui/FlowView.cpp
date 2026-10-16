#include "FlowView.hpp"

#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QMap>
#include <QMouseEvent>
#include <QQueue>
#include <QWheelEvent>
#include "Theme.hpp"
#include "TuiState.hpp"   // flowOf()

namespace {

constexpr double kW = 170, kH = 54, kGapX = 40, kGapY = 46;
constexpr int    kLineRole = 1;

struct Exit { QString label; QString target; };

std::vector<Exit> exitsOf(const des::FlowBlock& b) {
    std::vector<Exit> out;
    for (const std::string& e : b.exits) {
        const QString s = QString::fromStdString(e);
        const int arrow = static_cast<int>(s.indexOf(" -> "));
        if (arrow < 0) continue;
        QString label = s.left(arrow);
        if (label == "Next" || label == "to") label.clear();
        out.push_back({label, s.mid(arrow + 4)});
    }
    return out;
}

}  // namespace

FlowView::FlowView(QWidget* parent) : QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setBackgroundBrush(theme::background);
    setFrameShape(QFrame::NoFrame);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void FlowView::showModel(const des::ModelDocument& doc) {
    QGraphicsScene* s = scene();
    s->clear();
    const std::vector<des::FlowBlock> blocks = des::flowOf(doc);
    if (blocks.empty()) {
        auto* t = s->addSimpleText("No flowchart blocks yet. Insert a Create from the Modules list,\n"
                                   "or describe a model in the panel on the right.");
        t->setBrush(theme::muted);
        s->setSceneRect(s->itemsBoundingRect().adjusted(-40, -40, 40, 40));
        return;
    }

    QMap<QString, int> index;
    for (std::size_t i = 0; i < blocks.size(); ++i) index[QString::fromStdString(blocks[i].name)] = static_cast<int>(i);

    // Columns: shortest distance from an arrival (a Create, or anything
    // nothing points at). What cannot be reached goes in column 0, last.
    std::vector<int> column(blocks.size(), -1);
    std::vector<int> incoming(blocks.size(), 0);
    for (const auto& b : blocks)
        for (const Exit& e : exitsOf(b))
            if (index.contains(e.target)) ++incoming[static_cast<std::size_t>(index[e.target])];
    QQueue<int> q;
    for (std::size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i].type == "Create" || incoming[i] == 0) { column[i] = 0; q.enqueue(static_cast<int>(i)); }
    while (!q.isEmpty()) {
        const int i = q.dequeue();
        for (const Exit& e : exitsOf(blocks[static_cast<std::size_t>(i)])) {
            if (!index.contains(e.target)) continue;
            const int j = index[e.target];
            if (column[static_cast<std::size_t>(j)] < 0) {
                column[static_cast<std::size_t>(j)] = column[static_cast<std::size_t>(i)] + 1;
                q.enqueue(j);
            }
        }
    }
    for (int& c : column) if (c < 0) c = 0;

    // Top to bottom: a model is read downwards, and the pane is taller than
    // it is wide once the Modules and Describe panels take their share.
    std::vector<QPointF> pos(blocks.size());
    QMap<int, int> filled;
    int widest = 1;
    for (std::size_t i = 0; i < blocks.size(); ++i) widest = qMax(widest, ++filled[column[i]]);
    QMap<int, int> placed;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const int row = column[i];
        const int k = placed[row]++;
        const double rowWidth = filled[row] * kW + (filled[row] - 1) * kGapX;
        const double fullWidth = widest * kW + (widest - 1) * kGapX;
        pos[i] = QPointF((fullWidth - rowWidth) / 2 + k * (kW + kGapX), row * (kH + kGapY));
    }

    // Arrows first, so the boxes sit on top of them.
    QFont small = font();
    small.setPointSizeF(small.pointSizeF() * 0.85);
    const double rightEdge = widest * kW + (widest - 1) * kGapX;
    int loops = 0;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        for (const Exit& e : exitsOf(blocks[i])) {
            const bool known = index.contains(e.target);
            QPointF labelAt;
            if (known) {
                const std::size_t j = static_cast<std::size_t>(index[e.target]);
                QPainterPath path;
                QPointF to, dir;
                if (qFuzzyCompare(pos[j].y() + 1, pos[i].y() + 1) && pos[j].x() > pos[i].x()) {
                    // Side by side: straight across.
                    const QPointF from = pos[i] + QPointF(kW, kH / 2);
                    to = pos[j] + QPointF(0, kH / 2);
                    path.moveTo(from);
                    path.lineTo(to);
                    labelAt = QPointF((from.x() + to.x()) / 2 - 10, from.y() - 18);
                    dir = QPointF(1, 0);
                } else if (pos[j].y() > pos[i].y()) {
                    const QPointF from = pos[i] + QPointF(kW / 2, kH);
                    to = pos[j] + QPointF(kW / 2, 0);
                    const double mid = (from.y() + to.y()) / 2;
                    path.moveTo(from);
                    path.cubicTo(QPointF(from.x(), mid), QPointF(to.x(), mid), to);
                    labelAt = QPointF((from.x() + to.x()) / 2 + 6, mid - 9);
                    dir = QPointF(0, 1);
                } else {
                    // Back up the page -- rework, a loop -- around the right of everything.
                    const QPointF from = pos[i] + QPointF(kW, kH / 2);
                    to = pos[j] + QPointF(kW, kH / 2);
                    const double lane = rightEdge + 30 + 18 * (loops++ % 4);
                    path.moveTo(from);
                    path.lineTo(lane - 8, from.y());
                    path.quadTo(lane, from.y(), lane, from.y() - 8);
                    path.lineTo(lane, to.y() + 8);
                    path.quadTo(lane, to.y(), lane - 8, to.y());
                    path.lineTo(to);
                    labelAt = QPointF(lane + 4, (from.y() + to.y()) / 2);
                    dir = QPointF(-1, 0);
                }
                auto* edge = s->addPath(path, QPen(theme::muted, 1.6));
                edge->setZValue(-1);
                const QPointF side(-dir.y(), dir.x());
                QPolygonF head;
                head << to << to - dir * 9 + side * 4.5 << to - dir * 9 - side * 4.5;
                s->addPolygon(head, Qt::NoPen, theme::muted);
            } else {
                // A Next naming nothing that exists: a red stub, labelled.
                const QPointF from = pos[i] + QPointF(kW / 2, kH);
                QPainterPath path(from);
                path.lineTo(from + QPointF(0, 20));
                s->addPath(path, QPen(theme::error, 1.6, Qt::DashLine));
                auto* t = s->addSimpleText("? " + e.target);
                t->setBrush(theme::error);
                t->setFont(small);
                t->setPos(from + QPointF(6, 6));
            }
            if (!e.label.isEmpty() && known) {
                auto* t = s->addSimpleText(e.label);
                t->setFont(small);
                t->setBrush(QColor(0x7C, 0x3A, 0xED));
                t->setPos(labelAt);
            }
        }
    }

    QFont bold = font();
    bold.setBold(true);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        const QString type = QString::fromStdString(b.type);
        const QColor c = theme::blockColor(type);
        auto* box = s->addRect(QRectF(pos[i], QSizeF(kW, kH)), QPen(b.unreached ? theme::warning : theme::border, 1.2),
                               theme::surface);
        box->setData(kLineRole, static_cast<int>(b.line));
        box->setCursor(Qt::PointingHandCursor);
        box->setToolTip(type + " " + QString::fromStdString(b.name) +
                        (b.unreached ? "\nNothing routes here." : "") + "\nClick to open it in the text.");
        s->addRect(QRectF(pos[i], QSizeF(6, kH)), Qt::NoPen, c)->setData(kLineRole, static_cast<int>(b.line));
        auto* name = s->addSimpleText(QString::fromStdString(b.name), bold);
        name->setBrush(theme::text);
        name->setPos(pos[i] + QPointF(16, 10));
        name->setData(kLineRole, static_cast<int>(b.line));
        auto* kind = s->addSimpleText(type, small);
        kind->setBrush(c);
        kind->setPos(pos[i] + QPointF(16, 32));
        kind->setData(kLineRole, static_cast<int>(b.line));
    }
    s->setSceneRect(s->itemsBoundingRect().adjusted(-40, -30, 40, 30));
    if (!m_zoomed) fitToView();
}

void FlowView::fitToView() {
    // The whole model at once, but never magnified past its natural size.
    const QRectF r = sceneRect();
    if (r.isEmpty() || viewport()->width() < 50) return;
    const double k = qMin(1.0, qMin(viewport()->width() / r.width(), viewport()->height() / r.height()));
    resetTransform();
    scale(k, k);
    centerOn(r.center());
}

void FlowView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    if (!m_zoomed) fitToView();
}

void FlowView::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        const double f = e->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
        scale(f, f);
        m_zoomed = true;   // the person chose a zoom; stop fitting over it
        return;
    }
    QGraphicsView::wheelEvent(e);
}

void FlowView::mousePressEvent(QMouseEvent* e) {
    if (QGraphicsItem* it = itemAt(e->pos()); it != nullptr && it->data(kLineRole).isValid()) {
        const int line = it->data(kLineRole).toInt();
        if (line > 0) Q_EMIT blockActivated(line);
        return;
    }
    QGraphicsView::mousePressEvent(e);
}

void FlowView::mouseDoubleClickEvent(QMouseEvent* e) { mousePressEvent(e); }
