#include "DesHighlighter.hpp"

#include "Theme.hpp"

DesHighlighter::DesHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {
    QTextCharFormat header;
    header.setForeground(theme::accent);
    header.setFontWeight(QFont::Bold);
    QTextCharFormat key;
    key.setForeground(QColor(0x0F, 0x76, 0x6E));
    QTextCharFormat dist;
    dist.setForeground(QColor(0x7C, 0x3A, 0xED));
    dist.setFontWeight(QFont::DemiBold);
    QTextCharFormat number;
    number.setForeground(QColor(0xB4, 0x53, 0x09));

    m_rules.push_back({QRegularExpression(R"(^\s*(\[[A-Za-z]+\]))"), header, 1});
    m_rules.push_back({QRegularExpression(R"(^\s*([A-Za-z][A-Za-z0-9 \-]*?)\s*=)"), key, 1});
    m_rules.push_back({QRegularExpression(
                           R"(\b(EXPO|UNIF|TRIA|NORM|LOGN|CONS|WEIB|ERLA|POIS|DISC|NQ|NR|MR|WIP)\b)"),
                       dist, 1});
    m_rules.push_back({QRegularExpression(R"((?<![A-Za-z_])(\d+(?:\.\d+)?))"), number, 1});

    m_comment.setForeground(theme::muted);
    m_comment.setFontItalic(true);
}

void DesHighlighter::highlightBlock(const QString& text) {
    const int hash = text.indexOf('#');
    const QString code = hash >= 0 ? text.left(hash) : text;
    for (const Rule& r : m_rules) {
        auto it = r.pattern.globalMatch(code);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart(r.group)), static_cast<int>(m.capturedLength(r.group)),
                      r.format);
        }
    }
    if (hash >= 0) setFormat(hash, static_cast<int>(text.size()) - hash, m_comment);
}
