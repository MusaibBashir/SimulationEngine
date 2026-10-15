// ============================================================================
// TableStore.hpp  --  v16.1: variables, arrays, matrices and schedules as tables
// ============================================================================
// The Tables tab edits these; the Model tab's text holds everything else.
// They are still saved in the one .des file -- a marked section at the end --
// so the engine, the CLI and the regression harness read exactly the format
// they always have. This file is the part with no screen in it: taking a file
// apart, putting it back together, and the text-level edits a table needs.
//
// A cell is kept as the TEXT that was typed, so a value is written back as
// the person wrote it (0.60, not 0.6).
// ============================================================================

#pragma once
#include <string>
#include <utility>
#include <vector>
#include "ModelDocument.hpp"

namespace des {

enum class TableKind { Variable, Array, Matrix, Schedule };

const char* kindName(TableKind k);

struct Table {
    TableKind   kind{TableKind::Variable};
    std::string name;
    std::string note;                  // the comment above it, without '#'
    // Variable: 1 x 1. Array: rows x 1. Matrix: rows x columns.
    // Schedule: rows x 2, each row (Value, Duration).
    std::size_t rows{1};
    std::size_t columns{1};
    std::vector<std::string> cells;    // row-major, rows * columns of them
    std::string timeUnits{"Hours"};    // Schedule only
    std::string scheduleType{"Arrival"};

    std::string& at(std::size_t r, std::size_t c) { return cells[r * columns + c]; }
    const std::string& at(std::size_t r, std::size_t c) const { return cells[r * columns + c]; }
    // New cells are "0"; cells that no longer fit are dropped.
    void resize(std::size_t newRows, std::size_t newColumns);
    // Whether resize() to this shape would drop a cell that is not zero.
    bool resizeLoses(std::size_t newRows, std::size_t newColumns) const;
};

// The line a saved file's tables section starts with.
extern const char* const TABLES_MARKER;

struct SplitResult {
    std::string        modelText;   // the file without its tables
    std::vector<Table> tables;      // in the order they appeared
};

// Takes every [Variable], [Schedule] and [ScheduleRow] record out of a file,
// with the comment directly above each, and the tables marker.
SplitResult splitTables(const std::string& fileText);

// The file: the model text, then -- when there are tables -- the marker and
// every table in one fixed layout. Splitting what this returns gives back the
// same model text and tables, so saving twice writes the same bytes.
std::string joinTables(const std::string& modelText, const std::vector<Table>& tables);

// Where each table starts in joinTables()'s text, as 1-based lines, and the
// line after the last line of each. Used to put a compiler diagnostic that
// lands in the tables section on the table it is about.
std::vector<std::pair<std::size_t, std::size_t>>
tableLineRanges(const std::string& modelText, const std::vector<Table>& tables);

// A block copied from a spreadsheet or a CSV: one row per line, cells split
// on tabs if the line has any, else on commas or semicolons, else on spaces.
std::vector<std::vector<std::string>> parsePastedBlock(const std::string& text);

bool isNumberCell(const std::string& s);
// Letters, digits and underscores, not starting with a digit.
bool isTableName(const std::string& s);

// Every whole-word use of `from` on the value side of a `key = value` line
// becomes `to`. Comments and headers are left alone.
std::string renameInText(const std::string& text, const std::string& from,
                         const std::string& to);

// "Create Create_Pax_Suburb" for every record whose cells name `name` as a
// whole word -- what a rename or a delete would affect.
std::vector<std::string> usersOf(const ModelDocument& doc, const std::string& name);

}  // namespace des
