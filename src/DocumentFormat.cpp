#include "DocumentFormat.hpp"
#include <fstream>
#include <sstream>
#include "ModuleSchema.hpp"

namespace des {
namespace {

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    const std::size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text) {
        if (c == '\n') { lines.push_back(current); current.clear(); }
        else if (c != '\r') { current.push_back(c); }
    }
    // A trailing newline ends the last line rather than starting an empty one.
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void error(std::vector<Diagnostic>& out, std::size_t line, const std::string& message) {
    // The span's offset is the LINE NUMBER here, not a character offset: a
    // reader diagnostic is about a whole line, and the cell-relative offsets
    // v10 produces only make sense once there is a cell to be relative to.
    out.push_back(Diagnostic{Severity::Error, SourceSpan{line, 0}, message});
}

// Canonical form, used for a document built in code or one that has been
// edited. Cells come out in schema column order so two documents holding the
// same model produce the same file.
std::string writeCanonical(const ModelDocument& doc) {
    const ModuleRegistry& reg = ModuleRegistry::instance();
    std::ostringstream out;
    out << "version = 1\n";

    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        const std::size_t rows = doc.rowCount(type);
        for (std::size_t i = 0; i < rows; ++i) {
            out << "\n[" << type << "]\n";

            // Schema order first, then anything else the row carries. The
            // second part is what keeps an unknown column from being dropped.
            std::vector<std::string> written;
            if (schema != nullptr) {
                for (const Column& c : schema->columns) {
                    if (!doc.hasCell(type, i, c.id)) continue;
                    out << c.id << " = " << doc.cell(type, i, c.id) << "\n";
                    written.push_back(c.id);
                }
            }
            for (const auto& kv : doc.rows(type)[i].cells) {
                bool already = false;
                for (const std::string& w : written) if (w == kv.first) already = true;
                if (already) continue;
                out << kv.first << " = " << kv.second.text << "\n";
            }
        }
    }
    return out.str();
}

}  // namespace

ReadResult readDocument(const std::string& text) {
    ReadResult result;
    const std::vector<std::string> lines = splitLines(text);

    std::string currentType;
    std::size_t currentRow = 0;
    bool        sawVersion = false;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t lineNo = i + 1;
        const std::string line = trim(lines[i]);

        if (line.empty()) continue;
        if (line[0] == '#' || line[0] == ';') continue;

        if (line[0] == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string::npos) {
                error(result.diagnostics, lineNo,
                      "unclosed module header: expected ']'");
                continue;
            }
            currentType = trim(line.substr(1, close - 1));
            if (currentType.empty()) {
                error(result.diagnostics, lineNo, "empty module header");
                continue;
            }
            result.document.addRow(currentType);
            currentRow = result.document.rowCount(currentType) - 1;
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            error(result.diagnostics, lineNo,
                  "expected 'key = value', or a [Module] header");
            continue;
        }
        const std::string key   = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (currentType.empty()) {
            if (key == "version") {
                sawVersion = true;
                result.formatVersion = std::atoi(value.c_str());
                if (result.formatVersion != 1)
                    error(result.diagnostics, lineNo,
                          "unsupported format version '" + value + "'; this engine reads 1");
                continue;
            }
            error(result.diagnostics, lineNo,
                  "'" + key + "' appears before any [Module] header");
            continue;
        }
        if (key.empty()) {
            error(result.diagnostics, lineNo, "a cell with no column name");
            continue;
        }
        result.document.setCell(currentType, currentRow, key, value, lineNo);
    }

    if (!sawVersion)
        error(result.diagnostics, 1,
              "no 'version = 1' line: a model file must say what it is");

    // Last, so the edited flag is cleared: reading is not an edit, and an
    // untouched document must be writable back verbatim.
    result.document.setSourceLines(lines);
    return result;
}

ReadResult readDocumentFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ReadResult r;
        error(r.diagnostics, 0, "cannot open '" + path + "'");
        return r;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return readDocument(buffer.str());
}

std::string writeDocument(const ModelDocument& doc) {
    // Unedited and read from a file: give back exactly what was read. This is
    // the only way comments and spacing survive, and it is what stops a front
    // end rewriting a file somebody merely looked at.
    if (!doc.edited() && !doc.sourceLines().empty()) {
        std::string out;
        for (const std::string& line : doc.sourceLines()) {
            out += line;
            out += "\n";
        }
        return out;
    }
    return writeCanonical(doc);
}

bool writeDocumentFile(const ModelDocument& doc, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << writeDocument(doc);
    return out.good();
}

}  // namespace des
