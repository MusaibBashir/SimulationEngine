#include "ModelDocument.hpp"
#include "ModelError.hpp"

namespace des {
namespace {
const std::vector<ModelDocument::Row> EMPTY;
}

std::vector<ModelDocument::Row>& ModelDocument::require(const std::string& type,
                                                        std::size_t index) {
    auto it = m_rows.find(type);
    if (it == m_rows.end() || index >= it->second.size())
        throw ModelError("no row " + std::to_string(index) + " in '" + type + "'");
    return it->second;
}

std::size_t ModelDocument::rowCount(const std::string& type) const {
    const auto it = m_rows.find(type);
    return it == m_rows.end() ? 0 : it->second.size();
}

const std::vector<ModelDocument::Row>& ModelDocument::rows(const std::string& type) const {
    const auto it = m_rows.find(type);
    return it == m_rows.end() ? EMPTY : it->second;
}

ModelDocument::Row& ModelDocument::addRow(const std::string& type) {
    if (m_rows.find(type) == m_rows.end()) m_order.push_back(type);
    m_rows[type].push_back(Row{});
    markEdited();
    return m_rows[type].back();
}

void ModelDocument::removeRow(const std::string& type, std::size_t index) {
    std::vector<Row>& rs = require(type, index);
    rs.erase(rs.begin() + static_cast<std::ptrdiff_t>(index));
    markEdited();
}

void ModelDocument::moveRow(const std::string& type, std::size_t from, std::size_t to) {
    std::vector<Row>& rs = require(type, from);
    if (to >= rs.size()) throw ModelError("moveRow: destination out of range");
    Row moved = std::move(rs[from]);
    rs.erase(rs.begin() + static_cast<std::ptrdiff_t>(from));
    rs.insert(rs.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    markEdited();
}

bool ModelDocument::hasCell(const std::string& type, std::size_t index,
                            const std::string& column) const {
    if (index >= rowCount(type)) return false;
    const Row& r = rows(type)[index];
    return r.cells.find(column) != r.cells.end();
}

std::string ModelDocument::cell(const std::string& type, std::size_t index,
                                const std::string& column) const {
    if (index >= rowCount(type))
        throw ModelError("no row " + std::to_string(index) + " in '" + type + "'");
    const Row& r = rows(type)[index];
    const auto it = r.cells.find(column);
    return it == r.cells.end() ? std::string() : it->second.text;
}

std::size_t ModelDocument::cellLine(const std::string& type, std::size_t index,
                                    const std::string& column) const {
    if (index >= rowCount(type)) return 0;
    const Row& r = rows(type)[index];
    const auto it = r.cells.find(column);
    return it == r.cells.end() ? 0 : it->second.line;
}

void ModelDocument::setCell(const std::string& type, std::size_t index,
                            const std::string& column, std::string text,
                            std::size_t line) {
    std::vector<Row>& rs = require(type, index);
    rs[index].cells[column] = Cell{std::move(text), line};
    markEdited();
}

void ModelDocument::setSourceLines(std::vector<std::string> lines) {
    m_sourceLines = std::move(lines);
    // Reading a file is not an edit. The reader calls this last, so the
    // document can be written back verbatim until somebody actually changes it.
    m_edited = false;
}

}  // namespace des
