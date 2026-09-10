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

// Every name a Reference column may hold, in document order. "Block" is the
// pseudo-type meaning any flowchart block; anything else names one module type.
//
// PUBLIC because a front end offering a pick list must offer exactly what the
// reference pass accepts. Two lists built from the same idea drift, and the
// way that failure shows up is a menu whose choices are then rejected -- so
// the reference pass is written in terms of this one.
std::vector<std::string> referenceCandidates(const ModelDocument& doc,
                                             const std::string& targetType);

// Which line of the FILE a diagnostic is about, 1-based, or 0 when it cannot
// say. A reader diagnostic already carries a line; a compiler diagnostic
// carries a cell, and a cell knows where it was read from -- so the join lives
// here rather than in whatever is drawing.
//
// Never guesses. A document built in code has no lines, and a text editor
// showing an error beside an arbitrary one is worse than showing it beside
// none: the fourth occurrence of the rule this project keeps rediscovering.
std::size_t lineOf(const ModelDocument& doc, const Diagnostic& diagnostic);

// Builds INTO an existing Model, which is what a SimulationSystem owns: Model
// is neither copyable nor movable, so a compiled one cannot be handed over.
// Returns false when the document did not produce a usable model.
bool compileInto(const ModelDocument& doc, Model& model,
                 std::vector<Diagnostic>& diagnostics,
                 bool* structureChecked = nullptr);

}  // namespace des
