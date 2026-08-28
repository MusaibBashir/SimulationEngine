// ============================================================================
// Functions.hpp  --  v10: the built-in call table
// ============================================================================
// There is no separate "distribution field" in this engine any more. Arena's
// Delay cell holds an expression: 5 is constant, TRIA(1,2,3) samples each time,
// SetupTime*2 computes. So a distribution is just a function that samples, and
// the call OWNS one of the twelve IDistribution objects that already exist.
//
// EXPO is the exponential DISTRIBUTION; EXP is e^x. That collision is Arena's
// and is kept, because a model written against Arena's names should read the
// same here.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Expression.hpp"

namespace des {

bool isBuiltinFunction(const std::string& name);

// Reports unknown names and wrong arity into `out` and returns a placeholder
// rather than throwing, so the parser keeps going and reports every problem.
ExpressionPtr buildCall(const std::string& name,
                        std::vector<ExpressionPtr> args,
                        const std::vector<SourceSpan>& argSpans,
                        SourceSpan nameSpan,
                        std::vector<Diagnostic>& out);

}  // namespace des
