#include "TuiState.hpp"

#include <algorithm>
#include <fstream>
#include "Compiler.hpp"
#include "DocumentFormat.hpp"

namespace des {

TuiState TuiState::fromDocument(ModelDocument doc, std::string path) {
    TuiState s;
    s.m_document = std::move(doc);
    s.m_path = std::move(path);
    s.rebuildTypes();
    if (!s.m_types.empty()) s.m_type = s.m_types.front();
    s.recompile();
    return s;
}

TuiState TuiState::open(const std::string& path) {
    // is_open, NOT good: on the GCC this project builds with, constructing an
    // ifstream on a missing file leaves good() reporting true. v12's
    // regression harness lost an afternoon to exactly that.
    std::ifstream probe(path, std::ios::binary);
    if (!probe.is_open()) {
        TuiState s = fromDocument(ModelDocument{}, path);
        s.setStatus(path + ": new file");
        return s;
    }
    probe.close();

    ReadResult read = readDocumentFile(path);
    TuiState s = fromDocument(std::move(read.document), path);
    for (const Diagnostic& d : read.diagnostics) s.m_diagnostics.push_back(d);
    if (hasErrors(read.diagnostics))
        s.setStatus(path + ": opened WITH ERRORS -- see the diagnostics");
    else
        s.setStatus(path);
    return s;
}

void TuiState::rebuildTypes() {
    m_types.clear();
    for (const ModuleSchema& schema : ModuleRegistry::instance().all())
        m_types.push_back(schema.typeName);
    for (const std::string& t : m_document.types())
        if (std::find(m_types.begin(), m_types.end(), t) == m_types.end())
            m_types.push_back(t);
}

const ModuleSchema* TuiState::schemaHere() const {
    return ModuleRegistry::instance().find(m_type);
}

std::vector<std::string> TuiState::columnsHere() const {
    std::vector<std::string> out;
    if (const ModuleSchema* schema = schemaHere()) {
        for (const Column& c : schema->columns) out.push_back(c.id);
        return out;
    }
    // An unknown type has no schema, so its columns are whatever its rows
    // happen to carry. Showing them is the point: they are what tells a person
    // their editor is older than the file.
    for (std::size_t r = 0; r < m_document.rowCount(m_type); ++r)
        for (const auto& kv : m_document.rows(m_type)[r].cells)
            if (std::find(out.begin(), out.end(), kv.first) == out.end())
                out.push_back(kv.first);
    return out;
}

std::size_t TuiState::rowCountHere() const { return m_document.rowCount(m_type); }

void TuiState::setType(const std::string& type) {
    if (std::find(m_types.begin(), m_types.end(), type) == m_types.end()) return;
    m_type = type;
    m_row = 0;
    m_column = 0;
}

void TuiState::setRow(std::size_t row)       { m_row = row;       clampCursor(); }
void TuiState::setColumn(std::size_t column) { m_column = column; clampCursor(); }

void TuiState::clampCursor() {
    const std::size_t rows = rowCountHere();
    if (rows == 0)          m_row = 0;
    else if (m_row >= rows) m_row = rows - 1;

    const std::size_t cols = columnsHere().size();
    if (cols == 0)             m_column = 0;
    else if (m_column >= cols) m_column = cols - 1;
}

bool TuiState::save() { return save(m_path); }

bool TuiState::save(const std::string& toPath) {
    if (!writeDocumentFile(m_document, toPath)) {
        setStatus(toPath + ": could not write");
        return false;
    }
    if (toPath == m_path) m_dirty = false;
    setStatus(toPath + ": saved");
    return true;
}

void TuiState::recompile() {
    CompileResult r = compile(m_document);
    m_diagnostics = std::move(r.diagnostics);
}

bool TuiState::readOnlyHere() const {
    const ModuleSchema* schema = schemaHere();
    return schema != nullptr && schema->readOnly;
}

bool TuiState::refuseIfReadOnly() {
    if (!readOnlyHere()) return false;
    setStatus(m_type + " is read-only: there is no queue object apart from its "
                       "Process, so set the discipline on the Process row");
    return true;
}

void TuiState::pushUndo() {
    m_undo.push_back(m_document);
    if (m_undo.size() > UNDO_DEPTH) m_undo.erase(m_undo.begin());
}

void TuiState::setCell(const std::string& column, const std::string& text) {
    if (refuseIfReadOnly()) return;
    if (m_row >= rowCountHere()) return;
    pushUndo();
    m_document.setCell(m_type, m_row, column, text);
    m_dirty = true;
    recompile();
}

void TuiState::addRow() {
    if (refuseIfReadOnly()) return;
    pushUndo();
    m_document.addRow(m_type);
    rebuildTypes();
    m_row = rowCountHere() - 1;
    m_dirty = true;
    recompile();
}

void TuiState::removeRow() {
    if (refuseIfReadOnly()) return;
    if (m_row >= rowCountHere()) return;
    pushUndo();
    m_document.removeRow(m_type, m_row);
    clampCursor();
    m_dirty = true;
    recompile();
}

void TuiState::moveRow(int delta) {
    if (refuseIfReadOnly()) return;
    const std::size_t rows = rowCountHere();
    if (rows < 2 || m_row >= rows) return;
    const long long to = static_cast<long long>(m_row) + delta;
    if (to < 0 || to >= static_cast<long long>(rows)) return;
    pushUndo();
    m_document.moveRow(m_type, m_row, static_cast<std::size_t>(to));
    m_row = static_cast<std::size_t>(to);
    m_dirty = true;
    recompile();
}

void TuiState::undo() {
    if (m_undo.empty()) return;
    m_document = m_undo.back();
    m_undo.pop_back();
    rebuildTypes();
    clampCursor();
    // DIRTY STAYS TRUE, on purpose. Undoing back to what is on disk is not the
    // same as knowing you are there -- that needs a saved-at marker in the
    // stack -- and claiming "not dirty" wrongly is how work gets lost.
    m_dirty = true;
    recompile();
    setStatus("undone");
}

}  // namespace des
