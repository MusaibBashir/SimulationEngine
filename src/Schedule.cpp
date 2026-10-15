// ============================================================================
// Schedule.cpp
// ============================================================================

#include "Schedule.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include "EvalContext.hpp"
#include "ModelError.hpp"
#include "RandomStream.hpp"

namespace des {

bool timeUnitFrom(const std::string& text, TimeUnit& out) {
    if (text == "Seconds") { out = TimeUnit::Seconds; return true; }
    if (text == "Minutes") { out = TimeUnit::Minutes; return true; }
    if (text == "Hours")   { out = TimeUnit::Hours;   return true; }
    if (text == "Days")    { out = TimeUnit::Days;    return true; }
    return false;
}

double unitsPer(TimeUnit from, TimeUnit to) {
    const auto seconds = [](TimeUnit u) {
        switch (u) {
            case TimeUnit::Seconds: return 1.0;
            case TimeUnit::Minutes: return 60.0;
            case TimeUnit::Hours:   return 3600.0;
            case TimeUnit::Days:    return 86400.0;
        }
        return 1.0;
    };
    return seconds(from) / seconds(to);
}

ArrivalSchedule::ArrivalSchedule(std::vector<ScheduleSlot> slots) : m_slots(std::move(slots)) {
    for (const ScheduleSlot& s : m_slots) {
        if (s.rate < 0.0 || s.duration < 0.0)
            throw ModelError("a schedule row cannot have a negative rate or duration");
        m_cycle += s.duration;
        if (s.duration > 0.0) m_peak = std::max(m_peak, s.rate);
    }
    if (m_cycle <= 0.0) throw ModelError("a schedule's durations add up to zero");
    if (m_peak <= 0.0) throw ModelError("every rate in this schedule is zero, so nothing would arrive");
}

double ArrivalSchedule::rateAt(SimTime t) const {
    double into = std::fmod(static_cast<double>(t), m_cycle);
    if (into < 0.0) into += m_cycle;
    for (const ScheduleSlot& s : m_slots) {
        if (into < s.duration) return s.rate;
        into -= s.duration;
    }
    return m_slots.back().rate;   // rounding at the very end of the cycle
}

SimTime ArrivalSchedule::nextArrival(SimTime now, RandomStream& rng) const {
    SimTime t = now;
    for (;;) {
        // Never log(0), whatever a mirrored (antithetic) stream hands back.
        const double u = rng.u01();
        t += static_cast<SimTime>(-std::log(u > 0.0 ? u : 1e-300) / m_peak);
        if (rng.u01() * m_peak < rateAt(t)) return t;
    }
}

ScheduledGapExpression::ScheduledGapExpression(std::string scheduleName, ArrivalSchedule schedule)
    : IExpression(SourceSpan{}), m_scheduleName(std::move(scheduleName)),
      m_schedule(std::move(schedule)) {}

Value ScheduledGapExpression::evaluate(EvalContext& ctx) const {
    RandomStream& rng = m_stream != nullptr ? *m_stream : ctx.requireRng();
    const SimTime now = ctx.requireState().now();
    return static_cast<double>(m_schedule.nextArrival(now, rng) - now);
}

ExpressionPtr ScheduledGapExpression::clone() const {
    auto copy = std::make_unique<ScheduledGapExpression>(m_scheduleName, m_schedule);
    copy->m_stream = m_stream;
    return copy;
}

}  // namespace des
