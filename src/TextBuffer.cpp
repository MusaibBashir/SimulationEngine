#include "TextBuffer.hpp"

#include <algorithm>

namespace des {

bool operator==(const Caret& a, const Caret& b) {
    return a.line == b.line && a.column == b.column;
}
bool operator!=(const Caret& a, const Caret& b) { return !(a == b); }
bool operator<(const Caret& a, const Caret& b) {
    if (a.line != b.line) return a.line < b.line;
    return a.column < b.column;
}

namespace {
const std::string EMPTY_LINE;
}

TextBuffer::TextBuffer() : m_lines{std::string()} {}

TextBuffer::TextBuffer(const std::string& text) { setText(text); }

void TextBuffer::setText(const std::string& text) {
    m_lines.clear();
    std::string current;
    for (const char c : text) {
        if (c == '\n')      { m_lines.push_back(current); current.clear(); }
        else if (c != '\r') { current.push_back(c); }
    }
    m_lines.push_back(current);
    // ALWAYS at least one line, even for empty text. A buffer with no lines
    // has no place to put a cursor, and every caller would need the special
    // case that this one line removes.
    if (m_lines.empty()) m_lines.push_back(std::string());
    m_caret  = Caret{};
    m_anchor = Caret{};
    m_undo.clear();
    m_redo.clear();
}

std::string TextBuffer::text() const {
    std::string out;
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        out += m_lines[i];
        if (i + 1 < m_lines.size()) out.push_back('\n');
    }
    return out;
}

const std::string& TextBuffer::lineAt(std::size_t i) const {
    return i < m_lines.size() ? m_lines[i] : EMPTY_LINE;
}

void TextBuffer::clampCaret() {
    if (m_lines.empty()) m_lines.push_back(std::string());
    if (m_caret.line >= m_lines.size()) m_caret.line = m_lines.size() - 1;
    m_caret.column = std::min(m_caret.column, m_lines[m_caret.line].size());
    if (m_anchor.line >= m_lines.size()) m_anchor.line = m_lines.size() - 1;
    m_anchor.column = std::min(m_anchor.column, m_lines[m_anchor.line].size());
}

void TextBuffer::moveTo(Caret to, bool extend) {
    m_caret = to;
    clampCaret();
    if (!extend) m_anchor = m_caret;
}

void TextBuffer::moveBy(long long lines, long long columns, bool extend) {
    if (lines != 0) {
        long long l = static_cast<long long>(m_caret.line) + lines;
        l = std::max<long long>(0, std::min(l, static_cast<long long>(m_lines.size()) - 1));
        m_caret.line = static_cast<std::size_t>(l);
    }
    if (columns != 0) {
        long long c = static_cast<long long>(m_caret.column) + columns;
        // Off either end of a line steps to the next or previous one, which is
        // what every editor does and what makes Left at column 0 useful.
        while (c < 0 && m_caret.line > 0) {
            --m_caret.line;
            c += static_cast<long long>(m_lines[m_caret.line].size()) + 1;
        }
        while (c > static_cast<long long>(m_lines[m_caret.line].size()) &&
               m_caret.line + 1 < m_lines.size()) {
            c -= static_cast<long long>(m_lines[m_caret.line].size()) + 1;
            ++m_caret.line;
        }
        m_caret.column = static_cast<std::size_t>(std::max<long long>(0, c));
    }
    clampCaret();
    if (!extend) m_anchor = m_caret;
}

void TextBuffer::moveToLineStart(bool extend) {
    m_caret.column = 0;
    if (!extend) m_anchor = m_caret;
}

void TextBuffer::moveToLineEnd(bool extend) {
    m_caret.column = m_lines[m_caret.line].size();
    if (!extend) m_anchor = m_caret;
}

void TextBuffer::moveToStart(bool extend) { moveTo(Caret{0, 0}, extend); }

void TextBuffer::moveToEnd(bool extend) {
    moveTo(Caret{m_lines.size() - 1, m_lines.back().size()}, extend);
}

Caret TextBuffer::selectionStart() const {
    return m_caret < m_anchor ? m_caret : m_anchor;
}
Caret TextBuffer::selectionEnd() const {
    return m_caret < m_anchor ? m_anchor : m_caret;
}

void TextBuffer::selectAll() {
    m_anchor = Caret{0, 0};
    m_caret  = Caret{m_lines.size() - 1, m_lines.back().size()};
}

std::string TextBuffer::selectedText() const {
    if (!hasSelection()) return std::string();
    const Caret a = selectionStart();
    const Caret b = selectionEnd();
    if (a.line == b.line)
        return m_lines[a.line].substr(a.column, b.column - a.column);

    std::string out = m_lines[a.line].substr(a.column);
    for (std::size_t l = a.line + 1; l < b.line; ++l) {
        out.push_back('\n');
        out += m_lines[l];
    }
    out.push_back('\n');
    out += m_lines[b.line].substr(0, b.column);
    return out;
}

void TextBuffer::checkpoint() { pushUndo(); }

void TextBuffer::pushUndo() {
    m_undo.push_back(Snapshot{m_lines, m_caret, m_anchor});
    if (m_undo.size() > UNDO_DEPTH) m_undo.erase(m_undo.begin());
    // A NEW EDIT DISCARDS THE REDO STACK. Keeping it would let redo replay
    // changes that no longer apply to the text in front of the person.
    m_redo.clear();
}

void TextBuffer::undo() {
    if (m_undo.empty()) return;
    m_redo.push_back(Snapshot{m_lines, m_caret, m_anchor});
    const Snapshot& s = m_undo.back();
    m_lines  = s.lines;
    m_caret  = s.caret;
    m_anchor = s.anchor;
    m_undo.pop_back();
    clampCaret();
}

void TextBuffer::redo() {
    if (m_redo.empty()) return;
    m_undo.push_back(Snapshot{m_lines, m_caret, m_anchor});
    const Snapshot& s = m_redo.back();
    m_lines  = s.lines;
    m_caret  = s.caret;
    m_anchor = s.anchor;
    m_redo.pop_back();
    clampCaret();
}

bool TextBuffer::deleteSelection() {
    if (!hasSelection()) return false;
    const Caret a = selectionStart();
    const Caret b = selectionEnd();
    const std::string head = m_lines[a.line].substr(0, a.column);
    const std::string tail = m_lines[b.line].substr(b.column);
    m_lines.erase(m_lines.begin() + static_cast<long>(a.line),
                  m_lines.begin() + static_cast<long>(b.line) + 1);
    m_lines.insert(m_lines.begin() + static_cast<long>(a.line), head + tail);
    m_caret  = a;
    m_anchor = a;
    clampCaret();
    return true;
}

void TextBuffer::insert(const std::string& text) {
    if (text.empty() && !hasSelection()) return;
    pushUndo();
    deleteSelection();

    // Split what is being inserted, so a paste carrying newlines behaves the
    // same as typing them. A clipboard almost always does.
    std::vector<std::string> parts{std::string()};
    for (const char c : text) {
        if (c == '\n')      parts.push_back(std::string());
        else if (c != '\r') parts.back().push_back(c);
    }

    std::string& line = m_lines[m_caret.line];
    const std::string tail = line.substr(m_caret.column);
    line = line.substr(0, m_caret.column) + parts.front();

    if (parts.size() == 1) {
        m_caret.column = line.size();
        line += tail;
    } else {
        std::size_t at = m_caret.line;
        for (std::size_t i = 1; i < parts.size(); ++i)
            m_lines.insert(m_lines.begin() + static_cast<long>(++at), parts[i]);
        m_caret.line   = at;
        m_caret.column = m_lines[at].size();
        m_lines[at] += tail;
    }
    m_anchor = m_caret;
}

void TextBuffer::newline() {
    pushUndo();
    deleteSelection();
    std::string& line = m_lines[m_caret.line];
    const std::string tail = line.substr(m_caret.column);
    line.erase(m_caret.column);
    m_lines.insert(m_lines.begin() + static_cast<long>(m_caret.line) + 1, tail);
    ++m_caret.line;
    m_caret.column = 0;
    m_anchor = m_caret;
}

void TextBuffer::backspace() {
    if (hasSelection()) { pushUndo(); deleteSelection(); return; }
    if (m_caret.line == 0 && m_caret.column == 0) return;
    pushUndo();
    if (m_caret.column > 0) {
        m_lines[m_caret.line].erase(m_caret.column - 1, 1);
        --m_caret.column;
    } else {
        const std::string tail = m_lines[m_caret.line];
        m_lines.erase(m_lines.begin() + static_cast<long>(m_caret.line));
        --m_caret.line;
        m_caret.column = m_lines[m_caret.line].size();
        m_lines[m_caret.line] += tail;
    }
    m_anchor = m_caret;
}

void TextBuffer::del() {
    if (hasSelection()) { pushUndo(); deleteSelection(); return; }
    const bool atEnd = m_caret.line + 1 == m_lines.size() &&
                       m_caret.column == m_lines[m_caret.line].size();
    if (atEnd) return;
    pushUndo();
    if (m_caret.column < m_lines[m_caret.line].size()) {
        m_lines[m_caret.line].erase(m_caret.column, 1);
    } else {
        m_lines[m_caret.line] += m_lines[m_caret.line + 1];
        m_lines.erase(m_lines.begin() + static_cast<long>(m_caret.line) + 1);
    }
    m_anchor = m_caret;
}

void TextBuffer::insertLines(const std::vector<std::string>& block) {
    if (block.empty()) return;
    pushUndo();
    deleteSelection();

    // Land on a line boundary. Inserting a [Create] record into the middle of
    // somebody's `Interarrival = EXPO(1.0)` would produce two broken lines, so
    // a partly-typed line is split rather than cut through.
    std::size_t at = m_caret.line;
    if (!m_lines[at].empty()) {
        if (m_caret.column == 0) {
            // insert above
        } else if (m_caret.column == m_lines[at].size()) {
            ++at;
        } else {
            const std::string tail = m_lines[at].substr(m_caret.column);
            m_lines[at].erase(m_caret.column);
            m_lines.insert(m_lines.begin() + static_cast<long>(at) + 1, tail);
            ++at;
        }
    }

    for (std::size_t i = 0; i < block.size(); ++i)
        m_lines.insert(m_lines.begin() + static_cast<long>(at + i), block[i]);

    // The caret ends after the first `= ` in the block, which is the field a
    // person fills in first. Landing it on the [Header] instead would make
    // every insertion start with the same two keystrokes.
    m_caret = Caret{at, 0};
    for (std::size_t i = 0; i < block.size(); ++i) {
        const std::size_t eq = block[i].find('=');
        if (eq != std::string::npos) {
            m_caret = Caret{at + i, m_lines[at + i].size()};
            break;
        }
    }
    m_anchor = m_caret;
    clampCaret();
}

}  // namespace des
