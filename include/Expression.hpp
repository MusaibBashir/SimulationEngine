// ============================================================================
// Expression.hpp  --  v10: the AST a model's text becomes
// ============================================================================
// meanIfKnown is the one worth explaining. Model::offeredLoad() calls
// dist->mean() to check stability before a run, and an arbitrary expression has
// no knowable mean -- EXPO(Rate) depends on a variable that changes during the
// run. nullopt means "not computable in advance", NOT "zero", and the report
// then says it could not verify rather than passing silently.
//
// That is VisitRatios::exact a second time: say you do not know rather than
// guess, because a guessed stability check passes when it should not.

#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Distribution.hpp"
#include "EvalContext.hpp"
#include "Value.hpp"

namespace des {

class Entity;

enum class UnaryOp  { Negate, Not };
enum class BinaryOp {
    Add, Subtract, Multiply, Divide, Modulo, Power,
    Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
    And, Or
};

const char* spelling(UnaryOp op);
const char* spelling(BinaryOp op);

class IExpression {
protected:
    SourceSpan m_span;

public:
    explicit IExpression(SourceSpan span) : m_span(span) {}
    virtual ~IExpression() = default;
    IExpression(const IExpression&) = delete;
    IExpression& operator=(const IExpression&) = delete;

    SourceSpan span() const { return m_span; }

    virtual Value evaluate(EvalContext& ctx) const = 0;

    // Append every problem found; do NOT throw and do NOT stop at the first.
    virtual void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const = 0;

    virtual std::optional<double> meanIfKnown() const { return std::nullopt; }

    virtual std::string describe() const = 0;
    virtual std::unique_ptr<IExpression> clone() const = 0;

    // Give every sampling site below this node its own stream. This is what
    // finally separates a Delay's draws from a Decide's: they shared a stream
    // because they shared a code path, and now they are separate objects.
    virtual void useStream(RandomStream* s);

    // Back to the t=0 condition between replications. Only a Deterministic
    // distribution has anything to do -- it walks a cursor through a list --
    // but a node that owned a distribution used to reset it, so an expression
    // that owns one must too.
    virtual void reset();
};

using ExpressionPtr = std::unique_ptr<IExpression>;

class LiteralExpression : public IExpression {
    Value m_value;
public:
    LiteralExpression(Value v, SourceSpan span) : IExpression(span), m_value(std::move(v)) {}
    const Value& value() const { return m_value; }
    Value evaluate(EvalContext&) const override { return m_value; }
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override { return formatValue(m_value); }
    ExpressionPtr clone() const override;
};

// Resolved LATE, never by the parser: this node holds a string, and what that
// string is gets decided by validate() and evaluate(). That is what stops the
// parser going stale when v11 adds a Variable data module.
class NameExpression : public IExpression {
    std::string m_name;
public:
    NameExpression(std::string name, SourceSpan span)
        : IExpression(span), m_name(std::move(name)) {}
    const std::string& name() const { return m_name; }
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::string describe() const override { return m_name; }
    ExpressionPtr clone() const override;
};

class UnaryExpression : public IExpression {
    UnaryOp       m_op;
    ExpressionPtr m_operand;
public:
    UnaryExpression(UnaryOp op, ExpressionPtr operand, SourceSpan span)
        : IExpression(span), m_op(op), m_operand(std::move(operand)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override;
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
    void reset() override;
};

class BinaryExpression : public IExpression {
    BinaryOp      m_op;
    ExpressionPtr m_left;
    ExpressionPtr m_right;
public:
    BinaryExpression(BinaryOp op, ExpressionPtr left, ExpressionPtr right, SourceSpan span)
        : IExpression(span), m_op(op), m_left(std::move(left)), m_right(std::move(right)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::string describe() const override;
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
    void reset() override;
};

// The two adapters below are why v10 breaks nothing. The old C++ API does not
// become a second code path kept alive in parallel -- it becomes a constructor
// for a node in the one and only representation.

class LambdaExpression : public IExpression {
public:
    using Predicate = std::function<bool(const Entity&)>;
private:
    Predicate m_predicate;
public:
    explicit LambdaExpression(Predicate p)
        : IExpression(SourceSpan{0, 0}), m_predicate(std::move(p)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::string describe() const override { return "<C++ predicate>"; }
    ExpressionPtr clone() const override;
};

class DistributionExpression : public IExpression {
    std::unique_ptr<IDistribution> m_distribution;
public:
    explicit DistributionExpression(std::unique_ptr<IDistribution> d,
                                    SourceSpan span = SourceSpan{0, 0})
        : IExpression(span), m_distribution(std::move(d)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override { return m_distribution->describe(); }
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
    void reset() override { m_distribution->reset(); }
};

}  // namespace des
