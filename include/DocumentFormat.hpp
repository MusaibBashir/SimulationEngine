// ============================================================================
// DocumentFormat.hpp  --  v11: the .des file, read and written
// ============================================================================
// Line-oriented records, chosen over JSON for four reasons that point the same
// way: it diffs PER FIELD, so changing a service time is a one-line diff; it
// reuses v10's Diagnostic and SourceSpan for positions; it carries comments,
// which people writing models actually want; and it is a third of the code.
//
// ROUND TRIP MUST BE BYTE-IDENTICAL, comments and blank lines included.
// Anything less means opening and saving a file in a front end silently
// rewrites it. That is why the reader hands the document its raw lines rather
// than only the values it understood.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"

namespace des {

struct ReadResult {
    ModelDocument           document;
    std::vector<Diagnostic> diagnostics;
    int                     formatVersion{0};
};

// Never throws. Malformed input becomes diagnostics and reading continues, so
// one call reports every problem in the file.
ReadResult readDocument(const std::string& text);
ReadResult readDocumentFile(const std::string& path);

std::string writeDocument(const ModelDocument& doc);
bool        writeDocumentFile(const ModelDocument& doc, const std::string& path);

}  // namespace des
