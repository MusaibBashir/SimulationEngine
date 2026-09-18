// ============================================================================
// TableStore.cpp
// ============================================================================

#include "TableStore.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include "DocumentFormat.hpp"

namespace des {

const char* const TABLES_MARKER = "# ---- tables: edited on the Tables tab ----";

namespace {

std::string trimmed(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    const std::size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> out{std::string()};
    for (const char c : text) {
        if (c == '\n') out.push_back(std::string());
        else if (c != '\r') out.back().push_back(c);
    }
    return out;
}

bool isComment(const std::string& line) {
    const std::string t = trimmed(line);
    return !t.empty() && (t[0] == '#' || t[0] == ';');
}

bool isHeader(const std::string& line) {
    const std::string t = trimmed(line);
    return !t.empty() && t[0] == '[';
}

bool isTableType(const std::string& type) {
    return type == "Variable" || type == "Schedule" || type == "ScheduleRow";
}

std::vector<std::string> splitList(const std::string& text) {
    std::vector<std::string> out;
    std::string item;
    for (const char c : text) {
        if (c == ',') { out.push_back(trimmed(item)); item.clear(); }
        else item.push_back(c);
    }
    out.push_back(trimmed(item));
    return out;
}

std::size_t asCount(const std::string& s, std::size_t fallback) {
    if (s.empty()) return fallback;
    const long long v = std::strtoll(s.c_str(), nullptr, 10);
    return v < 1 ? fallback : static_cast<std::size_t>(v);
}

bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.';
}

// Whole-word occurrences of `word` in `s`.
bool mentions(const std::string& s, const std::string& word) {
    for (std::size_t at = s.find(word); at != std::string::npos; at = s.find(word, at + 1)) {
        const bool before = at == 0 || !isWordChar(s[at - 1]);
        const bool after = at + word.size() >= s.size() || !isWordChar(s[at + word.size()]);
        if (before && after) return true;
    }
    return false;
}

std::string field(const std::string& key, const std::string& value, std::size_t width) {
    std::string k = key;
    k.resize(std::max(width, key.size()), ' ');
    return k + " = " + value + "\n";
}

}  // namespace

const char* kindName(TableKind k) {
    switch (k) {
        case TableKind::Variable: return "Variable";
        case TableKind::Array:    return "Array";
        case TableKind::Matrix:   return "Matrix";
        case TableKind::Schedule: return "Schedule";
    }
    return "Variable";
}

void Table::resize(std::size_t newRows, std::size_t newColumns) {
    std::vector<std::string> next(newRows * newColumns, "0");
    for (std::size_t r = 0; r < std::min(rows, newRows); ++r)
        for (std::size_t c = 0; c < std::min(columns, newColumns); ++c)
            next[r * newColumns + c] = at(r, c);
    rows = newRows;
    columns = newColumns;
    cells = std::move(next);
}

bool Table::resizeLoses(std::size_t newRows, std::size_t newColumns) const {
    for (std::size_t r = 0; r < rows; ++r)
        for (std::size_t c = 0; c < columns; ++c) {
            if (r < newRows && c < newColumns) continue;
            const std::string& v = at(r, c);
            if (!v.empty() && std::strtod(v.c_str(), nullptr) != 0.0) return true;
        }
    return false;
}

SplitResult splitTables(const std::string& fileText) {
    const std::vector<std::string> lines = linesOf(fileText);
    const ReadResult read = readDocument(fileText);
    const ModelDocument& doc = read.document;

    std::vector<bool> drop(lines.size(), false);
    // header line (0-based) -> the note found above it
    std::map<std::size_t, std::string> noteAt;

    for (const char* type : {"Variable", "Schedule", "ScheduleRow"}) {
        for (const ModelDocument::Row& row : doc.rows(type)) {
            if (row.headerLine == 0 || row.headerLine > lines.size()) continue;
            const std::size_t h = row.headerLine - 1;
            std::size_t end = h + 1;
            while (end < lines.size() && !isHeader(lines[end])) ++end;
            // Comments right above the NEXT header are that header's.
            while (end > h + 1 && isComment(lines[end - 1])) --end;
            std::size_t start = h;
            while (start > 0 && isComment(lines[start - 1])) --start;
            std::string note;
            for (std::size_t i = start; i < h; ++i) {
                std::string t = trimmed(lines[i]);
                if (t == TABLES_MARKER) continue;
                t = trimmed(t.substr(1));
                note += (note.empty() ? "" : "\n") + t;
            }
            noteAt[h] = note;
            for (std::size_t i = start; i < end; ++i) drop[i] = true;
        }
    }
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (trimmed(lines[i]) == TABLES_MARKER) drop[i] = true;

    // Tables in the order their headers appear. A schedule's rows follow it
    // wherever they were written.
    SplitResult out;
    std::map<std::string, std::size_t> scheduleIndex;
    struct Pending { std::size_t line; std::string type; std::size_t row; };
    std::vector<Pending> order;
    for (const char* type : {"Variable", "Schedule", "ScheduleRow"})
        for (std::size_t r = 0; r < doc.rowCount(type); ++r)
            order.push_back({doc.rows(type)[r].headerLine, type, r});
    std::stable_sort(order.begin(), order.end(),
                     [](const Pending& a, const Pending& b) { return a.line < b.line; });

    const auto noteFor = [&noteAt](std::size_t headerLine) {
        const auto it = noteAt.find(headerLine == 0 ? 0 : headerLine - 1);
        return it == noteAt.end() ? std::string() : it->second;
    };
    const auto scheduleNamed = [&](const std::string& name, const std::string& note) -> Table& {
        const auto it = scheduleIndex.find(name);
        if (it != scheduleIndex.end()) return out.tables[it->second];
        Table t;
        t.kind = TableKind::Schedule;
        t.name = name;
        t.note = note;
        t.rows = 0;
        t.columns = 2;
        scheduleIndex[name] = out.tables.size();
        out.tables.push_back(std::move(t));
        return out.tables.back();
    };

    for (const Pending& p : order) {
        const auto cell = [&](const char* c) { return trimmed(doc.cell(p.type, p.row, c)); };
        if (p.type == "Variable") {
            Table t;
            t.name = cell("Name");
            t.note = noteFor(p.line);
            const std::string rows = cell("Rows");
            const std::string cols = cell("Columns");
            std::string init = cell("Initial Value");
            if (init.empty()) init = "0";
            if (rows.empty()) {
                t.kind = TableKind::Variable;
                t.cells = {init};
            } else {
                t.rows = asCount(rows, 1);
                t.columns = asCount(cols, 1);
                t.kind = t.columns == 1 ? TableKind::Array : TableKind::Matrix;
                std::vector<std::string> values = splitList(init);
                const std::size_t n = t.rows * t.columns;
                if (values.size() == 1) values.assign(n, values[0]);
                values.resize(n, "0");
                t.cells = values;
            }
            out.tables.push_back(std::move(t));
        } else if (p.type == "Schedule") {
            Table& t = scheduleNamed(cell("Name"), noteFor(p.line));
            if (!cell("Time Units").empty()) t.timeUnits = cell("Time Units");
            if (!cell("Type").empty()) t.scheduleType = cell("Type");
            if (t.note.empty()) t.note = noteFor(p.line);
        } else {
            Table& t = scheduleNamed(cell("Schedule"), std::string());
            std::string v = cell("Value"), d = cell("Duration");
            t.cells.push_back(v.empty() ? "0" : v);
            t.cells.push_back(d.empty() ? "0" : d);
            ++t.rows;
        }
    }
    // A schedule with no rows still needs one to be editable.
    for (Table& t : out.tables)
        if (t.kind == TableKind::Schedule && t.rows == 0) t.resize(1, 2);

    std::string model;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (drop[i]) continue;
        model += lines[i];
        if (i + 1 < lines.size()) model += "\n";
    }
    // No runs of blank lines where a table used to be, and none at the end.
    std::string squeezed;
    int blanks = 0;
    for (const std::string& l : linesOf(model)) {
        blanks = trimmed(l).empty() ? blanks + 1 : 0;
        if (blanks <= 1) squeezed += l + "\n";
    }
    // Exactly one final newline, or nothing at all for an empty model.
    while (!squeezed.empty() && (squeezed.back() == '\n' || squeezed.back() == ' '))
        squeezed.pop_back();
    if (!squeezed.empty()) squeezed += "\n";
    out.modelText = squeezed;
    return out;
}

namespace {

std::string tableText(const Table& t) {
    std::string s;
    if (!t.note.empty())
        for (const std::string& l : linesOf(t.note)) s += "# " + l + "\n";
    if (t.kind == TableKind::Schedule) {
        s += "[Schedule]\n";
        s += field("Name", t.name, 10);
        s += field("Type", t.scheduleType, 10);
        s += field("Time Units", t.timeUnits, 10);
        for (std::size_t r = 0; r < t.rows; ++r) {
            s += "\n[ScheduleRow]\n";
            s += field("Schedule", t.name, 8);
            s += field("Value", t.at(r, 0), 8);
            s += field("Duration", t.at(r, 1), 8);
        }
        return s;
    }
    s += "[Variable]\n";
    s += field("Name", t.name, 13);
    if (t.kind == TableKind::Variable) {
        s += field("Initial Value", t.cells.empty() ? "0" : t.cells[0], 13);
        return s;
    }
    s += field("Rows", std::to_string(t.rows), 13);
    if (t.kind == TableKind::Matrix) s += field("Columns", std::to_string(t.columns), 13);
    std::string values;
    for (std::size_t r = 0; r < t.rows; ++r)
        for (std::size_t c = 0; c < t.columns; ++c) {
            if (r > 0 || c > 0) values += (c == 0 ? ",  " : ", ");
            values += t.at(r, c);
        }
    s += field("Initial Value", values, 13);
    return s;
}

}  // namespace

std::string joinTables(const std::string& modelText, const std::vector<Table>& tables) {
    // No tables: the text exactly as typed. A model that never had a table is
    // saved byte for byte as it always was.
    if (tables.empty()) return modelText;
    std::string model = modelText;
    while (!model.empty() && (model.back() == '\n' || model.back() == ' ')) model.pop_back();
    std::string out = model.empty() ? std::string() : model + "\n\n";
    out += std::string(TABLES_MARKER) + "\n";
    for (std::size_t i = 0; i < tables.size(); ++i) {
        out += "\n";
        out += tableText(tables[i]);
    }
    return out;
}

std::vector<std::pair<std::size_t, std::size_t>>
tableLineRanges(const std::string& modelText, const std::vector<Table>& tables) {
    std::vector<std::pair<std::size_t, std::size_t>> out;
    if (tables.empty()) return out;
    std::string model = modelText;
    while (!model.empty() && (model.back() == '\n' || model.back() == ' ')) model.pop_back();
    std::size_t line = model.empty() ? 1 : linesOf(model).size() + 2;   // the marker
    ++line;
    for (const Table& t : tables) {
        const std::size_t first = line + 1;                 // after the blank line
        const std::size_t count = linesOf(tableText(t)).size() - 1;
        out.emplace_back(first, first + count);
        line = first + count;
    }
    return out;
}

std::vector<std::vector<std::string>> parsePastedBlock(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    for (const std::string& raw : linesOf(text)) {
        const std::string line = trimmed(raw);
        if (line.empty()) continue;
        std::vector<std::string> cells;
        std::string item;
        const bool tabs = line.find('\t') != std::string::npos;
        const bool commas = !tabs && line.find_first_of(",;") != std::string::npos;
        for (const char c : line) {
            const bool split = tabs ? c == '\t'
                             : commas ? (c == ',' || c == ';')
                                      : (c == ' ');
            if (split) {
                if (tabs || commas || !item.empty()) cells.push_back(trimmed(item));
                item.clear();
            } else {
                item.push_back(c);
            }
        }
        if (tabs || commas || !item.empty()) cells.push_back(trimmed(item));
        rows.push_back(cells);
    }
    return rows;
}

bool isNumberCell(const std::string& s) {
    const std::string t = trimmed(s);
    if (t.empty()) return false;
    char* end = nullptr;
    std::strtod(t.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool isTableName(const std::string& s) {
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (const char c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

std::string renameInText(const std::string& text, const std::string& from,
                         const std::string& to) {
    if (from.empty() || from == to) return text;
    const std::vector<std::string> lines = linesOf(text);
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string line = lines[i];
        const std::size_t eq = line.find('=');
        if (!isComment(line) && !isHeader(line) && eq != std::string::npos) {
            std::string value = line.substr(eq + 1);
            std::string rebuilt;
            std::size_t at = 0;
            for (std::size_t hit = value.find(from); hit != std::string::npos;
                 hit = value.find(from, hit + 1)) {
                const bool before = hit == 0 || !isWordChar(value[hit - 1]);
                const bool after = hit + from.size() >= value.size() ||
                                   !isWordChar(value[hit + from.size()]);
                if (!before || !after) continue;
                rebuilt += value.substr(at, hit - at) + to;
                at = hit + from.size();
            }
            rebuilt += value.substr(at);
            line = line.substr(0, eq + 1) + rebuilt;
        }
        out += line;
        if (i + 1 < lines.size()) out += "\n";
    }
    return out;
}

std::vector<std::string> usersOf(const ModelDocument& doc, const std::string& name) {
    std::vector<std::string> out;
    for (const std::string& type : doc.types()) {
        if (isTableType(type)) continue;
        for (std::size_t r = 0; r < doc.rowCount(type); ++r) {
            bool uses = false;
            for (const auto& kv : doc.rows(type)[r].cells)
                if (kv.first != "Name" && mentions(kv.second.text, name)) uses = true;
            if (!uses) continue;
            const std::string label = doc.cell(type, r, "Name");
            out.push_back(type + " " + (label.empty() ? "#" + std::to_string(r + 1) : label));
        }
    }
    return out;
}

}  // namespace des
