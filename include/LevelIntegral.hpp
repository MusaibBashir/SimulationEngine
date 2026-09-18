// ============================================================================
// LevelIntegral.hpp  --  v16: the time-average of a count that steps up and
// down: entities waiting in a Match lane, entities on the road in a Route.
// ============================================================================
// Each block keeps its own and brings it up to date whenever the count
// changes, so nothing has to visit it on every event the way Station's
// Statistics are visited. Read it with average(now), which closes the last
// rectangle without changing anything.
// ============================================================================

#pragma once
#include "Common.hpp"

namespace des {

class LevelIntegral {
    double  m_area{0.0};
    SimTime m_last{0.0};
    SimTime m_since{0.0};
    long long m_level{0};

public:
    void change(SimTime now, long long delta) {
        m_area += static_cast<double>(m_level) * (now - m_last);
        m_last = now;
        m_level += delta;
    }
    // Warm-up: forget the area, keep the level -- whoever is waiting still is.
    void restart(SimTime now) { m_area = 0.0; m_last = now; m_since = now; }
    void reset() { m_area = 0.0; m_last = 0.0; m_since = 0.0; m_level = 0; }

    long long level() const { return m_level; }
    double average(SimTime now) const {
        const SimTime span = now - m_since;
        if (span <= 0.0) return static_cast<double>(m_level);
        return (m_area + static_cast<double>(m_level) * (now - m_last)) / span;
    }
};

}  // namespace des
