#include "Theme.hpp"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QStyleFactory>

namespace theme {

QColor blockColor(const QString& type) {
    if (type == "Create")   return QColor(0x16, 0xA3, 0x4A);
    if (type == "Dispose")  return QColor(0xDC, 0x26, 0x26);
    if (type == "Process")  return QColor(0x25, 0x63, 0xEB);
    if (type == "Delay")    return QColor(0xD9, 0x77, 0x06);
    if (type == "Decide")   return QColor(0x7C, 0x3A, 0xED);
    if (type == "Assign" || type == "Record") return QColor(0x08, 0x91, 0xB2);
    if (type == "Batch" || type == "Separate" || type == "Match") return QColor(0xDB, 0x27, 0x77);
    if (type == "Station" || type == "Route") return QColor(0x4D, 0x7C, 0x0F);
    return QColor(0x64, 0x74, 0x8B);
}

namespace {

QPixmap micPixmap(const QColor& c) {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(22, 4, 20, 34), 10, 10);       // the capsule
    QPen pen(c, 5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(13, 12, 38, 38), 180 * 16, 180 * 16);  // the cradle
    p.drawLine(QPointF(32, 50), QPointF(32, 58));           // the stand
    p.drawLine(QPointF(22, 59), QPointF(42, 59));
    return pm;
}

}  // namespace

QIcon micIcon() {
    QIcon icon;
    icon.addPixmap(micPixmap(text), QIcon::Normal, QIcon::Off);
    icon.addPixmap(micPixmap(Qt::white), QIcon::Normal, QIcon::On);
    icon.addPixmap(micPixmap(Qt::white), QIcon::Active, QIcon::On);
    icon.addPixmap(micPixmap(border), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(micPixmap(Qt::white), QIcon::Disabled, QIcon::On);
    return icon;
}

void apply(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window, background);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, surface);
    p.setColor(QPalette::AlternateBase, background);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, surface);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, text);
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Mid, border);
    app.setPalette(p);

    app.setStyleSheet(QString(R"(
        QMainWindow::separator { background: %1; width: 1px; height: 1px; }
        QToolBar { background: %2; border-bottom: 1px solid %1; spacing: 4px; padding: 4px 6px; }
        QToolBar QToolButton { padding: 5px 9px; border-radius: 6px; }
        QToolBar QToolButton:hover { background: %3; }
        QToolBar QToolButton:checked { background: %3; color: %4; }
        QDockWidget { font-weight: 600; }
        QDockWidget::title { background: %5; padding: 6px 8px; border-bottom: 1px solid %1; }
        QTabWidget::pane { border: 1px solid %1; border-radius: 6px; background: %2; top: -1px; }
        QTabBar::tab { padding: 7px 16px; margin-right: 2px; border: 1px solid transparent;
                       border-top-left-radius: 6px; border-top-right-radius: 6px; color: %6; }
        QTabBar::tab:selected { background: %2; border-color: %1; border-bottom-color: %2; color: %7; }
        QTabBar::tab:hover:!selected { color: %7; }
        QPushButton { padding: 6px 14px; border: 1px solid %1; border-radius: 6px; background: %2; }
        QPushButton:hover { border-color: %4; }
        QPushButton#primary { background: %4; color: white; border-color: %4; font-weight: 600; }
        QPushButton#primary:disabled { background: %3; border-color: %3; }
        QPushButton#record { padding: 6px 10px; }
        QPushButton#record:checked { background: %8; color: white; border-color: %8; }
        QToolBar QLabel { color: %6; padding-left: 6px; }
        QToolBar QLineEdit, QToolBar QComboBox { padding: 3px 6px; }
        QPlainTextEdit, QTextBrowser, QTreeWidget, QListWidget, QLineEdit, QComboBox {
            border: 1px solid %1; border-radius: 6px; background: %2; selection-background-color: %4; }
        QLineEdit, QComboBox { padding: 4px 6px; }
        QHeaderView::section { background: %5; border: none; border-bottom: 1px solid %1; padding: 4px 6px; }
        QStatusBar { background: %2; border-top: 1px solid %1; }
        QProgressBar { border: 1px solid %1; border-radius: 5px; background: %5; text-align: center; height: 14px; }
        QProgressBar::chunk { background: %4; border-radius: 4px; }
        QToolTip { padding: 6px; border: none; }
    )")
                         .arg(border.name(), surface.name(), accentSoft.name(), accent.name(),
                              background.name(), muted.name(), text.name(), error.name()));
}

}  // namespace theme
