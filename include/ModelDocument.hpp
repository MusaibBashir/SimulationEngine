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
    std::size_t cellLine(const std::string& type, std::size_t index,
                         const std::string& column) const;
    void        setCell(const std::string& type, std::size_t index,
                        const std::string& column, std::string text,
                        std::size_t line = 0);

    void setSourceLines(std::vector<std::string> lines);
    const std::vector<std::string>& sourceLines() const { return m_sourceLines; }
    bool                            edited() const { return m_edited; }
    void                            markEdited() { m_edited = true; }
};

}  // namespace des
