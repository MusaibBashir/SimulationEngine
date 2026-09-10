// ============================================================================
// ModelDocument.hpp  --  v11: a model as rows of text
// ============================================================================
// THIS MAY BE INVALID, AND THAT IS THE POINT. A Process naming a Resource that
// does not exist yet, a half-typed expression, a missing required column: all
// legal states here. That is the whole reason this is not a Model, which may
// never hold a broken flowchart. Model's strictness is a feature, and editing
// convenience must not erode it.
//
// Rows are addressed by POSITION. An id and a position disagree the moment a
// row moves, and position is what a person sees in the file and in a
// spreadsheet -- a diagnostic naming row 4 means the fourth one.

#pragma once
#include <map>
#include <string>
#include <vector>

namespace des {

class ModelDocument {
public:
    struct Cell {
        std::string text;
        std::size_t line{0};    // where it came from, for diagnostics
    };
    struct Row {
        std::map<std::string, Cell> cells;

        // v13: the lines this row was READ from, and whether anyone has
        // changed it since. Kept per row rather than per document because
        // v11's flag was per document -- so a single cell edit re-emitted the
        // whole file canonically and destroyed every comment in it, which is
        // the exact failure V11_READLOG says a front end must not have.
        //
        // The block includes the blank line and comments that preceded the
        // header, so a row carries its own annotation when it moves and takes
        // it with it when it is deleted.
        std::vector<std::string> source;
        bool                     edited{false};

        // v15: the line the row's [Header] was read from, 1-based. A cell
        // carries its own line already; this is for the diagnostics that name
        // a cell which is NOT THERE -- "Create needs a Name" has no cell to
        // point at, and a text editor still has to put the error somewhere.
        std::size_t              headerLine{0};
    };

private:
    // Insertion-ordered: the order module types first appear is the order they
    // are written back, which is half of what makes round-trip byte-identical.
    std::vector<std::string>                m_order;
    std::map<std::string, std::vector<Row>> m_rows;

    // The file this was read from, line for line. The writer emits these
    // verbatim while nothing has changed, which is the only way comments and
    // spacing survive a read-then-write. Empty for a document built in code.
    std::vector<std::string> m_sourceLines;
    bool                     m_edited{false};
    std::vector<std::string> m_preamble;
    std::vector<std::string> m_trailer;

    std::vector<Row>& require(const std::string& type, std::size_t index);

public:
    const std::vector<std::string>& types() const { return m_order; }
    std::size_t                     rowCount(const std::string& type) const;
    const std::vector<Row>&         rows(const std::string& type) const;

    Row& addRow(const std::string& type);
    void removeRow(const std::string& type, std::size_t index);
    void moveRow(const std::string& type, std::size_t from, std::size_t to);

    bool        hasCell(const std::string& type, std::size_t index,
                        const std::string& column) const;
    std::string cell(const std::string& type, std::size_t index,
                     const std::string& column) const;
    // The cell, or the schema's default when it is empty. v11's build pass
    // had this privately and v12 needs it too; two copies of 'what does an
    // empty cell mean' is one too many.
    std::string cellOrDefault(const std::string& type, std::size_t index,
                              const std::string& column) const;

    std::size_t cellLine(const std::string& type, std::size_t index,
                         const std::string& column) const;
    void        setCell(const std::string& type, std::size_t index,
                        const std::string& column, std::string text,
                        std::size_t line = 0);

    void setSourceLines(std::vector<std::string> lines);

    // v13: the lines before the first record and after the last one. Neither
    // belongs to a row, and both have to survive an edit.
    void setPreamble(std::vector<std::string> lines) { m_preamble = std::move(lines); }
    void setTrailer(std::vector<std::string> lines)  { m_trailer  = std::move(lines); }
    const std::vector<std::string>& preamble() const { return m_preamble; }
    const std::vector<std::string>& trailer()  const { return m_trailer; }

    // For the reader, which fills a row's source block after parsing it.
    Row& rowAt(const std::string& type, std::size_t index);
    const std::vector<std::string>& sourceLines() const { return m_sourceLines; }
    bool                            edited() const { return m_edited; }
    void                            markEdited() { m_edited = true; }
};

}  // namespace des
