// ============================================================================
// TuiTables.cpp  --  v16.1: the Tables tab's half of TuiState
// ============================================================================
// Variables, arrays, matrices and schedules, edited as grids. Everything here
// changes m_tables and nothing else; tablesChanged() re-reads the joined model
// so the diagnostics, the pick lists and the runs see the change at once.
// ============================================================================

#include "TuiState.hpp"

#include <algorithm>
#include <cstdlib>

namespace des {

namespace {

const TableKind KINDS[4] = {TableKind::Variable, TableKind::Array, TableKind::Matrix,
                            TableKind::Schedule};
const char* const UNITS[4] = {"Seconds", "Minutes", "Hours", "Days"};

std::string trimmedText(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    const std::size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

bool growsByRows(TableKind k) { return k == TableKind::Array || k == TableKind::Schedule; }

}  // namespace

std::string TuiState::joinedText() const { return joinTables(m_buffer.text(), m_tables); }

std::size_t TuiState::tableErrorCount() const {
    std::size_t n = 0;
    for (const TableIssue& i : m_tableIssues) n += i.severity == Severity::Error ? 1 : 0;
    return n;
}

void TuiState::tablesChanging() {
    m_tableUndo.push_back(m_tables);
    if (m_tableUndo.size() > 256) m_tableUndo.erase(m_tableUndo.begin());
    m_tableRedo.clear();
}

void TuiState::tablesChanged() {
    m_dirty = true;
    reparse();
    clampTableCursor();
}

void TuiState::clampTableCursor() {
    if (m_tables.empty()) { m_tableIndex = 0; m_tableField = 0; return; }
    if (m_tableIndex >= m_tables.size()) m_tableIndex = m_tables.size() - 1;
    const Table& t = m_tables[m_tableIndex];
    const std::size_t fields = headerFields().size();
    if (m_tableField > fields) m_tableField = fields;
    if (m_cellRow >= t.rows) m_cellRow = t.rows - 1;
    if (m_cellColumn >= t.columns) m_cellColumn = t.columns - 1;
}

// --- panels ------------------------------------------------------------------

void TuiState::setTablesFocus(TablesFocus f) {
    if (m_editingTable) cancelTableEdit();
    m_tablesFocus = f;
}

void TuiState::stepKind(long long delta) {
    long long i = static_cast<long long>(m_kindIndex) + delta;
    m_kindIndex = static_cast<std::size_t>(std::max<long long>(0, std::min<long long>(i, 3)));
}

void TuiState::setKindIndex(std::size_t i) { if (i < 4) m_kindIndex = i; }

void TuiState::createTable() {
    const TableKind kind = KINDS[m_kindIndex];
    Table t;
    t.kind = kind;
    std::size_t n = 1;
    const auto taken = [this](const std::string& name) {
        for (const Table& x : m_tables) if (x.name == name) return true;
        return false;
    };
    while (taken(std::string(kindName(kind)) + std::to_string(n))) ++n;
    t.name = std::string(kindName(kind)) + std::to_string(n);
    switch (kind) {
        case TableKind::Variable: t.rows = 1; t.columns = 1; break;
        case TableKind::Array:    t.rows = 3; t.columns = 1; break;
        case TableKind::Matrix:   t.rows = 2; t.columns = 2; break;
        case TableKind::Schedule: t.rows = 1; t.columns = 2; break;
    }
    t.cells.assign(t.rows * t.columns, "0");
    if (kind == TableKind::Schedule) t.cells = {"0", "1"};
    tablesChanging();
    m_tables.push_back(std::move(t));
    m_tableIndex = m_tables.size() - 1;
    m_tablesFocus = TablesFocus::Editor;
    m_tableField = 0;
    m_cellRow = m_cellColumn = m_entryColumn = 0;
    tablesChanged();
    setStatus(std::string("a new ") + kindName(kind) +
              " -- Enter on Name to call it something that says what it is");
}

void TuiState::stepTable(long long delta) {
    if (m_tables.empty()) return;
    long long i = static_cast<long long>(m_tableIndex) + delta;
    i = std::max<long long>(0, std::min<long long>(i, static_cast<long long>(m_tables.size()) - 1));
    selectTable(static_cast<std::size_t>(i));
}

void TuiState::selectTable(std::size_t i) {
    if (i >= m_tables.size()) return;
    if (m_editingTable) cancelTableEdit();
    if (i != m_tableIndex) { m_tableField = 0; m_cellRow = m_cellColumn = m_entryColumn = 0; }
    m_tableIndex = i;
}

const Table* TuiState::currentTable() const {
    return m_tableIndex < m_tables.size() ? &m_tables[m_tableIndex] : nullptr;
}

// --- the editor --------------------------------------------------------------

std::vector<std::string> TuiState::headerFields() const {
    const Table* t = currentTable();
    if (t == nullptr) return {};
    switch (t->kind) {
        case TableKind::Variable: return {"Name"};
        case TableKind::Array:    return {"Name", "Size"};
        case TableKind::Matrix:   return {"Name", "Rows", "Columns"};
        case TableKind::Schedule: return {"Name", "Rows", "Time Units"};
    }
    return {"Name"};
}

std::string TuiState::headerValue(std::size_t field) const {
    const Table* t = currentTable();
    const std::vector<std::string> fields = headerFields();
    if (t == nullptr || field >= fields.size()) return std::string();
    const std::string& f = fields[field];
    if (f == "Name") return t->name;
    if (f == "Size" || f == "Rows") return std::to_string(t->rows);
    if (f == "Columns") return std::to_string(t->columns);
    return t->timeUnits;
}

bool TuiState::onGrid() const { return m_tableField >= headerFields().size(); }

void TuiState::setTableCursor(std::size_t field, std::size_t row, std::size_t column) {
    if (m_editingTable) cancelTableEdit();
    m_tableField = field;
    m_cellRow = row;
    m_cellColumn = column;
    clampTableCursor();
    m_entryColumn = m_cellColumn;
}

void TuiState::moveTableCursor(long long rows, long long columns) {
    const Table* t = currentTable();
    if (t == nullptr) return;
    if (m_editingTable && !commitTableEdit()) return;
    const std::size_t fields = headerFields().size();
    if (!onGrid()) {
        long long f = static_cast<long long>(m_tableField) + rows;
        f = std::max<long long>(0, f);
        if (f >= static_cast<long long>(fields)) { m_tableField = fields; m_cellRow = 0; }
        else m_tableField = static_cast<std::size_t>(f);
    } else {
        long long r = static_cast<long long>(m_cellRow) + rows;
        if (r < 0) { m_tableField = fields - 1; }
        else m_cellRow = static_cast<std::size_t>(std::min<long long>(r, static_cast<long long>(t->rows) - 1));
        long long c = static_cast<long long>(m_cellColumn) + columns;
        c = std::max<long long>(0, std::min<long long>(c, static_cast<long long>(t->columns) - 1));
        m_cellColumn = static_cast<std::size_t>(c);
    }
    m_entryColumn = m_cellColumn;
}

void TuiState::beginTableEdit(bool keepValue) {
    const Table* t = currentTable();
    if (t == nullptr) return;
    if (!onGrid() && headerFields()[m_tableField] == "Time Units") {
        m_choices.assign(std::begin(UNITS), std::end(UNITS));
        const auto it = std::find(m_choices.begin(), m_choices.end(), t->timeUnits);
        m_choice = it == m_choices.end() ? 2 : static_cast<std::size_t>(it - m_choices.begin());
        m_pickerFor = PickerFor::TableUnits;
        m_pickerTitle = "Time Units";
        m_overlay = Overlay::Picker;
        return;
    }
    m_tableDraft = !keepValue ? std::string()
                 : onGrid()   ? t->at(m_cellRow, m_cellColumn)
                              : headerValue(m_tableField);
    m_editingTable = true;
}

void TuiState::typeTable(char c) {
    if (!m_editingTable) beginTableEdit(false);
    if (m_editingTable) m_tableDraft.push_back(c);
}

void TuiState::backspaceTable() {
    if (!m_editingTable) beginTableEdit(true);
    if (m_editingTable && !m_tableDraft.empty()) m_tableDraft.pop_back();
}

void TuiState::cancelTableEdit() {
    m_editingTable = false;
    m_tableDraft.clear();
}

bool TuiState::commitTableEdit() {
    if (!m_editingTable) return true;
    const std::string value = trimmedText(m_tableDraft);
    if (!onGrid()) {
        if (!setHeaderField(m_tableField, value)) return false;
        m_editingTable = false;
        return true;
    }
    if (!isNumberCell(value)) {
        setStatus("a cell holds a number, not '" + value + "'");
        return false;
    }
    Table& t = m_tables[m_tableIndex];
    if (t.at(m_cellRow, m_cellColumn) != value) {
        tablesChanging();
        t.at(m_cellRow, m_cellColumn) = value;
        tablesChanged();
    }
    m_editingTable = false;
    return true;
}

bool TuiState::setHeaderField(std::size_t field, const std::string& value) {
    Table& t = m_tables[m_tableIndex];
    const std::string f = headerFields()[field];
    if (f == "Name") {
        if (value == t.name) return true;
        if (!isTableName(value)) {
            setStatus("a name is letters, digits and _, not starting with a digit");
            return false;
        }
        for (std::size_t i = 0; i < m_tables.size(); ++i)
            if (i != m_tableIndex && m_tables[i].name == value) {
                setStatus("there is already a table called " + value);
                return false;
            }
        if (!tableUsers().empty()) {
            m_pendingName = value;
            m_confirmFor = ConfirmFor::RenameTable;
            m_overlay = Overlay::Confirm;
            setStatus("rename " + t.name + " to " + value + " in the " +
                      std::to_string(tableUsers().size()) +
                      " place(s) that use it too? (y)es  (n)o, only the table");
            return true;
        }
        applyRename(value, false);
        return true;
    }
    if (f == "Time Units") {
        if (std::find(std::begin(UNITS), std::end(UNITS), value) == std::end(UNITS)) {
            setStatus("Time Units is Seconds, Minutes, Hours or Days");
            return false;
        }
        tablesChanging();
        t.timeUnits = value;
        tablesChanged();
        return true;
    }
    // A size.
    char* end = nullptr;
    const long long n = std::strtoll(value.c_str(), &end, 10);
    if (value.empty() || *end != '\0' || n < 1 || n > 1000) {
        setStatus(f + " is a whole number from 1 to 1000");
        return false;
    }
    const std::size_t rows = f == "Columns" ? t.rows : static_cast<std::size_t>(n);
    const std::size_t columns = f == "Columns" ? static_cast<std::size_t>(n) : t.columns;
    if (rows == t.rows && columns == t.columns) return true;
    if (t.resizeLoses(rows, columns)) {
        m_pendingRows = rows;
        m_pendingColumns = columns;
        m_confirmFor = ConfirmFor::ResizeTable;
        m_overlay = Overlay::Confirm;
        setStatus("that drops cells that are not 0 -- resize anyway? (y)es  (n)o");
        return true;
    }
    tablesChanging();
    t.resize(rows, columns);
    tablesChanged();
    return true;
}

void TuiState::applyRename(const std::string& to, bool references) {
    Table& t = m_tables[m_tableIndex];
    const std::string from = t.name;
    tablesChanging();
    t.name = to;
    if (references) {
        const std::string text = m_buffer.text();
        const std::string renamed = renameInText(text, from, to);
        if (renamed != text) {
            std::vector<std::string> lines{std::string()};
            for (const char c : renamed) {
                if (c == '\n') lines.push_back(std::string());
                else lines.back().push_back(c);
            }
            const Caret keep = m_buffer.caret();
            m_buffer.replaceLines(0, m_buffer.lineCount(), lines);
            m_buffer.moveTo(keep);
        }
    }
    tablesChanged();
    setStatus(references ? "renamed " + from + " to " + to + ", and the model uses the new name"
                         : "renamed " + from + " to " + to);
}

void TuiState::tableTab() {
    if (m_editingTable && !commitTableEdit()) return;
    const Table* t = currentTable();
    if (t == nullptr) return;
    if (!onGrid()) {
        m_tableField = std::min(m_tableField + 1, headerFields().size());
        return;
    }
    // m_entryColumn stays: Enter goes back to where this row started.
    if (m_cellColumn + 1 < t->columns) ++m_cellColumn;
}

void TuiState::tableEnter() {
    if (!m_editingTable) { beginTableEdit(true); return; }
    if (!commitTableEdit()) return;
    if (m_overlay == Overlay::Confirm) return;          // a rename or resize is asking
    if (!onGrid()) return;
    Table& t = m_tables[m_tableIndex];
    if (m_cellRow + 1 < t.rows) {
        ++m_cellRow;
    } else if (growsByRows(t.kind)) {
        // Typing -- or pasting -- down past the last row adds one, as a
        // spreadsheet would.
        tablesChanging();
        t.resize(t.rows + 1, t.columns);
        if (t.kind == TableKind::Schedule) t.at(t.rows - 1, 1) = t.at(t.rows - 2, 1);
        tablesChanged();
        ++m_cellRow;
    }
    m_cellColumn = std::min(m_entryColumn, t.columns - 1);
}

void TuiState::insertTableRow() {
    Table* t = m_tableIndex < m_tables.size() ? &m_tables[m_tableIndex] : nullptr;
    if (t == nullptr) return;
    if (!growsByRows(t->kind)) {
        setStatus(t->kind == TableKind::Matrix ? "a matrix grows by its Rows and Columns fields"
                                               : "a variable is one value");
        return;
    }
    tablesChanging();
    const std::size_t at = onGrid() ? m_cellRow + 1 : t->rows;
    std::vector<std::string> row(t->columns, "0");
    if (t->kind == TableKind::Schedule && at > 0) row[1] = t->at(at - 1, 1);
    t->cells.insert(t->cells.begin() + static_cast<long>(at * t->columns), row.begin(), row.end());
    ++t->rows;
    m_tableField = headerFields().size();
    m_cellRow = at;
    tablesChanged();
    setStatus("a row added");
}

void TuiState::deleteTableRow() {
    Table* t = m_tableIndex < m_tables.size() ? &m_tables[m_tableIndex] : nullptr;
    if (t == nullptr || !onGrid()) return;
    if (!growsByRows(t->kind)) { setStatus("rows of a matrix go by its Rows field"); return; }
    if (t->rows <= 1) { setStatus("a table keeps at least one row"); return; }
    tablesChanging();
    t->cells.erase(t->cells.begin() + static_cast<long>(m_cellRow * t->columns),
                   t->cells.begin() + static_cast<long>((m_cellRow + 1) * t->columns));
    --t->rows;
    tablesChanged();
    setStatus("row deleted -- ^Z brings it back");
}

void TuiState::pasteIntoTable(const std::string& block) {
    Table* t = m_tableIndex < m_tables.size() ? &m_tables[m_tableIndex] : nullptr;
    if (t == nullptr) return;
    const auto rows = parsePastedBlock(block);
    if (rows.empty()) { setStatus("nothing to paste"); return; }
    std::size_t width = 0;
    for (const auto& r : rows) {
        width = std::max(width, r.size());
        for (const std::string& c : r)
            if (!c.empty() && !isNumberCell(c)) {
                setStatus("the pasted block has '" + c + "', which is not a number");
                return;
            }
    }
    const std::size_t r0 = onGrid() ? m_cellRow : 0;
    const std::size_t c0 = onGrid() ? m_cellColumn : 0;
    tablesChanging();
    std::size_t needRows = t->rows, needColumns = t->columns;
    if (t->kind != TableKind::Variable) needRows = std::max(t->rows, r0 + rows.size());
    if (t->kind == TableKind::Matrix) needColumns = std::max(t->columns, c0 + width);
    t->resize(needRows, needColumns);
    bool clipped = false;
    for (std::size_t i = 0; i < rows.size(); ++i)
        for (std::size_t j = 0; j < rows[i].size(); ++j) {
            if (r0 + i >= t->rows || c0 + j >= t->columns) { clipped = true; continue; }
            if (!rows[i][j].empty()) t->at(r0 + i, c0 + j) = rows[i][j];
        }
    m_tableField = headerFields().size();
    tablesChanged();
    setStatus(clipped ? "pasted -- some columns did not fit this table and were left out"
                      : "pasted " + std::to_string(rows.size()) + " row(s)");
}

void TuiState::requestDeleteTable() {
    const Table* t = currentTable();
    if (t == nullptr) { setStatus("there is no table to delete"); return; }
    const std::vector<std::string> users = tableUsers();
    std::string why;
    for (const std::string& u : users) why += (why.empty() ? "" : ", ") + u;
    m_confirmFor = ConfirmFor::DeleteTable;
    m_overlay = Overlay::Confirm;
    setStatus("delete " + t->name + (users.empty() ? "" : " (used by " + why + ")") +
              "? (y)es  (n)o");
}

void TuiState::undoTables() {
    if (m_tableUndo.empty()) { setStatus("nothing to undo in the tables"); return; }
    m_tableRedo.push_back(m_tables);
    m_tables = m_tableUndo.back();
    m_tableUndo.pop_back();
    tablesChanged();
    setStatus("undone");
}

void TuiState::redoTables() {
    if (m_tableRedo.empty()) { setStatus("nothing to redo in the tables"); return; }
    m_tableUndo.push_back(m_tables);
    m_tables = m_tableRedo.back();
    m_tableRedo.pop_back();
    tablesChanged();
    setStatus("redone");
}

std::vector<std::string> TuiState::tableUsers() const {
    const Table* t = currentTable();
    return t == nullptr ? std::vector<std::string>{} : usersOf(m_document, t->name);
}

void TuiState::showTableHelp() {
    const Table* t = currentTable();
    m_helpBody.clear();
    if (t == nullptr || m_tablesFocus == TablesFocus::Kinds) {
        const TableKind k = KINDS[m_kindIndex];
        m_helpTitle = kindName(k);
        switch (k) {
            case TableKind::Variable:
                m_helpBody = {"One number every block can read and Assign can change, "
                              "such as a fleet size: Per Arrival = Num_CV."};
                break;
            case TableKind::Array:
                m_helpBody = {"A list of numbers under one name, read as Name(i), "
                              "counting from 1."};
                break;
            case TableKind::Matrix:
                m_helpBody = {"A table of numbers, read as Name(row, column) -- "
                              "TravelTime(myZone, 2) is row myZone, column 2."};
                break;
            case TableKind::Schedule:
                m_helpBody = {"Arrival rates that change over time. Each row is a Value "
                              "(arrivals per Time Unit) and a Duration; after the last row "
                              "it starts again. A Create uses it by naming it in its "
                              "Schedule field."};
                break;
        }
        m_helpBody.push_back(std::string());
        m_helpBody.push_back("Enter makes a new one.");
    } else if (!onGrid()) {
        const std::string f = headerFields()[m_tableField];
        m_helpTitle = t->name + "." + f;
        if (f == "Name")
            m_helpBody = {"What the model calls it. Renaming offers to change the "
                          "places that use it too."};
        else if (f == "Time Units")
            m_helpBody = {"The unit of every row's Duration, and the \"per\" in its "
                          "Value: 90 with Hours is 90 arrivals an hour."};
        else
            m_helpBody = {"How many " + std::string(f == "Columns" ? "columns" : "rows") +
                          ". Shrinking asks first if it would drop numbers."};
    } else {
        m_helpTitle = t->name;
        m_helpBody = {"Type a number to replace a cell; Enter keeps it and moves down, "
                      "Tab keeps it and moves right -- so a block pasted from Excel "
                      "lands as it looked there. ^V pastes this program's clipboard as "
                      "a block. ^O adds a row, Del removes one."};
    }
    m_overlay = Overlay::Help;
}

}  // namespace des
