#include "ModelEditor.hpp"

#include <QHelpEvent>
#include <QPainter>
#include <QTextBlock>
#include <QToolTip>
#include "DesHighlighter.hpp"
#include "Theme.hpp"

namespace {

class Gutter : public QWidget {
public:
    explicit Gutter(ModelEditor* editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->gutterWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* e) override { m_editor->paintGutter(e); }

private:
    ModelEditor* m_editor;
};

}  // namespace

ModelEditor::ModelEditor(QWidget* parent)
    : QPlainTextEdit(parent), m_gutter(new Gutter(this)), m_highlighter(new DesHighlighter(document())) {
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setFamilies({"Consolas", "Menlo", "DejaVu Sans Mono", "monospace"});
    mono.setPointSize(11);
    setFont(mono);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));
    setFrameShape(QFrame::NoFrame);
    setPlaceholderText("Describe a model in the panel on the right, insert modules from the left,\n"
                       "or use Model > Insert starter model.");
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { updateGutterWidth(); });
    connect(this, &QPlainTextEdit::updateRequest, this, &ModelEditor::updateGutter);
    updateGutterWidth();
}

int ModelEditor::gutterWidth() const {
    int digits = 1;
    for (int n = qMax(1, blockCount()); n >= 10; n /= 10) ++digits;
    return 22 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * qMax(3, digits);
}

void ModelEditor::updateGutterWidth() { setViewportMargins(gutterWidth(), 0, 0, 0); }

void ModelEditor::updateGutter(const QRect& rect, int dy) {
    if (dy != 0) m_gutter->scroll(0, dy);
    else         m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
}

void ModelEditor::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    const QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
}

void ModelEditor::paintGutter(QPaintEvent* event) {
    QPainter p(m_gutter);
    p.fillRect(event->rect(), theme::background);
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    const int h = fontMetrics().height();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible()) {
            const int line = block.blockNumber() + 1;
            const auto it = m_marks.constFind(line);
            if (it != m_marks.constEnd()) {
                p.setPen(Qt::NoPen);
                p.setBrush(it->error ? theme::error : theme::warning);
                p.drawEllipse(QPointF(8, top + h / 2.0), 3.5, 3.5);
            }
            p.setPen(it != m_marks.constEnd() ? (it->error ? theme::error : theme::warning) : theme::muted);
            p.drawText(0, top, m_gutter->width() - 6, h, Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(line));
        }
        block = block.next();
        top += qRound(blockBoundingRect(block).height());
    }
}

void ModelEditor::setMarks(const QMap<int, Mark>& marks) {
    m_marks = marks;
    QList<QTextEdit::ExtraSelection> sels;
    for (auto it = marks.constBegin(); it != marks.constEnd(); ++it) {
        const QTextBlock b = document()->findBlockByNumber(it.key() - 1);
        if (!b.isValid()) continue;
        QTextEdit::ExtraSelection s;
        s.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        s.format.setUnderlineColor(it->error ? theme::error : theme::warning);
        s.cursor = QTextCursor(b);
        s.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        sels.push_back(s);
    }
    setExtraSelections(sels);
    m_gutter->update();
}

void ModelEditor::goToLine(int line) {
    const QTextBlock b = document()->findBlockByNumber(qMax(0, line - 1));
    if (!b.isValid()) return;
    QTextCursor c(b);
    setTextCursor(c);
    centerCursor();
    setFocus();
}

void ModelEditor::replaceAll(const QString& text) {
    QTextCursor c(document());
    c.beginEditBlock();
    c.select(QTextCursor::Document);
    c.insertText(text);
    c.endEditBlock();
    moveCursor(QTextCursor::Start);
}

bool ModelEditor::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(e);
        const QPoint inViewport = viewport()->mapFromGlobal(help->globalPos());
        const int line = cursorForPosition(inViewport).blockNumber() + 1;
        const auto it = m_marks.constFind(line);
        if (it != m_marks.constEnd()) QToolTip::showText(help->globalPos(), it->message, this);
        else                          QToolTip::hideText();
        return true;
    }
    return QPlainTextEdit::event(e);
}
