// ============================================================================
// TuiState.hpp  --  v15: everything the UI knows
// ============================================================================
// One object, because "what is on screen" is one question. The renderer reads
// it and the input layer writes it, and neither touches a terminal -- which is
// what lets a test drive the whole UI by constructing one of these and feeding
// it keys.
//
// v15 INVERTS v13. The text is the model: a TextBuffer holds the .des file and
// everything else -- the document, the diagnostics, the flow, the list of runs
// -- is derived from it after every change. v13 edited a ModelDocument and
// wrote it out, which meant a value could be wrong in two places and a writer
// had to work to keep comments alive. Here what is saved is what was typed.

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Compiler.hpp"
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"
#include "ModuleSchema.hpp"
#include "RunController.hpp"
#include "TextBuffer.hpp"

namespace des {

// What fills the body of the window. Each is reachable from the bar across the
// top and from one control key, because a bar you can only reach with a mouse
// is a bar half the people using it cannot reach at all.
enum class View { Model, Flow, Runs, Results };

// Which half of the Model view has the keyboard.
enum class Pane { Palette, Editor };

// Something drawn OVER the view, which takes every key until it closes. Modal
// on purpose: a list of choices that let you edit underneath it would be
// answering a question about text that had moved.
enum class Overlay { None, Help, Picker, Confirm, Running };

// A working M/M/1: arrivals, one server, an exit, and a Run row. Somebody who
// has used Arena knows what a model looks like and not what THIS one looks
// like, and a file they can run and then take apart says it faster than any
// amount of prose. It carries comments for the same reason.
ModelDocument starterModel();
std::string   starterModelText();

// One flowchart block as the flow view shows it. Built from the document, so
// it says what the model SAYS rather than what a compiled Model ended up
// holding -- a document with a dangling exit does not compile, and that is
// exactly when a person most needs to see the wiring.
struct FlowBlock {
    std::string              name;
    std::string              type;
    std::size_t              row{0};
    std::size_t              line{0};       // where to put the cursor
    std::vector<std::string> exits;
    bool                     dangling{false};
    bool                     unreached{false};
};

std::vector<FlowBlock> flowOf(const ModelDocument& doc);

// The lines a module type inserts: its header, then every column it has, blank.
// Blank rather than defaulted, because a value written into the file is a
// decision somebody made and a default that is merely implied is not.
std::vector<std::string> templateFor(const ModuleSchema& schema);

// Which module block a line of the file is inside, walking back to the nearest
// [Header]. Empty when the line is above the first one.
std::string blockTypeAt(const TextBuffer& buffer, std::size_t line);

// The key of a `key = value` line, trimmed, or empty when it is not one.
std::string keyOfLine(const std::string& line);

class TuiState {
public:
    // A file that does not PARSE still opens -- the text is the text -- with
    // the reader's diagnostics shown. A file that does not EXIST opens empty,
    // so `des_tui new.des` starts a new model rather than refusing. The status
    // line says which happened, because "empty" and "unreadable" must not look
    // alike.
    static TuiState open(const std::string& path);
    static TuiState fromText(const std::string& text, std::string path);

    // --- what is on screen ---------------------------------------------------
    View    view()    const { return m_view; }
    Pane    focus()   const { return m_focus; }
    Overlay overlay() const { return m_overlay; }
    void    setView(View v);
    void    setFocus(Pane p) { m_focus = p; }
    void    setOverlay(Overlay o) { m_overlay = o; }

    const std::string& path()   const { return m_path; }
    bool               dirty()  const { return m_dirty; }
    const std::string& status() const { return m_status; }
    void setStatus(std::string s) { m_status = std::move(s); }

    // --- the text ------------------------------------------------------------
    const TextBuffer& buffer() const { return m_buffer; }
    TextBuffer&       buffer()       { return m_buffer; }
    // Every mutation goes through here so nothing can change the text without
    // the document, the diagnostics and the dirty flag catching up.
    void edited();

    void insertText(const std::string& text);
    void typeChar(char c);
    void newline();
    void backspace();
    void del();
    void undo();
    void redo();

    void cut();
    void copy();
    void paste();
    const std::string& clipboard() const { return m_clip; }
    void setClipboard(std::string s) { m_clip = std::move(s); }

    bool save();
    bool save(const std::string& toPath);

    // --- what the text means -------------------------------------------------
    const ModelDocument&           document()    const { return m_document; }
    const std::vector<Diagnostic>& diagnostics() const { return m_diagnostics; }
    // The first diagnostic on a 1-based file line, or null.
    const Diagnostic* diagnosticOnLine(std::size_t line) const;
    // Where the cursor should go to see the first error, or 0.
    std::size_t firstErrorLine() const;
    void        goToFirstError();

    // --- the palette ---------------------------------------------------------
    const std::vector<std::string>& palette() const { return m_palette; }
    std::size_t paletteIndex() const { return m_paletteIndex; }
    void stepPalette(long long delta);
    void setPaletteIndex(std::size_t i);
    // Inserts the selected type's template at the cursor.
    void insertSelectedModule();
    void showModuleHelp();          // the selected palette entry
    void showFieldHelp();           // whatever the cursor is on in the editor
    const std::string& helpTitle() const { return m_helpTitle; }
    const std::vector<std::string>& helpBody() const { return m_helpBody; }
    // An EMPTY title is what the renderer reads as "draw the key map instead".
    // Whoever opens the key map has to clear it, or F1 after any ^G shows that
    // field's help for ever.
    void clearHelp() { m_helpTitle.clear(); m_helpBody.clear(); }

    // --- the list, which is Arena's drop-down --------------------------------
    // Opens on the value of the `key = value` line the cursor is on, when that
    // key is an Enum or a Reference. Says why when it cannot.
    bool openPicker();
    const std::vector<std::string>& choices() const { return m_choices; }
    std::size_t choice() const { return m_choice; }
    void stepPicker(long long delta);
    void commitPicker();            // writes the value into the line
    void cancelPicker();

    // --- runs ----------------------------------------------------------------
    const std::vector<std::string>& runs() const { return m_runs; }
    std::size_t runIndex() const { return m_runIndex; }
    void stepRun(long long delta);
    void setRunIndex(std::size_t i);
    void addRunBlock();             // a new [Run] record, at the end
    void startRun();
    void advanceRun();
    void stopRun();
    const RunController* running() const { return m_run.get(); }

    // --- results -------------------------------------------------------------
    const std::string& results()     const { return m_results; }
    const std::string& resultsOf()   const { return m_resultsOf; }
    bool               haveResults() const { return !m_results.empty(); }
    std::size_t resultsScroll() const { return m_resultsScroll; }
    void scrollResults(long long delta);
    bool saveResults();
    const std::string& resultsPath() const { return m_resultsPath; }

    // --- the flow ------------------------------------------------------------
    std::size_t flowCursor() const { return m_flowCursor; }
    void stepFlow(long long delta);
    void gotoFlowBlock();           // jumps the text cursor to that record

    // --- quitting ------------------------------------------------------------
    bool wantsQuit() const { return m_quit; }
    void requestQuit();

private:
    void reparse();
    void rebuildPalette();

    TextBuffer  m_buffer;
    std::string m_path;
    std::string m_status;
    bool        m_dirty{false};
    bool        m_quit{false};

    View    m_view{View::Model};
    Pane    m_focus{Pane::Editor};
    Overlay m_overlay{Overlay::None};

    ModelDocument           m_document;
    std::vector<Diagnostic> m_diagnostics;

    std::vector<std::string> m_palette;
    std::size_t              m_paletteIndex{0};

    std::string              m_helpTitle;
    std::vector<std::string> m_helpBody;

    std::vector<std::string> m_choices;
    std::size_t              m_choice{0};

    std::vector<std::string> m_runs;
    std::size_t              m_runIndex{0};

    std::string m_clip;

    // A unique_ptr makes TuiState move-only, which is what it should be: it
    // owns a running simulation, and copying one would mean two.
    std::unique_ptr<RunController> m_run;
    std::string m_results;
    std::string m_resultsOf;        // which run produced them
    std::string m_resultsPath;
    std::size_t m_resultsScroll{0};

    std::size_t m_flowCursor{0};
};

}  // namespace des
