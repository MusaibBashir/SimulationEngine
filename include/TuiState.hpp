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
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Compiler.hpp"
#include "Diagnostic.hpp"
#include "FlowGraph.hpp"
#include "ModelDocument.hpp"
#include "ModelGenerator.hpp"
#include "Recorder.hpp"
#include "Speech.hpp"
#include "ModuleSchema.hpp"
#include "RunController.hpp"
#include "TableStore.hpp"
#include "TextBuffer.hpp"

namespace des {

// What fills the body of the window. Each is reachable from the bar across the
// top and from one control key, because a bar you can only reach with a mouse
// is a bar half the people using it cannot reach at all.
enum class View { Model, Tables, Flow, Runs, Results };

// Which half of the Model view has the keyboard.
enum class Pane { Palette, Editor };

// Something drawn OVER the view, which takes every key until it closes. Modal
// on purpose: a list of choices that let you edit underneath it would be
// answering a question about text that had moved.
enum class Overlay { None, Help, Picker, Confirm, Running, Prompt, Ask };

// One Confirm overlay asks several questions; this says which, so a 'd' that
// means "discard and quit" can never be read as "discard and open a new file".
enum class ConfirmFor { Quit, NewFile, DeleteRun, OpenExisting,
                        ResizeTable, DeleteTable, RenameTable };

// What the one-line Prompt is asking for.
enum class PromptFor { None, NewFile, SaveAs };

// v17: one line of the Ask panel's conversation.
struct AskLine {
    enum class Kind { You, Understood, Assumed, Question, Error, Info } kind{Kind::Info};
    std::string text;
};

// v17: how F2 reaches the outside world. Installed by tui/main.cpp; a test
// installs fakes, so the panel is driven with no network and no thread.
struct AskHooks {
    // One description (or answer) -> a result. Called OFF the UI thread.
    std::function<GenerateResult(const std::string&, const JsonValue&, const LlmConfig&)> generate;
    // Persist a key typed into the panel. Empty: the key lasts this session.
    std::function<bool(const std::string&, std::string&)> saveKey;
    // Run some work in the background. Empty: a detached std::thread.
    std::function<void(std::function<void()>)> launch;
    // v17: speaking into it. The microphone, and the speech-to-text call --
    // the latter also run through `launch`, off the UI thread.
    std::function<std::unique_ptr<IRecorder>(std::string&)> openMicrophone;
    std::function<TranscriptResult(const std::string& wav, const LlmConfig&)> transcribe;
};

struct AskJob;   // a request in flight; TuiState.cpp
struct SttJob;   // a recording being transcribed; TuiState.cpp

// The longest recording, in seconds. A description is a few sentences; this
// is room for that and a pause to think, and it bounds what an upload costs.
constexpr double MAX_TALK_SECONDS = 120.0;

// Which half of the Runs tab has the keyboard.
enum class RunsFocus { List, Form };

// Which field a Picker writes into: the editor's current line, or the Runs
// form's selected field.
enum class PickerFor { Editor, RunField, TableUnits };

// v16.1: which of the Tables tab's three panels has the keyboard.
enum class TablesFocus { Kinds, Editor, List };

// A compiler diagnostic that landed in the tables section, put on the table
// (and, where it can say, the grid row) it is about.
struct TableIssue {
    std::size_t table{0};
    Severity    severity{Severity::Error};
    std::string message;
};

// v16: a typed model name becomes a path. A bare name goes in `defaultDir`, a
// name with a folder in it is used as given, and ".des" is added when there is
// no extension. Blank comes back empty, which the caller refuses.
std::string resolveModelName(const std::string& typed, const std::string& defaultDir);

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

    // v16: the Runs tab's form. Every edit rewrites one `key = value` line of
    // that run's [Run] block, so the text stays the only copy of the model.
    RunsFocus runsFocus() const { return m_runsFocus; }
    void setRunsFocus(RunsFocus f);
    const std::vector<std::string>& runFields() const { return m_runFields; }
    std::size_t runField() const { return m_runField; }
    void stepRunField(long long delta);
    void setRunField(std::size_t i);
    bool editingRunField() const { return m_editingRunField; }
    const std::string& runFieldDraft() const { return m_runDraft; }
    void beginRunFieldEdit();       // Yes/no and Enum fields open the picker
    void typeRunField(char c);
    void backspaceRunField();
    bool commitRunFieldEdit();      // false, with the reason, when refused
    void cancelRunFieldEdit();
    void showRunFieldHelp();
    // Blank removes the line. Refused -- text untouched -- when the value
    // would be an error in that cell.
    bool setRunValue(std::size_t run, const std::string& column, const std::string& value);
    void requestDeleteRun();        // asks first
    bool deleteRun(std::size_t run);

    // v16: new files have names. ^N asks for one (after asking about unsaved
    // work), and a model opened with no file asks on its first ^S rather than
    // quietly becoming untitled.des.
    void requestNewFile();
    void openPrompt(PromptFor why);
    PromptFor promptFor() const { return m_promptFor; }
    const std::string& promptText() const { return m_promptText; }
    void typePrompt(char c) { m_promptText.push_back(c); }
    void backspacePrompt() { if (!m_promptText.empty()) m_promptText.pop_back(); }
    void commitPrompt();
    void cancelPrompt();
    void answerConfirm(char c);     // NewFile and OpenExisting
    bool unnamed() const { return m_unnamed; }
    void setUnnamed(bool on) { m_unnamed = on; }
    void setDefaultDirectory(std::string dir) { m_defaultDir = std::move(dir); }
    void newBlankModel(const std::string& path);

    ConfirmFor confirmFor() const { return m_confirmFor; }
    PickerFor  pickerFor()  const { return m_pickerFor; }
    const std::string& pickerTitle() const { return m_pickerTitle; }

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

    // v16.2: the flow as a graph, laid out by Graphviz and drawn in ASCII.
    // Built on demand and kept until the model changes, so dot runs once per
    // change rather than once per frame. Null when dot is missing or failed;
    // flowGraphError() then says why and the tab shows the list instead.
    const AsciiGraph* flowGraph() const;
    const std::string& flowGraphError() const { return m_flowError; }
    bool flowAsList() const { return m_flowAsList; }
    // Where the graph's view is, when somebody has moved it. Until then -- and
    // again after Up/Down or Home -- it follows the chosen block.
    bool flowViewPinned() const { return m_flowPinned; }
    int  flowViewX() const { return m_flowViewX; }
    int  flowViewY() const { return m_flowViewY; }
    void setFlowView(int x, int y) { m_flowViewX = x; m_flowViewY = y; m_flowPinned = true; }
    void followFlowCursor() { m_flowPinned = false; }
    void setFlowAsList(bool on) { m_flowAsList = on; }
    // P: <model's folder>/images/<model>-flow.png, then opened.
    bool saveFlowPicture();
    // How a file is opened with its default program. The terminal layer
    // installs it; tests install one that records the path.
    void setOpener(std::function<bool(const std::string&)> open) { m_opener = std::move(open); }

    // --- the Tables tab (v16.1) ---------------------------------------------
    // Variables, arrays, matrices and schedules, edited as tables. They are
    // saved in the same file but are not in buffer(): the model that compiles
    // and runs is joinedText().
    const std::vector<Table>& tables() const { return m_tables; }
    std::string joinedText() const;
    const std::vector<TableIssue>& tableIssues() const { return m_tableIssues; }
    std::size_t tableErrorCount() const;

    TablesFocus tablesFocus() const { return m_tablesFocus; }
    void setTablesFocus(TablesFocus f);
    std::size_t kindIndex() const { return m_kindIndex; }
    void stepKind(long long delta);
    void setKindIndex(std::size_t i);
    void createTable();                       // of the selected kind
    std::size_t tableIndex() const { return m_tableIndex; }
    void stepTable(long long delta);
    void selectTable(std::size_t i);
    const Table* currentTable() const;

    // The editor's cursor: a header field (0 .. headerFields().size() - 1) or
    // the grid (field == headerFields().size(), at cellRow/cellColumn).
    std::vector<std::string> headerFields() const;   // of the current table
    std::string headerValue(std::size_t field) const;
    std::size_t tableField() const { return m_tableField; }
    bool onGrid() const;
    std::size_t cellRow() const { return m_cellRow; }
    std::size_t cellColumn() const { return m_cellColumn; }
    void moveTableCursor(long long rows, long long columns);
    void setTableCursor(std::size_t field, std::size_t row, std::size_t column);
    bool editingTable() const { return m_editingTable; }
    const std::string& tableDraft() const { return m_tableDraft; }
    void beginTableEdit(bool keepValue);
    void typeTable(char c);
    void backspaceTable();
    bool commitTableEdit();
    void cancelTableEdit();
    // Keeps the edit, then moves -- how a block pasted into the terminal
    // (cells separated by Tab, rows by Enter) lands as it looked in Excel.
    void tableTab();
    void tableEnter();
    void insertTableRow();                    // after the cursor
    void deleteTableRow();
    void pasteIntoTable(const std::string& block);
    void requestDeleteTable();
    void undoTables();
    void redoTables();
    void showTableHelp();
    std::vector<std::string> tableUsers() const;     // of the current table

    // --- describe a model (v17, F2) -------------------------------------------
    // Somebody says or types what happens; a small language model fills in a
    // Scenario and our own code builds the model, which replaces the text as
    // ONE edit -- ^Z takes it back. Missing facts come back as questions, and
    // the answer goes in the same box. The request runs off the UI thread, so
    // the screen keeps drawing and Esc stops waiting.
    void setAskHooks(AskHooks hooks) { m_askHooks = std::move(hooks); }
    void setLlmConfig(LlmConfig c) { m_llm = std::move(c); }
    const LlmConfig& llmConfig() const { return m_llm; }

    void openAsk();
    void closeAsk();
    const std::string& askDraft() const { return m_askDraft; }
    void typeAsk(char c);
    void backspaceAsk();
    void pasteIntoAsk(const std::string& text);
    void clearAsk() { m_askDraft.clear(); }
    void commitAsk();               // Enter: send, or save the key being typed
    void escapeAsk();               // stop waiting, leave key entry, or close
    void beginKeyEntry();           // ^K in the panel
    bool enteringKey() const { return m_askKeyEntry; }
    // Anything under way -- a request out, a recording, a transcription. The
    // main loop keeps redrawing while this is true; typing is ignored.
    bool askBusy() const { return m_askJob != nullptr || m_recorder != nullptr || m_sttJob != nullptr; }

    // v17: ^T in the panel, or F3 anywhere. Press to start, press (or Enter)
    // to stop; the transcript goes into the box, to be read before sending.
    void startTalking();
    void stopTalking();
    bool   recording() const { return m_recorder != nullptr; }
    bool   transcribing() const { return m_sttJob != nullptr; }
    double recordingSeconds() const { return m_recorder ? m_recorder->seconds() : 0.0; }
    float  recordingLevel() const { return m_recorder ? m_recorder->level() : 0.0f; }
    // Called by the main loop while askBusy(): applies a finished answer.
    void pollAsk();
    const std::vector<AskLine>& askLog() const { return m_askLog; }
    // The filled-in form behind the model, so a later "add a second teller"
    // changes it rather than starting again.
    const JsonValue& askScenario() const { return m_askScenario; }

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
    RunsFocus                m_runsFocus{RunsFocus::List};
    std::vector<std::string> m_runFields;
    std::size_t              m_runField{0};
    bool                     m_editingRunField{false};
    std::string              m_runDraft;

    ConfirmFor  m_confirmFor{ConfirmFor::Quit};
    PromptFor   m_promptFor{PromptFor::None};
    std::string m_promptText;
    std::string m_pendingPath;      // the name OpenExisting is asking about
    bool        m_unnamed{false};
    std::string m_defaultDir{"."};
    PickerFor   m_pickerFor{PickerFor::Editor};
    std::string m_pickerTitle;

    std::string m_clip;

    // A unique_ptr makes TuiState move-only, which is what it should be: it
    // owns a running simulation, and copying one would mean two.
    std::unique_ptr<RunController> m_run;
    std::string m_results;
    std::string m_resultsOf;        // which run produced them
    std::string m_resultsPath;
    std::size_t m_resultsScroll{0};

    std::size_t m_flowCursor{0};
    bool        m_flowAsList{false};
    bool        m_flowPinned{false};
    int         m_flowViewX{0};
    int         m_flowViewY{0};
    // The layout is a CACHE of the document, so filling it from a const
    // renderer is not a change anybody can see.
    mutable std::string m_flowDot;
    mutable AsciiGraph  m_flowGraph;
    mutable bool        m_flowReady{false};
    mutable std::string m_flowError;
    std::function<bool(const std::string&)> m_opener;

    std::vector<Table>              m_tables;
    std::vector<TableIssue>         m_tableIssues;
    std::vector<std::vector<Table>> m_tableUndo;
    std::vector<std::vector<Table>> m_tableRedo;
    TablesFocus m_tablesFocus{TablesFocus::List};
    std::size_t m_kindIndex{0};
    std::size_t m_tableIndex{0};
    std::size_t m_tableField{0};
    std::size_t m_cellRow{0};
    std::size_t m_cellColumn{0};
    std::size_t m_entryColumn{0};     // where Enter returns to, after Tabs
    bool        m_editingTable{false};
    std::string m_tableDraft;
    // What a Confirm about a table is waiting to do.
    std::size_t m_pendingRows{0}, m_pendingColumns{0};
    std::string m_pendingName;

    AskHooks                 m_askHooks;
    LlmConfig                m_llm;
    std::string              m_askDraft;
    bool                     m_askKeyEntry{false};
    std::string              m_askPending;     // a description waiting on a key
    std::vector<AskLine>     m_askLog;
    JsonValue                m_askScenario;
    std::string              m_askBuiltText;   // the text the last answer wrote
    std::shared_ptr<AskJob>  m_askJob;
    std::unique_ptr<IRecorder> m_recorder;
    std::shared_ptr<SttJob>  m_sttJob;
    bool                     m_talkAfterKey{false};
    void startAsk(const std::string& text);
    void replaceWith(TuiState next);    // keeps the session's settings

    void tablesChanging();            // snapshot for ^Z, before a change
    void tablesChanged();             // dirty, and re-read the joined model
    bool setHeaderField(std::size_t field, const std::string& value);
    void applyRename(const std::string& to, bool references);
    void clampTableCursor();
};

}  // namespace des
