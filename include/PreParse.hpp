// ============================================================================
// PreParse.hpp  --  v17: read the numbers before the language model does
// ============================================================================
// The things a small model gets wrong are exactly the things a few hundred
// lines of code get right: "two" is 2, "half an hour" is 30 minutes, and "30
// an hour" is one every 2 minutes -- an inversion a 70B model will now and
// then skip, producing a model thirty times too busy that compiles cleanly.
//
// So the description is normalised first: number words become digits, and
// every rate gets its interarrival time written beside it. The model still
// reads the person's sentence; it just no longer has to do the arithmetic.

#pragma once
#include <string>
#include <vector>

namespace des {

struct PreParsed {
    std::string              text;    // the description, numbers normalised
    std::vector<std::string> notes;   // "30 per hour = one every 2 minutes"
};

PreParsed preParse(const std::string& description);

}  // namespace des
