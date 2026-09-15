#include <map>
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

    // The preamble a document was GIVEN, as opposed to one it was read with.
    // This writer runs only for a document with no source lines -- one built
    // in memory -- so anything here was set by a caller and dropping it loses
    // work. v14's starter model is the first caller to set one, and its
    // comments went nowhere until this existed.
    //
    // A read file keeps its version line IN the preamble, and writeMerged
    // emits no separate one. Skipping it here means a preamble that came from
    // either place writes the line exactly once.
    for (const std::string& line : doc.preamble()) {
        const std::size_t at = line.find_first_not_of(" \t");
        if (at != std::string::npos && line.compare(at, 7, "version") == 0) continue;
        out << line << "\n";
    }

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
    for (const std::string& line : doc.trailer()) out << line << "\n";
    return out.str();
}

// v13: give every source line an owner, so an edit to one record does not
// reformat the rest of the file.
//
// A record owns the blank line and comments that PRECEDE its header, not the
// ones that follow its last cell. That way a record carries its own annotation
// when it moves and takes it away when it is deleted -- which is what makes the
// merged writer below correct under insert, delete and move rather than only
// under a cell edit.
void attributeSourceLines(const std::vector<std::string>& lines, ModelDocument& doc) {
    std::vector<std::string> preamble;
    std::vector<std::string> pending;   // blanks and comments with no owner yet
    std::vector<std::string> block;     // the record being accumulated
    std::string              blockType;
    std::size_t              blockIndex = 0;
    bool                     haveRecord = false;
    std::map<std::string, std::size_t> seen;

    const auto closeRecord = [&]() {
        if (!haveRecord) return;
        doc.rowAt(blockType, blockIndex).source = block;
        block.clear();
        haveRecord = false;
    };

    for (const std::string& raw : lines) {
        const std::string line = trim(raw);
        const bool blank   = line.empty();
        const bool comment = !blank && (line[0] == '#' || line[0] == ';');
        const bool header  = !blank && line[0] == '[' && line.find(']') != std::string::npos;

        if (blank || comment) { pending.push_back(raw); continue; }

        if (header) {
            closeRecord();
            const std::string type = trim(line.substr(1, line.find(']') - 1));
            if (type.empty()) { pending.push_back(raw); continue; }
            blockType  = type;
            blockIndex = seen[type]++;
            if (blockIndex >= doc.rowCount(type)) { pending.push_back(raw); continue; }
            haveRecord = true;
            block = pending;
            pending.clear();
            block.push_back(raw);
            continue;
        }

        // A key = value line. Inside a record it belongs to it, comments and
        // all; before the first one it is the version line.
        if (haveRecord) {
            for (const std::string& p : pending) block.push_back(p);
            pending.clear();
            block.push_back(raw);
        } else {
            for (const std::string& p : pending) preamble.push_back(p);
            pending.clear();
            preamble.push_back(raw);
        }
    }
    closeRecord();
    doc.setPreamble(std::move(preamble));
    doc.setTrailer(std::move(pending));   // whatever trailed the last record
}

// One record, in canonical form. Extracted from writeCanonical so the merged
// writer below can emit a single edited record without rebuilding the file.
void writeRecord(std::ostringstream& out, const ModelDocument& doc,
                 const std::string& type, std::size_t i) {
    const ModuleSchema* schema = ModuleRegistry::instance().find(type);
    out << "\n[" << type << "]\n";
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

// An edited document, written record by record: verbatim where the row was not
// touched, canonical where it was.
//
// Records come out grouped by module type, which is the order the document
// holds them in. For a file that already groups its records -- every one this
// project ships, and everything writeCanonical produces -- that is the order
// they were read in. A file that INTERLEAVED types would be regrouped by an
// edit; it still round-trips byte-identically while untouched, which is the
// guarantee that matters.
std::string writeMerged(const ModelDocument& doc) {
    std::ostringstream out;
    for (const std::string& line : doc.preamble()) out << line << "\n";

    for (const std::string& type : doc.types()) {
        for (std::size_t i = 0; i < doc.rowCount(type); ++i) {
            const ModelDocument::Row& row = doc.rows(type)[i];
            if (!row.edited && !row.source.empty()) {
                for (const std::string& line : row.source) out << line << "\n";
            } else {
                writeRecord(out, doc, type, i);
            }
        }
    }

    for (const std::string& line : doc.trailer()) out << line << "\n";
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
            result.document.rowAt(currentType, currentRow).headerLine = lineNo;
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

    // A file with NOTHING in it is not a malformed model file, it is an empty
    // one, and there is nothing to be wrong about. v15's blank page drew a red
    // error marker beside line 1 of a file the person had not typed into yet,
    // which is a poor way to say hello. A file of only comments counts as empty
    // too; it still fails the structure pass for having no blocks.
    bool anyContent = false;
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (!t.empty() && t[0] != '#' && t[0] != ';') { anyContent = true; break; }
    }
    if (!sawVersion && anyContent)
        error(result.diagnostics, 1,
              "no 'version = 1' line: a model file must say what it is");

    // Attribute BEFORE setSourceLines, which clears the edited flags: the
    // attributor writes into the rows, and a row marked edited would then be
    // re-emitted canonically on the first save.
    attributeSourceLines(lines, result.document);

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
    if (doc.sourceLines().empty()) return writeCanonical(doc);

    if (!doc.edited()) {
        std::string out;
        for (const std::string& line : doc.sourceLines()) {
            out += line;
            out += "\n";
        }
        return out;
    }
    // Edited, but read from a file: keep every record nobody touched
    // exactly as it was read, comments and spacing included.
    return writeMerged(doc);
}

bool writeDocumentFile(const ModelDocument& doc, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << writeDocument(doc);
    return out.good();
}

}  // namespace des
