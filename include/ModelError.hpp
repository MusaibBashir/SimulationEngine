// ============================================================================
// ModelError.hpp  --  v5: telling the user what they got wrong
// ============================================================================
// Through v4 every user-facing mistake -- a duplicate station name, connecting
// to a station that does not exist, forgetting the entry point -- was an
// `assert`.
//
// *** assert IS THE WRONG TOOL FOR THIS. ***
// Asserts compile to NOTHING under -DNDEBUG. In a release build, `connect("A",
// "Typo")` would have done nothing at all, and the model would have run happily
// with entities leaving after the first station. Wrong answers, no message.
//
// The distinction is worth learning properly:
//
//   assert     -> an INVARIANT. Something the code guarantees, and a failure
//                 means the engine has a bug. Users cannot trigger it.
//                 e.g. "the queue rule returned an out-of-range index".
//
//   exception  -> VALIDATION. Something the user supplied and got wrong. It
//                 must be checked in every build, and the message is for them.
//                 e.g. "no station named 'Typo'".
//
// Same rule applies to config-file input, which is why v3 and v4 both declined
// to add it until this existed.

#pragma once

#include <stdexcept>
#include <string>

namespace des {

class ModelError : public std::runtime_error {
public:
    explicit ModelError(const std::string& what) : std::runtime_error(what) {}
};

}  // namespace des
