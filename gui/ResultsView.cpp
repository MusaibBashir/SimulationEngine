#include "ResultsView.hpp"

#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include "Theme.hpp"

void UtilisationChart::setBars(std::vector<std::pair<QString, double>> bars) {
    m_bars = std::move(bars);
    updateGeometry();
    update();
}

QSize UtilisationChart::sizeHint() const {
    return QSize(400, 24 + static_cast<int>(m_bars.size()) * 30);
}

void UtilisationChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_bars.empty()) return;
    const int labelW = 150, right = 70, rowH = 30;
    const int barW = qMax(50, width() - labelW - right - 16);
    int y = 8;
    for (const auto& [name, u] : m_bars) {
        p.setPen(theme::text);
        p.drawText(QRect(8, y, labelW - 12, rowH - 8), Qt::AlignVCenter | Qt::AlignRight,
                   fontMetrics().elidedText(name, Qt::ElideRight, labelW - 12));
        const QRectF track(labelW, y + 4, barW, rowH - 16);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::background);
        p.drawRoundedRect(track, 4, 4);
        // Green while there is slack, amber past 80% where queues start to
        // grow quickly, red past 95% where they explode.
        const QColor c = u >= 0.95 ? theme::error : u >= 0.80 ? theme::warning : theme::success;
        p.setBrush(c);
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * qBound(0.0, u, 1.0), track.height()), 4, 4);
        p.setPen(theme::text);
        p.drawText(QRect(labelW + barW + 8, y, right, rowH - 8), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::number(u * 100.0, 'f', 1) + "%");
        y += rowH;
    }
}

ResultsView::ResultsView(QWidget* parent)
    : QWidget(parent), m_title(new QLabel(this)), m_chart(new UtilisationChart(this)),
      m_report(new QPlainTextEdit(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);
    QFont big = m_title->font();
    big.setPointSizeF(big.pointSizeF() * 1.25);
    big.setBold(true);
    m_title->setFont(big);
    layout->addWidget(m_title);
    auto* busy = new QLabel("How busy each station was", this);
    busy->setStyleSheet(QString("color: %1;").arg(theme::muted.name()));
    layout->addWidget(busy);
    layout->addWidget(m_chart);
    m_report->setReadOnly(true);
    QFont mono("Consolas");
    mono.setFamilies({"Consolas", "Menlo", "DejaVu Sans Mono", "monospace"});
    mono.setStyleHint(QFont::Monospace);
    m_report->setFont(mono);
    m_report->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_report, 1);
    clear();
}

void ResultsView::showResults(const QString& title, const QString& report,
                              std::vector<std::pair<QString, double>> utilisation) {
    m_title->setText(title);
    m_chart->setBars(std::move(utilisation));
    m_report->setPlainText(report);
}

void ResultsView::clear() {
    m_title->setText("No results yet");
    m_chart->setBars({});
    m_report->setPlainText("Press Run (F5) to run the model. The report appears here.");
}

QString ResultsView::report() const { return m_report->toPlainText(); }
