// ============================================================================
// gui/ResultsView.hpp  --  a run's report, and how busy everything was
// ============================================================================
// The report is the engine's own text, unchanged -- the same words `des run`
// prints, so a result can be compared with a colleague's run of the CLI. Above
// it, one bar per station or resource: utilisation is the first number anyone
// looks for, and a bar near 100% says "this is the bottleneck" faster than a
// column of decimals.

#pragma once
#include <QWidget>
#include <utility>
#include <vector>

class QLabel;
class QPlainTextEdit;

class UtilisationChart : public QWidget {
public:
    explicit UtilisationChart(QWidget* parent = nullptr) : QWidget(parent) {}
    void setBars(std::vector<std::pair<QString, double>> bars);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    std::vector<std::pair<QString, double>> m_bars;
};

class ResultsView : public QWidget {
public:
    explicit ResultsView(QWidget* parent = nullptr);
    void showResults(const QString& title, const QString& report,
                     std::vector<std::pair<QString, double>> utilisation);
    void clear();
    QString report() const;

private:
    QLabel*           m_title;
    UtilisationChart* m_chart;
    QPlainTextEdit*   m_report;
};
