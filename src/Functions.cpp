#include "Functions.hpp"
#include <cmath>
#include <unordered_map>
#include "Build.hpp"
#include "RandomStream.hpp"

namespace des {
namespace {

// --- maths -----------------------------------------------------------------

enum class MathFn { Min, Max, Abs, Round, Trunc, Sqrt, Ln, Exp, Mod };

class MathCall : public IExpression {
    MathFn                     m_fn;
    std::string                m_name;
    std::vector<ExpressionPtr> m_args;

public:
    MathCall(MathFn fn, std::string name, std::vector<ExpressionPtr> args, SourceSpan span)
        : IExpression(span), m_fn(fn), m_name(std::move(name)), m_args(std::move(args)) {}

    Value evaluate(EvalContext& ctx) const override {
        std::vector<double> v;
        v.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) v.push_back(asNumber(a->evaluate(ctx)));
        switch (m_fn) {
            case MathFn::Min: { double r = v[0]; for (double x : v) if (x < r) r = x; return r; }
            case MathFn::Max: { double r = v[0]; for (double x : v) if (x > r) r = x; return r; }
            case MathFn::Abs:   return std::fabs(v[0]);
            case MathFn::Round: return std::floor(v[0] + 0.5);
            case MathFn::Trunc: return std::trunc(v[0]);
            case MathFn::Sqrt:
                if (v[0] < 0.0) throw ExpressionError("SQRT of a negative number");
                return std::sqrt(v[0]);
            case MathFn::Ln:
                if (v[0] <= 0.0) throw ExpressionError("LN of a non-positive number");
                return std::log(v[0]);
            case MathFn::Exp: return std::exp(v[0]);
            case MathFn::Mod:
                if (v[1] == 0.0) throw ExpressionError("MOD by zero");
                return std::fmod(v[0], v[1]);
        }
        throw ExpressionError("unreachable maths function");
    }

    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override {
        for (const ExpressionPtr& a : m_args) a->validate(vc, out);
    }

    std::string describe() const override {
        std::string s = m_name + "(";
        for (std::size_t i = 0; i < m_args.size(); ++i) {
            if (i) s += ", ";
            s += m_args[i]->describe();
        }
        return s + ")";
    }

    ExpressionPtr clone() const override {
        std::vector<ExpressionPtr> copies;
        copies.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) copies.push_back(a->clone());
        return std::make_unique<MathCall>(m_fn, m_name, std::move(copies), m_span);
    }

    void useStream(RandomStream* s) override {
        for (const ExpressionPtr& a : m_args) a->useStream(s);
    }

    void reset() override { for (const ExpressionPtr& a : m_args) a->reset(); }
};

// --- model state -----------------------------------------------------------

enum class StateFn { NQ, NR, MR, WIP };

// The one place an argument is NOT evaluated. NQ(Teller) means "the queue at
// the block called Teller", so `Teller` must not resolve as a variable -- the
// parser hands us a NameExpression and we keep its text.
class StateCall : public IExpression {
    StateFn     m_fn;
    std::string m_target;

public:
    StateCall(StateFn fn, std::string target, SourceSpan span)
        : IExpression(span), m_fn(fn), m_target(std::move(target)) {}

    Value evaluate(EvalContext& ctx) const override {
        const IModelState& s = ctx.requireState();
        switch (m_fn) {
            case StateFn::NQ:  return s.queueLength(m_target);
            case StateFn::NR:  return s.resourceBusy(m_target);
            case StateFn::MR:  return s.resourceCapacity(m_target);
            case StateFn::WIP: return s.numberInSystem();
        }
        throw ExpressionError("unreachable state function");
    }

    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}

    std::string describe() const override {
        const char* n = m_fn == StateFn::NQ ? "NQ" : m_fn == StateFn::NR ? "NR"
                      : m_fn == StateFn::MR ? "MR" : "WIP";
        return m_fn == StateFn::WIP ? std::string(n) + "()"
                                    : std::string(n) + "(" + m_target + ")";
    }

    ExpressionPtr clone() const override {
        return std::make_unique<StateCall>(m_fn, m_target, m_span);
    }
};

// --- distributions ---------------------------------------------------------

using DistBuilder = std::unique_ptr<IDistribution> (*)(const std::vector<double>&);

// Constant arguments build the distribution ONCE and keep a knowable mean.
// Computed arguments -- EXPO(Rate) where Rate is a variable -- must build per
// draw, and honestly have no mean the stability check can use in advance.
class DistributionCall : public IExpression {
    std::string                    m_name;
    DistBuilder                    m_build;
    std::vector<ExpressionPtr>     m_args;
    std::unique_ptr<IDistribution> m_fixed;
    RandomStream*                  m_stream{nullptr};

public:
    DistributionCall(std::string name, DistBuilder build,
                     std::vector<ExpressionPtr> args, SourceSpan span)
        : IExpression(span), m_name(std::move(name)), m_build(build), m_args(std::move(args)) {
        std::vector<double> constants;
        bool allConstant = true;
        for (const ExpressionPtr& a : m_args) {
            const auto* lit = dynamic_cast<const LiteralExpression*>(a.get());
            if (lit == nullptr || !isNumber(lit->value())) { allConstant = false; break; }
            constants.push_back(asNumber(lit->value()));
        }
        if (allConstant) m_fixed = m_build(constants);
    }

    Value evaluate(EvalContext& ctx) const override {
        RandomStream& rng = ctx.requireRng();
        if (m_fixed) return static_cast<double>(m_fixed->draw(rng));
        std::vector<double> v;
        v.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) v.push_back(asNumber(a->evaluate(ctx)));
        std::unique_ptr<IDistribution> d = m_build(v);
        if (m_stream) d->useStream(m_stream);
        return static_cast<double>(d->draw(rng));
    }

    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override {
        for (const ExpressionPtr& a : m_args) a->validate(vc, out);
    }

    std::optional<double> meanIfKnown() const override {
        if (m_fixed) return static_cast<double>(m_fixed->mean());
        return std::nullopt;
    }

    std::string describe() const override {
        std::string s = m_name + "(";
        for (std::size_t i = 0; i < m_args.size(); ++i) {
            if (i) s += ", ";
            s += m_args[i]->describe();
        }
        return s + ")";
    }

    ExpressionPtr clone() const override {
        std::vector<ExpressionPtr> copies;
        copies.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) copies.push_back(a->clone());
        auto c = std::make_unique<DistributionCall>(m_name, m_build, std::move(copies), m_span);
        c->useStream(m_stream);
        return c;
    }

    void useStream(RandomStream* s) override {
        m_stream = s;
        if (m_fixed) m_fixed->useStream(s);
    }

    void reset() override {
        for (const ExpressionPtr& a : m_args) a->reset();
        if (m_fixed) m_fixed->reset();
    }
};

// --- builders --------------------------------------------------------------

std::unique_ptr<IDistribution> buildExpo(const std::vector<double>& a) { return exponential(a[0]); }
std::unique_ptr<IDistribution> buildCons(const std::vector<double>& a) { return constant(a[0]); }
std::unique_ptr<IDistribution> buildUnif(const std::vector<double>& a) { return uniform(a[0], a[1]); }
std::unique_ptr<IDistribution> buildTria(const std::vector<double>& a) {
    return triangular(a[0], a[1], a[2]);
}
std::unique_ptr<IDistribution> buildNorm(const std::vector<double>& a) { return normal(a[0], a[1]); }
std::unique_ptr<IDistribution> buildWeib(const std::vector<double>& a) { return weibull(a[0], a[1]); }
std::unique_ptr<IDistribution> buildPois(const std::vector<double>& a) { return poisson(a[0]); }
std::unique_ptr<IDistribution> buildErla(const std::vector<double>& a) {
    return erlang(a[0], static_cast<int>(a[1]));
}
// Arena's LOGN takes the mean and sd of the VARIABLE, not of its logarithm.
// lognormalFrom does that conversion; lognormal() would silently take the
// wrong two numbers.
std::unique_ptr<IDistribution> buildLogn(const std::vector<double>& a) {
    return lognormalFrom(a[0], a[1]);
}

// Arena writes DISC(cum1, val1, cum2, val2, ...) with CUMULATIVE probabilities;
// this engine's Discrete takes individual ones and accumulates them itself.
// Differencing here is the whole conversion, and getting it wrong would be
// silent -- the probabilities would still sum to 1.
std::unique_ptr<IDistribution> buildDisc(const std::vector<double>& a) {
    std::vector<SimTime> values;
    std::vector<double>  individual;
    double previous = 0.0;
    for (std::size_t i = 0; i + 1 < a.size(); i += 2) {
        individual.push_back(a[i] - previous);
        previous = a[i];
        values.push_back(a[i + 1]);
    }
    return std::make_unique<Discrete>(values, individual);
}

// --- the table -------------------------------------------------------------

struct Entry {
    enum class Kind { Math, State, Dist };
    int         arity;          // -1 means variadic, at least one argument
    Kind        kind;
    MathFn      math{MathFn::Abs};
    StateFn     state{StateFn::WIP};
    DistBuilder dist{nullptr};
};

Entry mathEntry(int arity, MathFn f)  { Entry e{arity, Entry::Kind::Math};  e.math = f;  return e; }
Entry stateEntry(int arity, StateFn f){ Entry e{arity, Entry::Kind::State}; e.state = f; return e; }
Entry distEntry(int arity, DistBuilder b) {
    Entry e{arity, Entry::Kind::Dist}; e.dist = b; return e;
}

const std::unordered_map<std::string, Entry>& table() {
    static const std::unordered_map<std::string, Entry> t = {
        {"EXPO", distEntry(1, &buildExpo)},
        {"CONS", distEntry(1, &buildCons)},
        {"UNIF", distEntry(2, &buildUnif)},
        {"TRIA", distEntry(3, &buildTria)},
        {"NORM", distEntry(2, &buildNorm)},
        {"LOGN", distEntry(2, &buildLogn)},
        {"WEIB", distEntry(2, &buildWeib)},
        {"ERLA", distEntry(2, &buildErla)},
        {"POIS", distEntry(1, &buildPois)},
        {"DISC", distEntry(-1, &buildDisc)},

        {"NQ",  stateEntry(1, StateFn::NQ)},
        {"NR",  stateEntry(1, StateFn::NR)},
        {"MR",  stateEntry(1, StateFn::MR)},
        {"WIP", stateEntry(0, StateFn::WIP)},

        {"MIN",   mathEntry(-1, MathFn::Min)},
        {"MAX",   mathEntry(-1, MathFn::Max)},
        {"ABS",   mathEntry(1,  MathFn::Abs)},
        {"ROUND", mathEntry(1,  MathFn::Round)},
        {"TRUNC", mathEntry(1,  MathFn::Trunc)},
        {"SQRT",  mathEntry(1,  MathFn::Sqrt)},
        {"LN",    mathEntry(1,  MathFn::Ln)},
        {"EXP",   mathEntry(1,  MathFn::Exp)},
        {"MOD",   mathEntry(2,  MathFn::Mod)},
    };
    return t;
}

ExpressionPtr placeholder(SourceSpan span) {
    return std::make_unique<LiteralExpression>(Value(0.0), span);
}

}  // namespace

bool isBuiltinFunction(const std::string& name) { return table().count(name) != 0; }

ExpressionPtr buildCall(const std::string& name,
                        std::vector<ExpressionPtr> args,
                        const std::vector<SourceSpan>& argSpans,
                        SourceSpan nameSpan,
                        std::vector<Diagnostic>& out) {
    const auto it = table().find(name);
    if (it == table().end()) {
        out.push_back(Diagnostic{Severity::Error, nameSpan, "unknown function '" + name + "'"});
        return placeholder(nameSpan);
    }
    const Entry& entry = it->second;
    const int given = static_cast<int>(args.size());

    if (entry.arity >= 0 && given != entry.arity) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            name + " expects " + std::to_string(entry.arity) +
            (entry.arity == 1 ? " argument, got " : " arguments, got ") +
            std::to_string(given)});
        return placeholder(nameSpan);
    }
    if (entry.arity < 0 && given == 0) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            name + " expects at least one argument"});
        return placeholder(nameSpan);
    }
    if (name == "DISC" && given % 2 != 0) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            "DISC expects probability/value pairs, so an even number of arguments"});
        return placeholder(nameSpan);
    }

    switch (entry.kind) {
        case Entry::Kind::Math:
            return std::make_unique<MathCall>(entry.math, name, std::move(args), nameSpan);

        case Entry::Kind::State: {
            if (entry.state == StateFn::WIP)
                return std::make_unique<StateCall>(entry.state, "", nameSpan);

            if (const auto* asName = dynamic_cast<const NameExpression*>(args[0].get()))
                return std::make_unique<StateCall>(entry.state, asName->name(), nameSpan);

            if (const auto* lit = dynamic_cast<const LiteralExpression*>(args[0].get()))
                if (isText(lit->value()))
                    return std::make_unique<StateCall>(entry.state, asText(lit->value()), nameSpan);

            out.push_back(Diagnostic{Severity::Error,
                argSpans.empty() ? nameSpan : argSpans[0],
                name + " takes the NAME of a block, not a computed value"});
            return placeholder(nameSpan);
        }

        case Entry::Kind::Dist:
            return std::make_unique<DistributionCall>(name, entry.dist, std::move(args), nameSpan);
    }
    return placeholder(nameSpan);
}

}  // namespace des
