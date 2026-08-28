#include "Expression.hpp"
#include <cmath>
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "VariableStore.hpp"

namespace des {

void IExpression::useStream(RandomStream*) {}

const char* spelling(UnaryOp op) {
    switch (op) {
        case UnaryOp::Negate: return "-";
        case UnaryOp::Not:    return "!";
    }
    return "?";
}

const char* spelling(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";       case BinaryOp::Subtract: return "-";
        case BinaryOp::Multiply: return "*";  case BinaryOp::Divide: return "/";
        case BinaryOp::Modulo: return "%";    case BinaryOp::Power: return "^";
        case BinaryOp::Less: return "<";      case BinaryOp::LessEqual: return "<=";
        case BinaryOp::Greater: return ">";   case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::Equal: return "==";    case BinaryOp::NotEqual: return "!=";
        case BinaryOp::And: return "&&";      case BinaryOp::Or: return "||";
    }
    return "?";
}

std::optional<double> LiteralExpression::meanIfKnown() const {
    if (isNumber(m_value)) return asNumber(m_value);
    return std::nullopt;
}

ExpressionPtr LiteralExpression::clone() const {
    return std::make_unique<LiteralExpression>(m_value, m_span);
}

// Resolution order is fixed and total: special names, then variables, then
// attributes. A variable colliding with an attribute is refused at DECLARATION
// (VariableStore), so this order can never silently pick the wrong one of two
// live candidates.
Value NameExpression::evaluate(EvalContext& ctx) const {
    if (m_name == "TNOW")        return static_cast<double>(ctx.requireState().now());
    if (m_name == "Entity.Type") return ctx.entityType();
    if (ctx.variables() != nullptr && ctx.variables()->has(m_name))
        return ctx.variables()->get(m_name);
    return ctx.attribute(m_name);
}

void NameExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    if (m_name == "TNOW") return;

    if (m_name == "Entity.Type") {
        if (vc.field == FieldContext::NoEntity)
            out.push_back(Diagnostic{Severity::Error, m_span,
                "'Entity.Type' needs an entity, but this field is evaluated without one"});
        return;
    }

    if (vc.isVariable(m_name)) return;

    if (vc.isAttribute(m_name)) {
        if (vc.field == FieldContext::NoEntity)
            out.push_back(Diagnostic{Severity::Error, m_span,
                "'" + m_name + "' is an entity attribute, but this field is evaluated "
                "without an entity"});
        return;
    }

    out.push_back(Diagnostic{Severity::Error, m_span,
        "unknown name '" + m_name + "' -- not a declared variable or attribute"});
}

ExpressionPtr NameExpression::clone() const {
    return std::make_unique<NameExpression>(m_name, m_span);
}

Value UnaryExpression::evaluate(EvalContext& ctx) const {
    const double v = asNumber(m_operand->evaluate(ctx));
    switch (m_op) {
        case UnaryOp::Negate: return -v;
        case UnaryOp::Not:    return (v == 0.0) ? 1.0 : 0.0;
    }
    throw ExpressionError("unreachable unary operator");
}

void UnaryExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    m_operand->validate(vc, out);
}

std::optional<double> UnaryExpression::meanIfKnown() const {
    if (m_op != UnaryOp::Negate) return std::nullopt;
    if (auto inner = m_operand->meanIfKnown()) return -*inner;
    return std::nullopt;
}

std::string UnaryExpression::describe() const {
    return std::string(spelling(m_op)) + m_operand->describe();
}

ExpressionPtr UnaryExpression::clone() const {
    return std::make_unique<UnaryExpression>(m_op, m_operand->clone(), m_span);
}

void UnaryExpression::useStream(RandomStream* s) { m_operand->useStream(s); }

Value BinaryExpression::evaluate(EvalContext& ctx) const {
    // && and || SHORT-CIRCUIT. Not an optimisation: it is what lets
    // `Count > 0 && Total / Count > 5` be written at all.
    if (m_op == BinaryOp::And) {
        if (!truthy(m_left->evaluate(ctx))) return 0.0;
        return truthy(m_right->evaluate(ctx)) ? 1.0 : 0.0;
    }
    if (m_op == BinaryOp::Or) {
        if (truthy(m_left->evaluate(ctx))) return 1.0;
        return truthy(m_right->evaluate(ctx)) ? 1.0 : 0.0;
    }

    const Value l = m_left->evaluate(ctx);
    const Value r = m_right->evaluate(ctx);

    // Text compares only for equality -- the one thing strings are for.
    if (isText(l) || isText(r)) {
        if (m_op == BinaryOp::Equal || m_op == BinaryOp::NotEqual) {
            if (isText(l) != isText(r))
                throw ExpressionError("cannot compare text with a number");
            const bool same = (asText(l) == asText(r));
            return ((m_op == BinaryOp::Equal) == same) ? 1.0 : 0.0;
        }
        throw ExpressionError(std::string("'") + spelling(m_op) +
                              "' cannot be applied to text");
    }

    const double a = asNumber(l);
    const double b = asNumber(r);
    switch (m_op) {
        case BinaryOp::Add:      return a + b;
        case BinaryOp::Subtract: return a - b;
        case BinaryOp::Multiply: return a * b;
        case BinaryOp::Divide:
            if (b == 0.0) throw ExpressionError("division by zero");
            return a / b;
        case BinaryOp::Modulo:
            if (b == 0.0) throw ExpressionError("modulo by zero");
            return std::fmod(a, b);
        case BinaryOp::Power:        return std::pow(a, b);
        case BinaryOp::Less:         return a <  b ? 1.0 : 0.0;
        case BinaryOp::LessEqual:    return a <= b ? 1.0 : 0.0;
        case BinaryOp::Greater:      return a >  b ? 1.0 : 0.0;
        case BinaryOp::GreaterEqual: return a >= b ? 1.0 : 0.0;
        case BinaryOp::Equal:        return a == b ? 1.0 : 0.0;
        case BinaryOp::NotEqual:     return a != b ? 1.0 : 0.0;
        case BinaryOp::And: case BinaryOp::Or: break;   // handled above
    }
    throw ExpressionError("unreachable binary operator");
}

void BinaryExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    m_left->validate(vc, out);
    m_right->validate(vc, out);
}

std::string BinaryExpression::describe() const {
    return "(" + m_left->describe() + " " + spelling(m_op) + " " + m_right->describe() + ")";
}

ExpressionPtr BinaryExpression::clone() const {
    return std::make_unique<BinaryExpression>(m_op, m_left->clone(), m_right->clone(), m_span);
}

void BinaryExpression::useStream(RandomStream* s) {
    m_left->useStream(s);
    m_right->useStream(s);
}

Value LambdaExpression::evaluate(EvalContext& ctx) const {
    if (ctx.entity() == nullptr)
        throw ExpressionError("a C++ predicate was evaluated with no entity");
    return m_predicate(*ctx.entity()) ? 1.0 : 0.0;
}

ExpressionPtr LambdaExpression::clone() const {
    return std::make_unique<LambdaExpression>(m_predicate);
}

Value DistributionExpression::evaluate(EvalContext& ctx) const {
    return static_cast<double>(m_distribution->draw(ctx.requireRng()));
}

std::optional<double> DistributionExpression::meanIfKnown() const {
    return static_cast<double>(m_distribution->mean());
}

ExpressionPtr DistributionExpression::clone() const {
    return std::make_unique<DistributionExpression>(m_distribution->clone(), m_span);
}

void DistributionExpression::useStream(RandomStream* s) { m_distribution->useStream(s); }

}  // namespace des
