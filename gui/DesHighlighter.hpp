// ============================================================================
// gui/DesHighlighter.hpp  --  colour for a .des file
// ============================================================================
// Headers, keys, distributions, numbers and comments, by regular expression.
// Colour only -- what is WRONG is the compiler's to say, drawn by the editor.

#pragma once
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <vector>

class DesHighlighter : public QSyntaxHighlighter {
public:
    explicit DesHighlighter(QTextDocument* doc);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; int group; };
    std::vector<Rule> m_rules;
    QTextCharFormat   m_comment;
};
