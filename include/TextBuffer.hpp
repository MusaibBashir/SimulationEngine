// ============================================================================
// TextBuffer.hpp  --  v15: the text IS the model
// ============================================================================
// v13 edited a ModelDocument and wrote it out. v15 edits the FILE and parses
// it, which is the other way round and settles three things at once:
//
//   - the byte-identical round trip is free rather than a guarantee to keep,
//     because what is saved is what was typed;
//   - there is one place a value can be wrong instead of two;
//   - comments, blank lines and ordering are ordinary text a person edits,
//     rather than data a writer has to be careful with.
//
// So this is a plain vector of lines with a cursor on it. Everything above it
// re-reads and re-compiles after each change, which for a file measured in
// kilobytes costs nothing worth measuring.

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace des {

// Where a cursor or a selection edge is. Line and column are both 0-based, and
// a column may equal the line's length -- that is the position after the last
// character, which is where typing usually happens.
struct Caret {
    std::size_t line{0};
    std::size_t column{0};
};

bool operator==(const Caret& a, const Caret& b);
bool operator!=(const Caret& a, const Caret& b);
bool operator<(const Caret& a, const Caret& b);

class TextBuffer {
public:
    TextBuffer();
    explicit TextBuffer(const std::string& text);

    // Splits on \n and drops \r, so a CRLF file edits as though it were LF and
    // is written back as LF. A .des file is line-oriented and this project's
    // gates compare bytes, so one line ending everywhere is the only sane rule.
    void setText(const std::string& text);
    std::string text() const;

    const std::vector<std::string>& lines() const { return m_lines; }
    std::size_t lineCount() const { return m_lines.size(); }
    const std::string& lineAt(std::size_t i) const;

    Caret caret() const { return m_caret; }
    void  moveTo(Caret to, bool extend = false);
    void  moveBy(long long lines, long long columns, bool extend = false);
    void  moveToLineStart(bool extend = false);
    void  moveToLineEnd(bool extend = false);
    void  moveToStart(bool extend = false);
    void  moveToEnd(bool extend = false);

    // A selection exists when its anchor differs from the caret. Both edges are
    // kept rather than a start and a length, because a selection built by
    // dragging backwards is the same selection built by dragging forwards and
    // nothing above this should have to know which way it was made.
    bool  hasSelection() const { return m_anchor != m_caret; }
    Caret selectionStart() const;
    Caret selectionEnd() const;
    void  selectAll();
    void  clearSelection() { m_anchor = m_caret; }
    std::string selectedText() const;

    void insert(const std::string& text);   // replaces the selection, if any
    void newline();
    void backspace();
    void del();
    bool deleteSelection();                 // false when there was none

    // Whole lines, which is what the module palette inserts. The block lands
    // BEFORE the caret's line, and the caret ends on the block's first blank
    // value -- the field a person fills in first.
    void insertLines(const std::vector<std::string>& block);

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    void undo();
    void redo();
    // Called before a group of edits that should undo as one. Ordinary edits
    // call it themselves; a caller only needs it to merge several.
    void checkpoint();

private:
    struct Snapshot {
        std::vector<std::string> lines;
        Caret                    caret;
        Caret                    anchor;
    };

    void clampCaret();
    void pushUndo();

    std::vector<std::string> m_lines;
    Caret                    m_caret;
    Caret                    m_anchor;

    static constexpr std::size_t UNDO_DEPTH = 256;
    std::vector<Snapshot>    m_undo;
    std::vector<Snapshot>    m_redo;
};

}  // namespace des
