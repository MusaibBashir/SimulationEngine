// ============================================================================
// gui/ModelEditor.hpp  --  the .des text, with line numbers and errors
// ============================================================================
// The text IS the model, as in the terminal UI since v15: everything else in
// the window is derived from what is typed here. Errors come from the
// compiler, by line -- a red wave under the line, a mark in the gutter, and
// the message on hover.

#pragma once
#include <QMap>
#include <QPlainTextEdit>

class DesHighlighter;

class ModelEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    struct Mark { bool error{true}; QString message; };

    explicit ModelEditor(QWidget* parent = nullptr);

    // 1-based file line -> what is wrong there.
    void setMarks(const QMap<int, Mark>& marks);
    void goToLine(int line);
    // Replaces everything as ONE undo step -- what F2 and the starter model do.
    void replaceAll(const QString& text);

    int  gutterWidth() const;
    void paintGutter(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

private:
    QWidget*        m_gutter;
    DesHighlighter* m_highlighter;
    QMap<int, Mark> m_marks;
    void updateGutterWidth();
    void updateGutter(const QRect& rect, int dy);
};
