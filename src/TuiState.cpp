#include "TuiState.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include "DocumentFormat.hpp"

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
            blocks.push_back(std::move(b));
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
    for (FlowBlock& b : blocks) {
        for (const std::string& e : b.exits) {
            const std::size_t at = e.rfind(" -> ");
            if (at != std::string::npos && !exists(e.substr(at + 4)))
                b.dangling = true;
        }
        // A Create is where entities come FROM, so nothing pointing at it is
        // the normal case rather than something to flag.
        b.unreached = b.type != "Create" && !b.name.empty() &&
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
    s.m_buffer.setText(text);
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
        m_palette.push_back(schema.typeName);
    }
}

void TuiState::reparse() {
    ReadResult read = readDocument(m_buffer.text());
    m_document    = std::move(read.document);
    m_diagnostics = std::move(read.diagnostics);

    // The reader's diagnostics FIRST, then the compiler's. A file that does
    // not parse cannot be compiled meaningfully, but the compiler is run
    // anyway on whatever did parse: half a model with its errors named beats
    // one message about a stray bracket.
    CompileResult compiled = compile(m_document);
    for (Diagnostic& d : compiled.diagnostics) m_diagnostics.push_back(std::move(d));

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

bool TuiState::save() { return save(m_path); }

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
    const std::string text = m_buffer.text();
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
    const ModuleSchema* schema = ModuleRegistry::instance().find("Run");
    if (schema == nullptr) return;
    m_buffer.moveToEnd();
    if (!m_buffer.lineAt(m_buffer.caret().line).empty()) m_buffer.newline();
    m_buffer.insertLines(templateFor(*schema));
    edited();
    m_runIndex = m_runs.empty() ? 0 : m_runs.size() - 1;
    setView(View::Model);
    setStatus("a new [Run] -- give it a Name so you can tell it apart");
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

void TuiState::requestQuit() {
    if (!m_dirty) { m_quit = true; return; }
    // Unsaved work is asked about, never discarded on one keystroke. The whole
    // point of a UI over a file format is that a person trusts it with work
    // that exists nowhere else.
    m_overlay = Overlay::Confirm;
    setStatus("unsaved changes -- (s)ave and quit, (d)iscard and quit, (c)ancel");
}

}  // namespace des
