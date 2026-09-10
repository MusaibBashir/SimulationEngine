// ============================================================================
// TuiState.hpp  --  v13: everything the UI knows
// ============================================================================
// One object, because "what is on screen" is one question. The renderer reads
// it and the input layer writes it, and neither touches a terminal -- which is
// what lets a test drive the whole UI by constructing one of these and feeding
// it keys.

#pragma once
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"
#include "ModuleSchema.hpp"
#include "RunController.hpp"

namespace des {

enum class Mode { Grid, Detail, Picking, Editing, Running, Confirm, Help, Flow };

// One flowchart block as the flow view shows it. Built from the document, so
// it says what the model SAYS rather than what a compiled Model ended up
// holding -- a document with a dangling exit does not compile, and that is
// exactly when a person most needs to see the wiring.
struct FlowBlock {
    std::string              name;
    std::string              type;
    std::size_t              row{0};
    std::vector<std::string> exits;      // "Next -> Serve", "Balk To -> Wait"
    bool                     dangling{false};   // an exit names nothing
    bool                     unreached{false};  // nothing names this
};

std::vector<FlowBlock> flowOf(const ModelDocument& doc);

// A working M/M/1: arrivals, one server, an exit, and a Run row. The blank
// page answer was "empty, but the screen teaches"; this is the other half of
// it. Somebody who has used Arena knows what a model looks like and not what
// THIS one looks like, and a file they can run and then take apart says it
// faster than any amount of prose. It carries comments for the same reason.
ModelDocument starterModel();

class TuiState {
public:
    // A file that does not PARSE still opens -- v11 preserves what it read --
    // with the reader's diagnostics shown. A file that does not EXIST opens as
    // an empty document, so `des_tui new.des` starts a new model rather than
    // refusing. The status line says which happened, because "empty" and
    // "unreadable" must not look alike.
    static TuiState open(const std::string& path);
    static TuiState fromDocument(ModelDocument doc, std::string path);

    Mode mode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }

    const ModelDocument&           document()    const { return m_document; }
    const std::string&             path()        const { return m_path; }
    bool                           dirty()       const { return m_dirty; }
    const std::vector<Diagnostic>& diagnostics() const { return m_diagnostics; }
    const std::string&             status()      const { return m_status; }
    void setStatus(std::string s) { m_status = std::move(s); }

    // Every type the registry publishes, then any the document holds that it
    // does not. Computed, never a list in this file: that is the property the
    // flat child tables were chosen to protect.
    const std::vector<std::string>& types() const { return m_types; }

    const std::string& type()   const { return m_type; }
    std::size_t        row()    const { return m_row; }
    std::size_t        column() const { return m_column; }

    void setType(const std::string& type);
    void setRow(std::size_t row);
    void setColumn(std::size_t column);

    // Null for a type the registry does not know. Every caller must cope: v11
    // keeps unknown module types on purpose.
    const ModuleSchema* schemaHere() const;
    // The column ids to show. The schema's, or -- for an unknown type -- the
    // ones the document's own rows happen to carry.
    std::vector<std::string> columnsHere() const;
    std::size_t              rowCountHere() const;

    bool save();                                  // to path()
    bool save(const std::string& toPath);
    void recompile();

    // Every mutation pushes a snapshot first. Each is refused, with a reason
    // in the status line, when the current module type is read-only.
    void setCell(const std::string& column, const std::string& text);
    void addRow();
    void removeRow();
    void moveRow(int delta);

    bool canUndo() const { return !m_undo.empty(); }
    void undo();

    // ^T. Refuses on a document that already has rows: it replaces everything,
    // and a single keystroke must not be able to discard a model.
    void insertStarter();

    // ^F. Arena's canvas, as far as a terminal can go: who connects to whom,
    // read out of the Next cells. Enter jumps the cursor to the block under
    // the marker, which is what makes it a way of getting somewhere rather
    // than a picture.
    void openFlow();
    std::size_t flowCursor() const { return m_flowCursor; }
    void stepFlow(long long delta);
    void gotoFlowBlock();

    bool readOnlyHere() const;

    // The cell being edited, held OUTSIDE the document until it commits. A
    // half-typed expression is not a model change, and recompiling on every
    // keystroke would flag EXPO(0.8 as broken while it is still being typed.
    const std::string& editBuffer() const { return m_edit; }
    void beginEdit();
    void typeEdit(char c);
    void backspaceEdit();
    void cancelEdit();
    void commitEdit();

    // Arena's drop-down. An Enum cell offers the spellings it allows; a
    // Reference cell offers what may go there, taken from the document.
    //
    // Returns false, having entered nothing, when this column is not one that
    // can be picked from. The caller then opens the text editor -- which is
    // also what beginPick leaves for a column that HAS a list but whose answer
    // is not on it yet, the case that matters: wiring Serve -> Out before Out
    // exists must not be something the UI makes hard.
    bool beginPick();
    const std::vector<std::string>& choices()  const { return m_choices; }
    std::size_t                     choice()   const { return m_choice; }
    void stepPick(long long delta);
    void commitPick();
    void cancelPick();
    // Esc: abandon the list, keep the cell, and start typing it instead.
    void typeInstead();

    // The first diagnostic against a column of the CURRENT row, or null.
    const Diagnostic* diagnosticFor(const std::string& column) const;

    // ^R. Refuses, with a reason, when the document does not compile:
    // entering a run mode with nothing running is worse than not entering it.
    void startRun();
    void advanceRun();
    void stopRun();
    const RunController* running()   const { return m_run.get(); }
    const std::string&   runReport() const { return m_runReport; }

private:
    void rebuildTypes();
    void clampCursor();
    bool refuseIfReadOnly();
    void pushUndo();

    ModelDocument            m_document;
    std::string              m_path;
    std::string              m_status;
    std::vector<std::string> m_types;
    std::string              m_type;
    std::size_t              m_row{0};
    std::size_t              m_column{0};
    Mode                     m_mode{Mode::Grid};
    bool                     m_dirty{false};
    std::string              m_edit;
    std::vector<std::string> m_choices;
    std::size_t              m_choice{0};
    std::size_t              m_flowCursor{0};
    std::vector<Diagnostic>  m_diagnostics;

    // Whole documents, not per-operation inverses. A snapshot cannot be
    // wrong about its own inverse; a hand-written undo for moveRow can. A
    // model document is kilobytes, so the crude answer is also the correct
    // one.
    static constexpr std::size_t UNDO_DEPTH = 64;
    std::vector<ModelDocument> m_undo;

    // A unique_ptr makes TuiState move-only, which is what it should be: it
    // owns a running simulation, and copying one would mean two.
    std::unique_ptr<RunController> m_run;
    std::string                    m_runReport;
};

}  // namespace des
