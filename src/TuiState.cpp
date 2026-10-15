#include "TuiState.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>
#include "DocumentFormat.hpp"
#include "RunSetup.hpp"

namespace des {

namespace {

std::string trimmed(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    const std::size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

// The same word the reference pass uses in its error, so "no block named 'Out'"
// and "no block exists yet" plainly concern one thing.
std::string nameOfTarget(const std::string& targetType) {
    return targetType == "Block" ? std::string("block") : targetType;
}

}  // namespace

std::string keyOfLine(const std::string& line) {
    const std::string t = trimmed(line);
    if (t.empty() || t[0] == '#' || t[0] == ';' || t[0] == '[') return std::string();
    const std::size_t eq = t.find('=');
    if (eq == std::string::npos) return std::string();
    return trimmed(t.substr(0, eq));
}

std::string blockTypeAt(const TextBuffer& buffer, std::size_t line) {
    for (std::size_t i = line + 1; i-- > 0;) {
        const std::string t = trimmed(buffer.lineAt(i));
        if (t.size() < 2 || t[0] != '[') continue;
        const std::size_t close = t.find(']');
        if (close == std::string::npos) continue;
        return trimmed(t.substr(1, close - 1));
    }
    return std::string();
}

std::vector<std::string> templateFor(const ModuleSchema& schema) {
    std::vector<std::string> out;
    out.push_back("[" + schema.typeName + "]");
    // EVERY column, blank, rather than only the required ones. A person who
    // has used Arena expects the dialog to list what a module can do; a
    // template that hid the optional half would hide balking, reneging and
    // entity types behind knowing they exist.
    for (const Column& c : schema.columns) out.push_back(c.id + " = ");
    out.push_back(std::string());
    return out;
}

std::vector<FlowBlock> flowOf(const ModelDocument& doc) {
    const ModuleRegistry& reg = ModuleRegistry::instance();
    std::vector<FlowBlock> blocks;

    // A Decide's Next is its ELSE exit, taken only when no branch matched, so
    // it has to be listed AFTER the branches. Printed first it read as the
    // default path tried first, which is the opposite of what happens -- and
    // this view exists to make exactly that kind of thing visible.
    std::vector<std::pair<std::size_t, std::string>> elseExits;

    // Registry order rather than document order, so a Create is at the top
    // whatever order the file happens to list its records in.
    for (const ModuleSchema& schema : reg.all()) {
        if (schema.kind != ModuleKind::Flowchart) continue;
        for (std::size_t r = 0; r < doc.rowCount(schema.typeName); ++r) {
            FlowBlock b;
            b.name = doc.cell(schema.typeName, r, "Name");
            b.type = schema.typeName;
            b.row  = r;
            b.line = doc.rows(schema.typeName)[r].headerLine;
            for (const Column& c : schema.columns) {
                if (c.type != ColumnType::Reference || c.referencedType != "Block")
                    continue;
                const std::string target = doc.cell(schema.typeName, r, c.id);
                if (target.empty()) continue;
                if (schema.typeName == "Decide" && c.id == "Next")
                    elseExits.emplace_back(blocks.size(), "else -> " + target);
                else
                    b.exits.push_back(c.id + " -> " + target);
            }
            // v16: a Route's Destination is where it goes, though no line
            // is drawn; the flow view is where that should be visible.
            if (schema.typeName == "Route") {
                const std::string to = doc.cell("Route", r, "Destination");
                if (!to.empty()) b.exits.push_back("to -> " + to);
            }
            const bool isMatch = schema.typeName == "Match";
            if (isMatch) {
                // Listed, not arrows: a Match is entered through its lanes,
                // so naming them must not count as reaching them.
                for (std::size_t l = 0; l < doc.rowCount("MatchLane"); ++l)
                    if (doc.cell("MatchLane", l, "Match") == b.name)
                        b.exits.push_back("lane " + doc.cell("MatchLane", l, "Name"));
            }
            const std::string matchName = b.name;
            blocks.push_back(std::move(b));

            // Each lane straight after its Match, as a block of its own --
            // it is what other blocks route to, and it has its own exit.
            if (isMatch) {
                for (std::size_t l = 0; l < doc.rowCount("MatchLane"); ++l) {
                    if (doc.cell("MatchLane", l, "Match") != matchName) continue;
                    FlowBlock lane;
                    lane.name = doc.cell("MatchLane", l, "Name");
                    lane.type = "MatchLane";
                    lane.row  = l;
                    lane.line = doc.rows("MatchLane")[l].headerLine;
                    const std::string next = doc.cell("MatchLane", l, "Next");
                    if (!next.empty()) lane.exits.push_back("Next -> " + next);
                    blocks.push_back(std::move(lane));
                }
            }
        }
    }

    // A Decide's branches live in their own table, keyed by the Decide's name.
    if (const ModuleSchema* branch = reg.find("DecideBranch")) {
        for (std::size_t r = 0; r < doc.rowCount("DecideBranch"); ++r) {
            const std::string owner = doc.cell("DecideBranch", r, branch->parentColumn);
            const std::string to    = doc.cell("DecideBranch", r, "To");
            const std::string cond  = doc.cell("DecideBranch", r, "Condition");
            const std::string prob  = doc.cell("DecideBranch", r, "Probability");
            if (to.empty()) continue;
            std::string label = cond;
            if (label.empty() && !prob.empty()) label = "p=" + prob;
            if (label.empty()) label = "else";
            for (FlowBlock& b : blocks)
                if (b.name == owner) b.exits.push_back(label + " -> " + to);
        }
    }
    for (const auto& kv : elseExits)
        if (kv.first < blocks.size()) blocks[kv.first].exits.push_back(kv.second);

    // Two things worth seeing, and neither is an error on its own: an exit
    // that names nothing (which the compiler also reports, but not as a
    // picture), and a block nothing arrives at.
    const std::vector<std::string> declared = referenceCandidates(doc, "Block");
    const auto exists = [&declared](const std::string& n) {
        return std::find(declared.begin(), declared.end(), n) != declared.end();
    };
    std::vector<std::string> targets;
    for (const FlowBlock& b : blocks)
        for (const std::string& e : b.exits) {
            const std::size_t at = e.rfind(" -> ");
            if (at != std::string::npos) targets.push_back(e.substr(at + 4));
        }
    const std::vector<std::string> stations = referenceCandidates(doc, "Station");
    for (FlowBlock& b : blocks) {
        for (const std::string& e : b.exits) {
            const std::size_t at = e.rfind(" -> ");
            if (at == std::string::npos) continue;
            const std::string to = e.substr(at + 4);
            const bool known = e.compare(0, 6, "to -> ") == 0
                                   ? std::find(stations.begin(), stations.end(), to) != stations.end()
                                   : exists(to);
            if (!known) b.dangling = true;
        }
        // A Create is where entities come FROM, so nothing pointing at it is
        // the normal case rather than something to flag.
        b.unreached = b.type != "Create" && b.type != "Match" && !b.name.empty() &&
                      std::find(targets.begin(), targets.end(), b.name) == targets.end();
    }
    return blocks;
}

std::string starterModelText() {
    return
        "version = 1\n"
        "\n"
        "# A single teller: entities arrive, wait for the one server, are\n"
        "# served, and leave. The smallest complete model, and every value in\n"
        "# it is meant to be changed.\n"
        "#\n"
        "# ^R runs it. Lines starting with # are comments.\n"
        "\n"
        "[Run]\n"
        "Name = Baseline\n"
        "Length = 480\n"
        "Replications = 1\n"
        "\n"
        "[Create]\n"
        "Name = Arrivals\n"
        "Interarrival = EXPO(1.0)\n"
        "Next = Serve\n"
        "\n"
        "[Process]\n"
        "Name = Serve\n"
        "Capacity = 1\n"
        "Discipline = FIFO\n"
        "Service = EXPO(0.8)\n"
        "Next = Out\n"
        "\n"
        "[Dispose]\n"
        "Name = Out\n";
}

ModelDocument starterModel() {
    return readDocument(starterModelText()).document;
}

// ---------------------------------------------------------------------------

TuiState TuiState::fromText(const std::string& text, std::string path) {
    TuiState s;
    // v16.1: the tables come out of the text and onto the Tables tab. What is
    // left is what the Model tab edits.
    SplitResult split = splitTables(text);
    s.m_buffer.setText(split.tables.empty() ? text : split.modelText);
    s.m_tables = std::move(split.tables);
    s.m_path = std::move(path);
    s.rebuildPalette();
    s.reparse();
    return s;
}

TuiState TuiState::open(const std::string& path) {
    // is_open, NOT good: on the GCC this project builds with, constructing an
    // ifstream on a missing file leaves good() reporting true. v12's
    // regression harness lost an afternoon to exactly that.
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        TuiState s = fromText(std::string(), path);
        s.setStatus(path + ": new file -- pick a module on the left, or ^T for "
                           "a working one");
        return s;
    }
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    TuiState s = fromText(text, path);
    s.setStatus(hasErrors(s.m_diagnostics)
                    ? path + ": opened, and it does not compile -- ^J goes to "
                             "the first error"
                    : path);
    return s;
}

void TuiState::rebuildPalette() {
    m_palette.clear();
    for (const ModuleSchema& schema : ModuleRegistry::instance().all()) {
        // A read-only type has nothing to insert: Queue is a view of a Process
        // row, and a [Queue] record in a file would be a second place to say
        // the same thing.
        if (schema.readOnly) continue;
        // v16.1: these are made on the Tables tab.
        if (schema.typeName == "Variable" || schema.typeName == "Schedule" ||
            schema.typeName == "ScheduleRow") continue;
        m_palette.push_back(schema.typeName);
    }
    m_runFields.clear();
    if (const ModuleSchema* run = ModuleRegistry::instance().find("Run"))
        for (const Column& c : run->columns) m_runFields.push_back(c.id);
}

void TuiState::reparse() {
    ReadResult read = readDocument(joinedText());
    m_document    = std::move(read.document);
    m_diagnostics = std::move(read.diagnostics);

    // The reader's diagnostics FIRST, then the compiler's. A file that does
    // not parse cannot be compiled meaningfully, but the compiler is run
    // anyway on whatever did parse: half a model with its errors named beats
    // one message about a stray bracket.
    CompileResult compiled = compile(m_document);
    for (Diagnostic& d : compiled.diagnostics) m_diagnostics.push_back(std::move(d));

    // A diagnostic past the model text is about a table.
    m_tableIssues.clear();
    const auto ranges = tableLineRanges(m_buffer.text(), m_tables);
    for (const Diagnostic& d : m_diagnostics) {
        const std::size_t line = lineOf(m_document, d);
        for (std::size_t i = 0; i < ranges.size(); ++i)
            if (line >= ranges[i].first && line < ranges[i].second)
                m_tableIssues.push_back(TableIssue{i, d.severity, d.message});
    }

    m_runs = runNames(m_document);
    if (m_runIndex >= m_runs.size()) m_runIndex = m_runs.empty() ? 0 : m_runs.size() - 1;

    const std::vector<FlowBlock> flow = flowOf(m_document);
    if (m_flowCursor >= flow.size()) m_flowCursor = flow.empty() ? 0 : flow.size() - 1;
}

void TuiState::edited() {
    m_dirty = true;
    reparse();
}

void TuiState::setView(View v) {
    // The STATUS GOES WITH THE VIEW it was about. "type it instead -- the name
    // does not have to exist yet" is good advice under the editor and noise
    // under the Runs tab, and it sat there through every other view because
    // nothing ever cleared it.
    if (v != m_view) m_status.clear();
    m_view = v;
    m_overlay = Overlay::None;
    if (v == View::Model) m_focus = Pane::Editor;
}

// --- editing ---------------------------------------------------------------

void TuiState::insertText(const std::string& text) { m_buffer.insert(text); edited(); }
void TuiState::typeChar(char c) { m_buffer.insert(std::string(1, c)); edited(); }
void TuiState::newline()   { m_buffer.newline();   edited(); }
void TuiState::backspace() { m_buffer.backspace(); edited(); }
void TuiState::del()       { m_buffer.del();       edited(); }

void TuiState::undo() {
    if (!m_buffer.canUndo()) { setStatus("nothing to undo"); return; }
    m_buffer.undo();
    // DIRTY STAYS TRUE, on purpose. Undoing back to what is on disk is not the
    // same as knowing you are there -- that needs a saved-at marker in the
    // stack -- and claiming "not dirty" wrongly is how work gets lost.
    edited();
    setStatus("undone");
}

void TuiState::redo() {
    if (!m_buffer.canRedo()) { setStatus("nothing to redo"); return; }
    m_buffer.redo();
    edited();
    setStatus("redone");
}

void TuiState::cut() {
    if (!m_buffer.hasSelection()) { setStatus("nothing selected"); return; }
    m_clip = m_buffer.selectedText();
    m_buffer.checkpoint();
    m_buffer.deleteSelection();
    edited();
    setStatus("cut");
}

void TuiState::copy() {
    if (!m_buffer.hasSelection()) { setStatus("nothing selected"); return; }
    m_clip = m_buffer.selectedText();
    setStatus("copied");
}

void TuiState::paste() {
    if (m_clip.empty()) { setStatus("the clipboard is empty"); return; }
    m_buffer.insert(m_clip);
    edited();
    setStatus("pasted");
}

bool TuiState::save() {
    if (m_unnamed) { openPrompt(PromptFor::SaveAs); return false; }
    return save(m_path);
}

// --- naming ------------------------------------------------------------------

std::string resolveModelName(const std::string& typed, const std::string& defaultDir) {
    const std::string name = trimmed(typed);
    if (name.empty()) return std::string();
    const bool hasFolder = name.find_first_of("/\\") != std::string::npos;
    std::string path = hasFolder ? name : defaultDir + "/" + name;
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        path += ".des";
    return path;
}

void TuiState::requestNewFile() {
    if (m_dirty) {
        m_confirmFor = ConfirmFor::NewFile;
        m_overlay = Overlay::Confirm;
        setStatus("unsaved changes -- (s)ave first, (d)iscard them, (c)ancel");
        return;
    }
    openPrompt(PromptFor::NewFile);
}

void TuiState::openPrompt(PromptFor why) {
    m_promptFor = why;
    m_promptText.clear();
    m_overlay = Overlay::Prompt;
    setStatus(why == PromptFor::SaveAs ? "this model has no name yet -- what should it be called?"
                                       : "the new model's name, e.g. line2");
}

void TuiState::cancelPrompt() {
    m_promptFor = PromptFor::None;
    m_promptText.clear();
    m_overlay = Overlay::None;
    setStatus("");
}

void TuiState::commitPrompt() {
    const std::string path = resolveModelName(m_promptText, m_defaultDir);
    if (path.empty()) { setStatus("type a name first -- Esc cancels"); return; }

    if (m_promptFor == PromptFor::SaveAs) {
        // save(path) only clears dirty when the path is the model's own, so
        // the path changes FIRST.
        const std::string before = m_path;
        m_path = path;
        if (!save(path)) { m_path = before; return; }
        m_unnamed = false;
        m_promptFor = PromptFor::None;
        m_overlay = Overlay::None;
        return;
    }

    std::error_code ignored;
    if (std::filesystem::exists(path, ignored)) {
        m_pendingPath = path;
        m_confirmFor = ConfirmFor::OpenExisting;
        m_overlay = Overlay::Confirm;
        setStatus(path + " exists -- open it instead? (y)es  (n)o");
        return;
    }
    m_promptFor = PromptFor::None;
    newBlankModel(path);
}

void TuiState::answerConfirm(char c) {
    if (m_confirmFor == ConfirmFor::NewFile) {
        if (c == 's') { if (save() && !m_dirty) openPrompt(PromptFor::NewFile); return; }
        if (c == 'd') { openPrompt(PromptFor::NewFile); return; }
        if (c == 'c') { m_overlay = Overlay::None; setStatus(""); }
        return;
    }
    if (m_confirmFor == ConfirmFor::ResizeTable) {
        m_overlay = Overlay::None;
        if (c == 'y' && m_tableIndex < m_tables.size()) {
            tablesChanging();
            m_tables[m_tableIndex].resize(m_pendingRows, m_pendingColumns);
            tablesChanged();
            setStatus("resized");
        } else {
            setStatus("left as it was");
        }
        return;
    }
    if (m_confirmFor == ConfirmFor::DeleteTable) {
        m_overlay = Overlay::None;
        if (c == 'y' && m_tableIndex < m_tables.size()) {
            const std::string name = m_tables[m_tableIndex].name;
            tablesChanging();
            m_tables.erase(m_tables.begin() + static_cast<long>(m_tableIndex));
            tablesChanged();
            setStatus("deleted " + name + " -- ^Z brings it back");
        } else {
            setStatus("");
        }
        return;
    }
    if (m_confirmFor == ConfirmFor::RenameTable) {
        m_overlay = Overlay::None;
        if ((c == 'y' || c == 'n') && m_tableIndex < m_tables.size())
            applyRename(m_pendingName, c == 'y');
        return;
    }
    if (m_confirmFor == ConfirmFor::OpenExisting) {
        if (c == 'y') {
            const std::string path = m_pendingPath;
            replaceWith(open(path));
            return;
        }
        if (c == 'n') {
            // Back to the name, with what was typed still there to correct.
            const std::string typed = m_promptText;
            openPrompt(PromptFor::NewFile);
            m_promptText = typed;
        }
    }
}

// A new or reopened model replaces the whole state -- but not what belongs to
// the SESSION. The first version kept only the models folder, so after ^N the
// picture opener (v16.2) was gone and F2 answered "not available in this
// build" with the saved key forgotten: everything tui/main.cpp installs at
// start-up was dropped with the old model. Anything installed there goes here.
void TuiState::replaceWith(TuiState next) {
    next.m_defaultDir = std::move(m_defaultDir);
    next.m_opener     = std::move(m_opener);
    next.m_askHooks   = std::move(m_askHooks);
    next.m_llm        = std::move(m_llm);
    next.m_clip       = std::move(m_clip);
    *this = std::move(next);
}

void TuiState::newBlankModel(const std::string& path) {
    replaceWith(fromText(std::string(), path));
    setStatus(path + ": a new model -- pick a module on the left, or ^T for a working one");
}

bool TuiState::save(const std::string& toPath) {
    // CREATE THE FOLDER FIRST. Double-clicking des_tui with no file opens
    // <Documents>/DES Models/untitled.des, and on a laptop that has never run
    // it that folder does not exist -- so the first ^S a new person ever
    // pressed answered "could not write". A failure here is not reported on its
    // own: the open below fails too, and says so.
    const std::filesystem::path parent = std::filesystem::path(toPath).parent_path();
    if (!parent.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(parent, ignored);
    }
    std::ofstream out(toPath, std::ios::binary);
    if (!out.is_open()) { setStatus(toPath + ": could not write"); return false; }
    const std::string text = joinedText();
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    // A trailing newline, because every text tool expects one and its absence
    // is the sort of thing that shows up as a diff nobody made.
    if (!text.empty() && text.back() != '\n') out.put('\n');
    if (!out.good()) { setStatus(toPath + ": could not write"); return false; }
    if (toPath == m_path) m_dirty = false;
    setStatus(toPath + ": saved");
    return true;
}

// --- what the text means ----------------------------------------------------

const Diagnostic* TuiState::diagnosticOnLine(std::size_t line) const {
    if (line == 0) return nullptr;
    const Diagnostic* warning = nullptr;
    for (const Diagnostic& d : m_diagnostics) {
        if (lineOf(m_document, d) != line) continue;
        // An ERROR beats a warning on the same line. One marker per line, and
        // the one worth showing is the one that stops the model running.
        if (d.severity == Severity::Error) return &d;
        if (warning == nullptr) warning = &d;
    }
    return warning;
}

std::size_t TuiState::firstErrorLine() const {
    std::size_t best = 0;
    for (const Diagnostic& d : m_diagnostics) {
        if (d.severity != Severity::Error) continue;
        const std::size_t line = lineOf(m_document, d);
        if (line == 0) continue;
        if (best == 0 || line < best) best = line;
    }
    return best;
}

void TuiState::goToFirstError() {
    const std::size_t line = firstErrorLine();
    // In the tables section: the Tables tab, on that table.
    const auto ranges = tableLineRanges(m_buffer.text(), m_tables);
    for (std::size_t i = 0; line > 0 && i < ranges.size(); ++i) {
        if (line < ranges[i].first || line >= ranges[i].second) continue;
        setView(View::Tables);
        m_tablesFocus = TablesFocus::Editor;
        selectTable(i);
        for (const TableIssue& issue : m_tableIssues)
            if (issue.table == i && issue.severity == Severity::Error) {
                setStatus(m_tables[i].name + ": " + issue.message);
                break;
            }
        return;
    }
    if (line == 0) {
        setStatus(hasErrors(m_diagnostics)
                      ? "there is an error, but nothing in the file to point at"
                      : "no errors");
        return;
    }
    setView(View::Model);
    m_focus = Pane::Editor;
    m_buffer.moveTo(Caret{line - 1, 0});
    if (const Diagnostic* d = diagnosticOnLine(line)) setStatus(d->message);
}

// --- the palette ------------------------------------------------------------

void TuiState::setPaletteIndex(std::size_t i) {
    if (i < m_palette.size()) m_paletteIndex = i;
}

void TuiState::stepPalette(long long delta) {
    if (m_palette.empty()) return;
    long long i = static_cast<long long>(m_paletteIndex) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(m_palette.size()) - 1));
    m_paletteIndex = static_cast<std::size_t>(i);
}

void TuiState::insertSelectedModule() {
    if (m_paletteIndex >= m_palette.size()) return;
    const ModuleSchema* schema = ModuleRegistry::instance().find(m_palette[m_paletteIndex]);
    if (schema == nullptr) return;
    m_buffer.insertLines(templateFor(*schema));
    edited();
    m_focus = Pane::Editor;
    setStatus(schema->typeName + " inserted -- fill in the fields, and delete "
                                 "the ones you do not need");
}

void TuiState::showModuleHelp() {
    if (m_paletteIndex >= m_palette.size()) return;
    const ModuleSchema* schema = ModuleRegistry::instance().find(m_palette[m_paletteIndex]);
    if (schema == nullptr) return;
    m_helpTitle = schema->typeName;
    m_helpBody.clear();
    m_helpBody.push_back(schema->help);
    m_helpBody.push_back(std::string());
    for (const Column& c : schema->columns) {
        std::string head = c.id;
        if (c.required) head += "  (required)";
        m_helpBody.push_back(head);
        m_helpBody.push_back("    " + c.help);
        if (c.type == ColumnType::Enum) {
            std::string values = "    one of:";
            for (const std::string& v : c.enumValues) values += " " + v;
            m_helpBody.push_back(values);
        } else if (c.type == ColumnType::Reference) {
            m_helpBody.push_back("    names a " + nameOfTarget(c.referencedType));
        }
        m_helpBody.push_back(std::string());
    }
    m_overlay = Overlay::Help;
}

void TuiState::showFieldHelp() {
    const Caret at = m_buffer.caret();
    const std::string type = blockTypeAt(m_buffer, at.line);
    const std::string key  = keyOfLine(m_buffer.lineAt(at.line));
    const ModuleSchema* schema = type.empty() ? nullptr
                                              : ModuleRegistry::instance().find(type);
    if (schema == nullptr) {
        setStatus(type.empty()
                      ? "the cursor is not inside a module block"
                      : "'" + type + "' is not a module type this build knows");
        return;
    }
    if (key.empty()) {
        // On the header, or on a blank line: the module's own help is the
        // useful answer rather than a refusal.
        m_helpTitle = schema->typeName;
        m_helpBody.clear();
        m_helpBody.push_back(schema->help);
        m_helpBody.push_back(std::string());
        m_helpBody.push_back("Put the cursor on a field and press ^G again for "
                             "that field.");
        m_overlay = Overlay::Help;
        return;
    }
    const Column* col = schema->column(key);
    if (col == nullptr) {
        setStatus("'" + key + "' is not a field on " + schema->typeName +
                  " -- it is kept in the file, and ignored");
        return;
    }
    m_helpTitle = schema->typeName + "." + col->id;
    m_helpBody.clear();
    m_helpBody.push_back(col->help);
    m_helpBody.push_back(std::string());
    if (col->required) m_helpBody.push_back("Required.");
    if (!col->defaultValue.empty())
        m_helpBody.push_back("Left blank it means " + col->defaultValue + ".");
    if (col->type == ColumnType::Enum) {
        std::string values = "One of:";
        for (const std::string& v : col->enumValues) values += " " + v;
        m_helpBody.push_back(values);
        m_helpBody.push_back("^L lists them.");
    } else if (col->type == ColumnType::Reference) {
        m_helpBody.push_back("Names a " + nameOfTarget(col->referencedType) + ".");
        m_helpBody.push_back("^L lists the ones that exist.");
    }
    m_overlay = Overlay::Help;
}

// --- the list ---------------------------------------------------------------

bool TuiState::openPicker() {
    const Caret at = m_buffer.caret();
    const std::string type = blockTypeAt(m_buffer, at.line);
    const std::string key  = keyOfLine(m_buffer.lineAt(at.line));
    if (type.empty() || key.empty()) {
        setStatus("^L lists the values for a field -- put the cursor on one");
        return false;
    }
    const ModuleSchema* schema = ModuleRegistry::instance().find(type);
    const Column* col = schema ? schema->column(key) : nullptr;
    if (col == nullptr) {
        setStatus("nothing to list for '" + key + "'");
        return false;
    }

    m_choices.clear();
    if (col->type == ColumnType::Enum) {
        m_choices = col->enumValues;
    } else if (col->type == ColumnType::Reference) {
        m_choices = referenceCandidates(m_document, col->referencedType);
        if (m_choices.empty()) {
            // NOT an error, and not an empty menu either. On a model being
            // built from scratch this is the ordinary case -- the first Next
            // is typed before anything exists to point at.
            setStatus("no " + nameOfTarget(col->referencedType) +
                      " exists yet -- type the name, then add it");
            return false;
        }
        // An optional exit may be left empty: the engine spells "leaves the
        // system" as a null next. FIRST, so an unwired exit opens at the top
        // of the list rather than scrolled past it.
        if (!col->required) m_choices.insert(m_choices.begin(), std::string());
    } else {
        setStatus(key + " is free text -- there is no list of values for it");
        return false;
    }
    if (m_choices.empty()) return false;

    // Open ON the current value when it is one of the choices, so Enter alone
    // is a no-op rather than a silent change to whatever sorted first.
    const std::string line = m_buffer.lineAt(at.line);
    const std::size_t eq = line.find('=');
    const std::string now = eq == std::string::npos
                                ? std::string()
                                : trimmed(line.substr(eq + 1));
    const auto it = std::find(m_choices.begin(), m_choices.end(), now);
    m_choice = (it == m_choices.end())
                   ? 0
                   : static_cast<std::size_t>(it - m_choices.begin());
    m_pickerFor   = PickerFor::Editor;
    m_pickerTitle = key;
    m_overlay = Overlay::Picker;
    return true;
}

void TuiState::stepPicker(long long delta) {
    if (m_choices.empty()) return;
    long long i = static_cast<long long>(m_choice) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(m_choices.size()) - 1));
    m_choice = static_cast<std::size_t>(i);
}

void TuiState::commitPicker() {
    if (m_choice >= m_choices.size()) { cancelPicker(); return; }
    if (m_pickerFor == PickerFor::TableUnits) {
        const std::string value = m_choices[m_choice];
        m_choices.clear();
        m_overlay = Overlay::None;
        m_pickerFor = PickerFor::Editor;
        if (m_tableIndex < m_tables.size()) setHeaderField(m_tableField, value);
        return;
    }
    if (m_pickerFor == PickerFor::RunField) {
        const std::string value = m_choices[m_choice];
        m_choices.clear();
        m_overlay = Overlay::None;
        m_pickerFor = PickerFor::Editor;
        if (m_runField < m_runFields.size())
            setRunValue(m_runIndex, m_runFields[m_runField], value);
        return;
    }
    const Caret at = m_buffer.caret();
    const std::string line = m_buffer.lineAt(at.line);
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) { cancelPicker(); return; }

    // Rewrite the whole line, keeping the key exactly as it was typed. Editing
    // only the value would leave `Discipline    =FIFO` looking like whatever it
    // looked like before, and the file is the thing a person reads.
    const std::string key = trimmed(line.substr(0, eq));
    m_buffer.checkpoint();
    m_buffer.moveTo(Caret{at.line, 0});
    m_buffer.moveTo(Caret{at.line, line.size()}, /*extend=*/true);
    m_buffer.deleteSelection();
    m_buffer.insert(key + " = " + m_choices[m_choice]);
    m_choices.clear();
    m_overlay = Overlay::None;
    edited();
}

void TuiState::cancelPicker() {
    m_choices.clear();
    m_overlay = Overlay::None;
    m_pickerFor = PickerFor::Editor;
}

// --- runs -------------------------------------------------------------------

void TuiState::setRunIndex(std::size_t i) { if (i < m_runs.size()) m_runIndex = i; }

void TuiState::stepRun(long long delta) {
    if (m_runs.empty()) return;
    long long i = static_cast<long long>(m_runIndex) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(m_runs.size()) - 1));
    m_runIndex = static_cast<std::size_t>(i);
}

void TuiState::addRunBlock() {
    const std::size_t before = m_document.rowCount("Run");
    // A header and a Name, and nothing else: the form shows every other field
    // with its default, and an unset field is one the file does not mention.
    std::vector<std::string> block;
    if (m_buffer.lineCount() > 0 && !trimmed(m_buffer.lines().back()).empty())
        block.push_back(std::string());
    block.push_back("[Run]");
    block.push_back("Name = Run" + std::to_string(before + 1));
    const Caret keep = m_buffer.caret();
    std::size_t at = m_buffer.lineCount();
    if (at > 0 && m_buffer.lines().back().empty()) --at;   // before the final newline
    m_buffer.replaceLines(at, 0, block);
    m_buffer.moveTo(keep);
    edited();
    m_runIndex = m_runs.empty() ? 0 : m_runs.size() - 1;
    m_runsFocus = RunsFocus::Form;
    m_runField = 0;
    setStatus("a new run -- Enter on a field changes it");
}

void TuiState::startRun() {
    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run =
        RunController::fromDocument(m_document, problems, std::nullopt, m_runIndex);
    if (run == nullptr) {
        const std::size_t line = firstErrorLine();
        std::string why = "cannot run: the model does not compile";
        for (const Diagnostic& d : problems)
            if (d.severity == Severity::Error) { why = "cannot run -- " + d.message; break; }
        if (line > 0) why += "   (^J goes there)";
        setStatus(why);
        return;
    }
    m_run = std::move(run);
    m_results.clear();
    m_resultsOf = m_runIndex < m_runs.size() ? m_runs[m_runIndex] : std::string("(defaults)");
    m_overlay = Overlay::Running;
    setStatus("running " + m_resultsOf);
}

void TuiState::advanceRun() {
    if (!m_run) return;
    m_run->advance(4096);
    if (m_run->state() == RunState::Finished || m_run->state() == RunState::Failed) {
        std::ostringstream out;
        m_run->report(out);
        m_results = out.str();
        m_resultsScroll = 0;
        if (m_run->state() == RunState::Failed) {
            setStatus(m_run->failure());
            m_overlay = Overlay::None;
        } else {
            // STRAIGHT TO THE RESULTS. The run view has nothing left to say
            // once it is over, and making somebody press a key to see what
            // they asked for is a key with no decision behind it.
            m_overlay = Overlay::None;
            m_view = View::Results;
            setStatus(m_resultsOf + " finished -- ^W saves this report");
        }
        m_run.reset();
    }
}

void TuiState::stopRun() {
    if (m_run) m_run->cancel();
    m_run.reset();
    m_overlay = Overlay::None;
    setStatus("run stopped");
}

// --- the Runs form -----------------------------------------------------------

namespace {

bool isCommentLine(const std::string& line) {
    const std::string t = trimmed(line);
    return !t.empty() && (t[0] == '#' || t[0] == ';');
}

bool isHeaderLine(const std::string& line) {
    const std::string t = trimmed(line);
    return !t.empty() && t[0] == '[';
}

// The 0-based header line of a [Run] and the first line of whatever follows
// it: the next header, or the comments that introduce the next header.
struct BlockLines { std::size_t header{0}; std::size_t end{0}; };

bool runBlockLines(const ModelDocument& doc, const TextBuffer& buffer,
                   std::size_t run, BlockLines& out) {
    if (run >= doc.rowCount("Run")) return false;
    const std::size_t line = doc.rows("Run")[run].headerLine;
    if (line == 0 || line > buffer.lineCount()) return false;
    out.header = line - 1;
    std::size_t next = out.header + 1;
    while (next < buffer.lineCount() && !isHeaderLine(buffer.lineAt(next))) ++next;
    while (next > out.header + 1 && isCommentLine(buffer.lineAt(next - 1))) --next;
    out.end = next;
    return true;
}

}  // namespace

void TuiState::setRunsFocus(RunsFocus f) {
    m_runsFocus = f;
    if (f == RunsFocus::List) m_editingRunField = false;
}

void TuiState::setRunField(std::size_t i) {
    if (i < m_runFields.size()) m_runField = i;
}

void TuiState::stepRunField(long long delta) {
    if (m_runFields.empty()) return;
    long long i = static_cast<long long>(m_runField) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(m_runFields.size()) - 1));
    m_runField = static_cast<std::size_t>(i);
}

void TuiState::beginRunFieldEdit() {
    if (m_runs.empty() || m_runField >= m_runFields.size()) return;
    const ModuleSchema* schema = ModuleRegistry::instance().find("Run");
    const Column* col = schema ? schema->column(m_runFields[m_runField]) : nullptr;
    if (col == nullptr) return;
    const std::string now = m_document.cell("Run", m_runIndex, col->id);

    if (col->type == ColumnType::Boolean || col->type == ColumnType::Enum) {
        // The same list the editor offers, blank first: blank is "not set",
        // which is how a field goes back to its default.
        m_choices = col->type == ColumnType::Boolean
                        ? std::vector<std::string>{"true", "false"}
                        : col->enumValues;
        m_choices.insert(m_choices.begin(), std::string());
        const auto it = std::find(m_choices.begin(), m_choices.end(), now);
        m_choice = it == m_choices.end() ? 0 : static_cast<std::size_t>(it - m_choices.begin());
        m_pickerFor   = PickerFor::RunField;
        m_pickerTitle = col->id;
        m_overlay     = Overlay::Picker;
        return;
    }
    m_runDraft = now;
    m_editingRunField = true;
    setStatus("editing " + col->id + " -- Enter keeps it, Esc leaves it as it was");
}

void TuiState::typeRunField(char c) { if (m_editingRunField) m_runDraft.push_back(c); }

void TuiState::backspaceRunField() {
    if (m_editingRunField && !m_runDraft.empty()) m_runDraft.pop_back();
}

bool TuiState::commitRunFieldEdit() {
    if (!m_editingRunField || m_runField >= m_runFields.size()) return false;
    if (!setRunValue(m_runIndex, m_runFields[m_runField], trimmed(m_runDraft)))
        return false;          // still editing, so the person can fix it
    m_editingRunField = false;
    return true;
}

void TuiState::cancelRunFieldEdit() {
    m_editingRunField = false;
    m_runDraft.clear();
    setStatus("");
}

void TuiState::showRunFieldHelp() {
    const ModuleSchema* schema = ModuleRegistry::instance().find("Run");
    if (schema == nullptr || m_runField >= m_runFields.size()) return;
    const Column* col = schema->column(m_runFields[m_runField]);
    if (col == nullptr) return;
    m_helpTitle = "Run." + col->id;
    m_helpBody  = {col->help};
    if (!col->defaultValue.empty()) {
        m_helpBody.push_back(std::string());
        m_helpBody.push_back("Default: " + col->defaultValue);
    }
    m_overlay = Overlay::Help;
}

bool TuiState::setRunValue(std::size_t run, const std::string& column,
                           const std::string& value) {
    BlockLines block;
    if (!runBlockLines(m_document, m_buffer, run, block)) return false;

    // The line to rewrite, or where a new one goes: after the last line of
    // the block that says something, so a trailing blank line stays trailing.
    std::size_t found = block.end, last = block.header;
    for (std::size_t i = block.header + 1; i < block.end; ++i) {
        const std::string k = keyOfLine(m_buffer.lineAt(i));
        if (!k.empty()) last = i;
        if (k == column) found = i;
    }
    std::vector<std::string> with;
    std::size_t first = found, count = 1;
    if (found == block.end) {
        if (value.empty()) return true;          // unset, and it already was
        first = last + 1;
        count = 0;
        with.push_back(column + " = " + value);
    } else if (!value.empty()) {
        // The key exactly as it was typed, alignment included.
        const std::string& line = m_buffer.lineAt(found);
        std::string key = line.substr(0, line.find('='));
        while (!key.empty() && key.back() == ' ') key.pop_back();
        const std::string gap = line.substr(key.size(), line.find('=') - key.size());
        with.push_back(key + gap + "= " + value);
    }

    // TRIED ON A COPY FIRST. Refusing after the fact would mean an undo, and
    // that would leave a redo able to put the bad value back.
    TextBuffer trial = m_buffer;
    trial.replaceLines(first, count, with);
    ReadResult read = readDocument(trial.text());
    CompileResult compiled = compile(read.document);
    std::vector<Diagnostic> all = std::move(read.diagnostics);
    for (Diagnostic& d : compiled.diagnostics) all.push_back(std::move(d));
    std::vector<Diagnostic> setup;
    readRunSetup(read.document, setup, run);
    for (Diagnostic& d : setup) all.push_back(std::move(d));
    for (const Diagnostic& d : all) {
        if (d.severity != Severity::Error || !d.cell) continue;
        if (d.cell->moduleType == "Run" && d.cell->row == run && d.cell->column == column) {
            setStatus(column + ": " + d.message);
            return false;
        }
    }

    const Caret keep = m_buffer.caret();
    m_buffer.replaceLines(first, count, with);
    m_buffer.moveTo(keep);
    edited();
    setStatus(value.empty() ? column + " cleared -- back to its default"
                            : column + " = " + value);
    return true;
}

void TuiState::requestDeleteRun() {
    if (m_runs.empty()) { setStatus("there is no run to delete"); return; }
    m_confirmFor = ConfirmFor::DeleteRun;
    m_overlay = Overlay::Confirm;
    setStatus("delete run " + m_runs[m_runIndex] + "? (y)es  (n)o");
}

bool TuiState::deleteRun(std::size_t run) {
    BlockLines block;
    if (!runBlockLines(m_document, m_buffer, run, block)) return false;
    std::size_t start = block.header;
    // The comment directly above a block is about that block.
    while (start > 0 && isCommentLine(m_buffer.lineAt(start - 1))) --start;
    std::size_t end = block.end;
    // The file's final newline is the file's, not the block's.
    if (end == m_buffer.lineCount() && end > block.header + 1 &&
        m_buffer.lineAt(end - 1).empty())
        --end;
    // The LAST block takes the blank lines that separated it with it, or the
    // file would end in them.
    if (end >= m_buffer.lineCount() - 1)
        while (start > 0 && trimmed(m_buffer.lineAt(start - 1)).empty()) --start;
    const std::string name = run < m_runs.size() ? m_runs[run] : std::string("run");
    const Caret keep = m_buffer.caret();
    m_buffer.replaceLines(start, end - start, {});
    m_buffer.moveTo(keep);
    edited();
    setStatus("deleted run " + name + " -- ^Z brings it back");
    return true;
}

// --- results ----------------------------------------------------------------

void TuiState::scrollResults(long long delta) {
    long long i = static_cast<long long>(m_resultsScroll) + delta;
    m_resultsScroll = static_cast<std::size_t>(std::max<long long>(0, i));
}

bool TuiState::saveResults() {
    if (m_results.empty()) { setStatus("no results yet -- ^R runs the model"); return false; }
    std::string base = m_path;
    const std::size_t dot = base.find_last_of('.');
    const std::size_t slash = base.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        base = base.substr(0, dot);
    std::string name = base + ".results.txt";

    std::ofstream out(name, std::ios::binary);
    if (!out.is_open()) { setStatus(name + ": could not write"); return false; }
    out << "# " << m_path << "   run: " << m_resultsOf << "\n";
    out << m_results;
    if (!out.good()) { setStatus(name + ": could not write"); return false; }
    m_resultsPath = name;
    setStatus(name + ": saved");
    return true;
}

// --- the flow ---------------------------------------------------------------

void TuiState::stepFlow(long long delta) {
    const std::size_t n = flowOf(m_document).size();
    if (n == 0) return;
    long long i = static_cast<long long>(m_flowCursor) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(n) - 1));
    m_flowCursor = static_cast<std::size_t>(i);
    m_flowPinned = false;       // choosing a block brings the view back to it
}

void TuiState::gotoFlowBlock() {
    const std::vector<FlowBlock> blocks = flowOf(m_document);
    if (m_flowCursor >= blocks.size()) { setView(View::Model); return; }
    const FlowBlock& b = blocks[m_flowCursor];
    setView(View::Model);
    m_focus = Pane::Editor;
    if (b.line > 0) m_buffer.moveTo(Caret{b.line - 1, 0});
    setStatus(b.name.empty() ? b.type : b.name);
}

const AsciiGraph* TuiState::flowGraph() const {
    const std::string dot = dotFor(m_document);
    if (dot != m_flowDot) {
        m_flowDot = dot;
        m_flowReady = false;
        m_flowError.clear();
        const std::string dotExe = findDot();
        std::error_code ec;
        const std::string out =
            (std::filesystem::temp_directory_path(ec) / "des_flow_view.plain").string();
        std::string why;
        if (dotExe.empty()) {
            m_flowError = "Graphviz is not installed -- showing the list. "
                          "(winget install Graphviz.Graphviz)";
        } else if (!runDot(dotExe, dot, "plain", out, why)) {
            m_flowError = "Graphviz could not draw this: " + why;
        } else {
            std::ifstream in(out, std::ios::binary);
            const std::string plain((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
            m_flowGraph = drawAscii(parsePlain(plain));
            m_flowReady = !m_flowGraph.lines.empty();
            if (!m_flowReady) m_flowError = "Graphviz returned an empty layout";
            std::filesystem::remove(out, ec);
        }
    }
    return m_flowReady ? &m_flowGraph : nullptr;
}

bool TuiState::saveFlowPicture() {
    const std::string dotExe = findDot();
    if (dotExe.empty()) {
        setStatus("Graphviz is not installed, so there is no picture to save -- "
                  "winget install Graphviz.Graphviz");
        return false;
    }
    const std::string path = flowImagePath(m_path);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::string why;
    if (!runDot(dotExe, dotFor(m_document), "png", path, why)) {
        setStatus("could not draw the picture: " + why);
        return false;
    }
    const bool opened = m_opener && m_opener(path);
    setStatus(path + (opened ? ": saved and opened" : ": saved"));
    return true;
}

void TuiState::requestQuit() {
    if (!m_dirty) { m_quit = true; return; }
    // Unsaved work is asked about, never discarded on one keystroke. The whole
    // point of a UI over a file format is that a person trusts it with work
    // that exists nowhere else.
    m_confirmFor = ConfirmFor::Quit;
    m_overlay = Overlay::Confirm;
    setStatus("unsaved changes -- (s)ave and quit, (d)iscard and quit, (c)ancel");
}

// --- describe a model (v17) ----------------------------------------------------

// The request in flight. Shared with the worker, so abandoning it -- Esc, or
// quitting while it waits -- just drops the UI's reference: the worker writes
// into an object nobody reads any more, and nothing dangles.
struct AskJob {
    std::atomic<bool> done{false};
    GenerateResult    result;
};

void TuiState::openAsk() {
    m_overlay = Overlay::Ask;
    if (m_askLog.empty())
        m_askLog.push_back({AskLine::Kind::Info,
                            "Say or type what happens -- what arrives and how often, what is done to "
                            "it and how long that takes, how many servers. Windows: Win+H to speak. "
                            "Mac: press the Dictation key, or Fn twice."});
    setStatus(m_llm.needsKey() ? "no API key yet -- ^K to enter one (console.groq.com/keys)"
                               : "your words go to " + m_llm.host() + " (" + m_llm.model + ")");
}

void TuiState::closeAsk() {
    m_overlay = Overlay::None;
    m_askKeyEntry = false;
    m_askDraft.clear();
    setStatus("");
}

void TuiState::typeAsk(char c) {
    // Ignored while waiting: the box is not drawn then, and words typed into
    // something invisible would be sent with the next Enter by surprise.
    if (askBusy()) return;
    // A tab or newline from a paste or from dictation is a space here: the box
    // holds one description, and Enter is what sends it.
    m_askDraft.push_back(c == '\t' || c == '\n' || c == '\r' ? ' ' : c);
}

void TuiState::backspaceAsk() {
    if (!askBusy() && !m_askDraft.empty()) m_askDraft.pop_back();
}

void TuiState::pasteIntoAsk(const std::string& text) {
    for (const char c : text) typeAsk(c);
}

void TuiState::beginKeyEntry() {
    if (askBusy()) return;
    m_askKeyEntry = true;
    m_askDraft.clear();
    setStatus("paste the API key and press Enter -- it is saved for you alone, never in a model");
}

void TuiState::escapeAsk() {
    if (askBusy()) {
        m_askJob.reset();
        m_askLog.push_back({AskLine::Kind::Info, "Stopped waiting. Nothing was changed."});
        setStatus("stopped waiting");
        return;
    }
    if (m_askKeyEntry) {
        m_askKeyEntry = false;
        m_askDraft.clear();
        m_askPending.clear();
        setStatus("key left as it was");
        return;
    }
    closeAsk();
}

void TuiState::commitAsk() {
    if (askBusy()) return;
    const std::string text = trimmed(m_askDraft);

    if (m_askKeyEntry) {
        if (text.empty()) { setStatus("paste the key first -- Esc leaves it as it was"); return; }
        std::string error;
        const bool saved = m_askHooks.saveKey && m_askHooks.saveKey(text, error);
        m_llm.apiKey = text;
        m_askKeyEntry = false;
        m_askDraft.clear();
        m_askLog.push_back({saved ? AskLine::Kind::Info : AskLine::Kind::Error,
                            saved ? "Key " + maskKey(text) + " saved."
                                  : "Key " + maskKey(text) + " will be used until you quit" +
                                        (error.empty() ? std::string(".") : ", but not saved: " + error)});
        if (!m_askPending.empty()) {
            const std::string pending = m_askPending;
            m_askPending.clear();
            startAsk(pending);
        } else {
            setStatus("key set -- now say what happens");
        }
        return;
    }

    if (text.empty()) {
        // Enter on an empty line after a finished model means "done".
        if (!m_askBuiltText.empty() && m_askBuiltText == m_buffer.text()) { closeAsk(); return; }
        setStatus("say or type what happens first -- Esc closes");
        return;
    }
    m_askDraft.clear();
    if (m_llm.needsKey()) {
        m_askPending = text;
        m_askLog.push_back({AskLine::Kind::You, text});
        m_askLog.push_back({AskLine::Kind::Info,
                            m_llm.host() + " needs an API key. Paste yours below and press Enter; "
                            "a free one is at console.groq.com/keys."});
        m_askKeyEntry = true;
        setStatus("paste the API key and press Enter");
        return;
    }
    m_askLog.push_back({AskLine::Kind::You, text});
    startAsk(text);
}

void TuiState::startAsk(const std::string& text) {
    if (!m_askHooks.generate) {
        m_askLog.push_back({AskLine::Kind::Error, "Describing a model is not available in this build."});
        return;
    }
    // The form is only carried forward while the text is still what it built.
    // Hand edits since then cannot be expressed in the form, so say plainly
    // that a change from here starts over -- and ^Z still brings them back.
    JsonValue previous;
    if (m_askScenario.isObject()) {
        previous = m_askScenario;
        if (m_buffer.text() != m_askBuiltText)
            m_askLog.push_back({AskLine::Kind::Info,
                                "The text was edited by hand since the last answer; those edits will "
                                "be replaced (^Z brings them back)."});
    }

    auto job = std::make_shared<AskJob>();
    m_askJob = job;
    auto generate = m_askHooks.generate;
    const LlmConfig config = m_llm;
    std::function<void()> work = [job, generate, text, previous, config] {
        job->result = generate(text, previous, config);
        job->done.store(true, std::memory_order_release);
    };
    setStatus("asking " + m_llm.model + " at " + m_llm.host() + " ... Esc stops waiting");
    if (m_askHooks.launch) m_askHooks.launch(std::move(work));
    else                   std::thread(std::move(work)).detach();
}

void TuiState::pollAsk() {
    if (!m_askJob || !m_askJob->done.load(std::memory_order_acquire)) return;
    const std::shared_ptr<AskJob> job = std::move(m_askJob);
    m_askJob.reset();
    GenerateResult& r = job->result;

    if (!r.error.empty()) {
        m_askLog.push_back({AskLine::Kind::Error, r.error});
        setStatus("that did not work -- see the box");
        return;
    }
    if (r.scenarioJson.isObject()) m_askScenario = r.scenarioJson;
    for (const std::string& n : r.notes) m_askLog.push_back({AskLine::Kind::Understood, n});
    if (!r.ok) {
        for (const Question& q : r.questions) m_askLog.push_back({AskLine::Kind::Question, q.text});
        setStatus(r.questions.size() == 1 ? "one thing is still needed -- answer in the box"
                                          : std::to_string(r.questions.size()) +
                                                " things are still needed -- answer in the box");
        return;
    }
    for (const std::string& a : r.assumptions) m_askLog.push_back({AskLine::Kind::Assumed, a});

    // ONE edit: select everything, insert the model. TextBuffer records that
    // as a single undo step, so ^Z puts back exactly what was there.
    m_buffer.selectAll();
    m_buffer.insert(writeDocument(r.document));
    m_buffer.moveToStart();
    edited();
    m_askBuiltText = m_buffer.text();
    m_view = View::Model;
    m_askLog.push_back({AskLine::Kind::Info,
                        "Model written into the editor. ^Z undoes it, ^R runs it. Type a change "
                        "to make (\"add a second teller\"), or press Enter to close."});
    setStatus(hasErrors(m_diagnostics) ? "model written -- but it has errors, see the E marks"
                                       : "model written -- ^R runs it, ^Z takes it back");
}

}  // namespace des
