// ============================================================================
// Compiler.hpp  --  v11: a document becomes a Model, or becomes diagnostics
// ============================================================================
// Four passes: schema, references, expressions, structure. EVERY PASS RUNS even
// when an earlier one found errors, wherever that is meaningful, because the
// point of the whole layer is to report every bad cell rather than the first.
//
// The exception is the structure pass: it cannot run with broken references, so
// it is skipped and `structureChecked` says so. Reporting a model as checked
// when it was not is the failure this project keeps refusing.

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Model.hpp"
#include "ModelDocument.hpp"

namespace des {

struct CompileResult {
    std::unique_ptr<Model>  model;              // null when there are errors
    std::vector<Diagnostic> diagnostics;
    bool                    structureChecked{false};
};

// Builds a Model of its own. Convenient for tests and for `des check`.
CompileResult compile(const ModelDocument& doc);

// Builds INTO an existing Model, which is what a SimulationSystem owns: Model
// is neither copyable nor movable, so a compiled one cannot be handed over.
// Returns false when the document did not produce a usable model.
bool compileInto(const ModelDocument& doc, Model& model,
                 std::vector<Diagnostic>& diagnostics,
                 bool* structureChecked = nullptr);

}  // namespace des
