// ============================================================================
// TuiState.hpp  --  v13: everything the UI knows
// ============================================================================
// One object, because "what is on screen" is one question. The renderer reads
// it and the input layer writes it, and neither touches a terminal -- which is
// what lets a test drive the whole UI by constructing one of these and feeding
// it keys.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"
#include "ModuleSchema.hpp"

namespace des {

enum class Mode { Grid, Detail, Editing, Running, Confirm };

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

private:
    void rebuildTypes();
    void clampCursor();

    ModelDocument            m_document;
    std::string              m_path;
    std::string              m_status;
    std::vector<std::string> m_types;
    std::string              m_type;
    std::size_t              m_row{0};
    std::size_t              m_column{0};
    Mode                     m_mode{Mode::Grid};
    bool                     m_dirty{false};
    std::vector<Diagnostic>  m_diagnostics;
};

}  // namespace des
