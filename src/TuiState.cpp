#include "TuiState.hpp"

#include <algorithm>
#include <fstream>
#include <utility>
#include "Compiler.hpp"
#include "DocumentFormat.hpp"

namespace des {

TuiState TuiState::fromDocument(ModelDocument doc, std::string path) {
    TuiState s;
    s.m_document = std::move(doc);
    s.m_path = std::move(path);
    s.rebuildTypes();
    // The first FLOWCHART type that has rows, not the first the registry
    // publishes. That was Variable, so opening any model at all landed on an
    // empty table explaining global counters, with the model itself two tabs
    // away and no sign of it. Flowchart before data because the flowchart IS
    // the model -- teller.des starts with its [Run] record, and settings are
    // not what a person opened it to see. Anything with rows beats nothing,
    // and a blank document falls back to Create, where a flow starts.
    s.m_type = "Create";
    const ModuleRegistry& reg = ModuleRegistry::instance();
    std::string firstWithRows;
    for (const std::string& t : s.m_document.types()) {
        if (s.m_document.rowCount(t) == 0) continue;
        if (firstWithRows.empty()) firstWithRows = t;
        const ModuleSchema* schema = reg.find(t);
        if (schema != nullptr && schema->kind == ModuleKind::Flowchart) {
            firstWithRows = t;
            break;
        }
    }
    if (!firstWithRows.empty()) s.m_type = firstWithRows;
    if (std::find(s.m_types.begin(), s.m_types.end(), s.m_type) == s.m_types.end() &&
        !s.m_types.empty())
        s.m_type = s.m_types.front();
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

ModelDocument starterModel() {
    ModelDocument d;
    // Comments, because the file this writes is the second thing that teaches.
    // v11 preserves them through an edit, so they survive being taken apart.
    d.setPreamble({"# A single teller: entities arrive, wait for the one",
                   "# server, are served, and leave. The smallest complete",
                   "# model, and every cell in it is meant to be changed.",
                   "#",
                   "# ^R runs it. Tab moves between module types."});

    d.addRow("Run");
    d.setCell("Run", 0, "Name", "Setup");
    d.setCell("Run", 0, "Length", "480");
    d.setCell("Run", 0, "Replications", "1");

    d.addRow("Create");
    d.setCell("Create", 0, "Name", "Arrivals");
    d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
    d.setCell("Create", 0, "Next", "Serve");

    d.addRow("Process");
    d.setCell("Process", 0, "Name", "Serve");
    d.setCell("Process", 0, "Capacity", "1");
    d.setCell("Process", 0, "Discipline", "FIFO");
    d.setCell("Process", 0, "Service", "EXPO(0.8)");
    d.setCell("Process", 0, "Next", "Out");

    d.addRow("Dispose");
    d.setCell("Dispose", 0, "Name", "Out");
    return d;
}

void TuiState::insertStarter() {
    for (const std::string& t : m_document.types()) {
        if (m_document.rowCount(t) == 0) continue;
        setStatus("^T starts a model from nothing, and this one has rows "
                  "already -- open a new file to use it");
        return;
    }
    pushUndo();
    m_document = starterModel();
    rebuildTypes();
    m_type = "Create";      // where the flow starts, and where to read from
    m_row = 0;
    m_column = 0;
    m_dirty = true;
    recompile();
    setStatus("a working single-server model -- ^R runs it, ? lists the keys");
}

void TuiState::openFlow() {
    const std::vector<FlowBlock> blocks = flowOf(m_document);
    if (blocks.empty()) {
        setStatus("no flowchart blocks yet -- ^T fills in a working model, "
                  "or ^N adds a Create");
        return;
    }
    // Land on the block the cursor is already sitting on, when it is one, so
    // ^F answers "where am I in this" and not only "what is this".
    m_flowCursor = 0;
    for (std::size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i].type == m_type && blocks[i].row == m_row) m_flowCursor = i;
    m_mode = Mode::Flow;
}

void TuiState::stepFlow(long long delta) {
    const std::size_t n = flowOf(m_document).size();
    if (n == 0) return;
    long long i = static_cast<long long>(m_flowCursor) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(n) - 1));
    m_flowCursor = static_cast<std::size_t>(i);
}

void TuiState::gotoFlowBlock() {
    const std::vector<FlowBlock> blocks = flowOf(m_document);
    if (m_flowCursor >= blocks.size()) { m_mode = Mode::Grid; return; }
    setType(blocks[m_flowCursor].type);
    setRow(blocks[m_flowCursor].row);
    m_mode = Mode::Detail;
    setStatus(blocks[m_flowCursor].name);
}

void TuiState::beginEdit() {
    if (refuseIfReadOnly()) return;
    const std::vector<std::string> columns = columnsHere();
    if (m_column >= columns.size() || m_row >= rowCountHere()) return;
    m_edit = m_document.cell(m_type, m_row, columns[m_column]);
    m_mode = Mode::Editing;
}

void TuiState::typeEdit(char c) { m_edit.push_back(c); }

void TuiState::backspaceEdit() { if (!m_edit.empty()) m_edit.pop_back(); }

void TuiState::cancelEdit() {
    m_edit.clear();
    m_mode = Mode::Detail;
}

void TuiState::commitEdit() {
    const std::vector<std::string> columns = columnsHere();
    if (m_column < columns.size()) setCell(columns[m_column], m_edit);
    m_edit.clear();
    m_mode = Mode::Detail;
}

namespace {
// The same word the reference pass uses in its error, so "no block named 'Out'"
// and "no block exists yet" plainly concern one thing.
std::string nameOfTarget(const std::string& targetType) {
    return targetType == "Block" ? std::string("block") : targetType;
}
}  // namespace

bool TuiState::beginPick() {
    // Refusing counts as handling it. Otherwise the caller would fall through
    // to the editor and a read-only table would look editable right up until
    // the commit silently did nothing.
    if (refuseIfReadOnly()) return true;

    const std::vector<std::string> columns = columnsHere();
    if (m_column >= columns.size() || m_row >= rowCountHere()) return false;
    const ModuleSchema* schema = schemaHere();
    if (schema == nullptr) return false;
    const Column* col = schema->column(columns[m_column]);
    if (col == nullptr) return false;

    m_choices.clear();
    if (col->type == ColumnType::Enum) {
        m_choices = col->enumValues;
    } else if (col->type == ColumnType::Reference) {
        m_choices = referenceCandidates(m_document, col->referencedType);
        if (m_choices.empty()) {
            // NOT an error, and not an empty menu either. On a model being
            // built from scratch this is the ordinary case -- the first Next
            // is typed before anything exists to point at -- so it opens the
            // editor and says why.
            setStatus("no " + nameOfTarget(col->referencedType) +
                      " exists yet -- type the name, then add it");
            return false;
        }
        // An optional exit may be left empty: the engine spells "leaves the
        // system" as a null next, and clearing a wired exit has to be
        // reachable from the list that wired it.
        //
        // FIRST, not last. An unwired exit opens on whatever it holds, which
        // is this one -- and at the end of a twenty-block list that meant
        // opening scrolled to the bottom with no block name in sight.
        if (!col->required) m_choices.insert(m_choices.begin(), std::string());
    } else {
        return false;
    }
    if (m_choices.empty()) return false;

    // Open ON the current value when it is one of the choices, so Enter alone
    // is a no-op rather than a silent change to whatever sorted first.
    const std::string now = m_document.cell(m_type, m_row, columns[m_column]);
    const auto it = std::find(m_choices.begin(), m_choices.end(), now);
    m_choice = (it == m_choices.end())
                   ? 0
                   : static_cast<std::size_t>(it - m_choices.begin());
    m_mode = Mode::Picking;
    return true;
}

void TuiState::stepPick(long long delta) {
    if (m_choices.empty()) return;
    long long i = static_cast<long long>(m_choice) + delta;
    i = std::max<long long>(0, std::min(i, static_cast<long long>(m_choices.size()) - 1));
    m_choice = static_cast<std::size_t>(i);
}

void TuiState::commitPick() {
    const std::vector<std::string> columns = columnsHere();
    if (m_choice < m_choices.size() && m_column < columns.size())
        setCell(columns[m_column], m_choices[m_choice]);
    m_choices.clear();
    m_mode = Mode::Detail;
}

void TuiState::cancelPick() {
    m_choices.clear();
    m_mode = Mode::Detail;
}

void TuiState::typeInstead() {
    m_choices.clear();
    m_mode = Mode::Detail;
    beginEdit();
}

const Diagnostic* TuiState::diagnosticFor(const std::string& column) const {
    for (const Diagnostic& d : m_diagnostics)
        if (d.cell && d.cell->moduleType == m_type && d.cell->row == m_row &&
            d.cell->column == column)
            return &d;
    return nullptr;
}

void TuiState::startRun() {
    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run =
        RunController::fromDocument(m_document, problems);
    if (run == nullptr) {
        m_diagnostics = std::move(problems);
        setStatus("cannot run: the document does not compile");
        return;
    }
    m_run = std::move(run);
    m_runReport.clear();
    m_mode = Mode::Running;
    setStatus("running");
}

void TuiState::advanceRun() {
    if (!m_run) return;
    m_run->advance(4096);
    if (m_run->state() == RunState::Finished || m_run->state() == RunState::Failed) {
        std::ostringstream out;
        m_run->report(out);
        m_runReport = out.str();
        setStatus(m_run->state() == RunState::Failed ? m_run->failure() : "run finished");
    }
}

void TuiState::stopRun() {
    if (m_run) m_run->cancel();
    m_run.reset();
    m_runReport.clear();
    m_mode = Mode::Grid;
}

}  // namespace des
