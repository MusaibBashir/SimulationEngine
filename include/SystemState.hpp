// ============================================================================
// SystemState.hpp  --  the STATE VECTOR
// ============================================================================
// Theory: "A group of variables depicting the status of the system, SUFFICIENT
// TO DESCRIBE IT AT ANY INSTANT (e.g. server busy/idle plus queue length)."
//
// "Sufficient" is the operative word. If you froze the simulation and were handed
// only this object plus the FEL, you should be able to resume correctly.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Include "Common.hpp" (ResourceState).
//
// [3] class SystemState, private data -- v1 models a SINGLE-SERVER queue:
//     [3a] int m_numberInSystem     : entities present (waiting + in service)
//     [3b] int m_numberInQueue      : entities waiting only
//     [3c] ResourceState m_serverStatus
//
// [4] PUBLIC INTERFACE:
//     [4a] Default constructor: both ints to 0, status to ResourceState::Idle.
//     [4b] int numberInSystem() const
//     [4c] int numberInQueue() const
//     [4d] ResourceState serverStatus() const
//     [4e] void setNumberInSystem(int)
//     [4f] void setNumberInQueue(int)
//     [4g] void setServerStatus(ResourceState)
//          Plain setters, and in v2 SimulationSystem changes state ONLY through
//          these -- never by reaching into the fields. That way you can later
//          put a trace-print or a statistics hook inside one setter and catch
//          every state change in the program from one place.
//     [4h] void reset()  -- v1 stub; back to the t=0 values.
//
// [5] Close class with semicolon.
//
// ---------------- *** THE DELIBERATE SMELL -- READ THIS *** ----------------
// This class DUPLICATES information. m_numberInQueue is also EntityQueue::length().
// m_serverStatus is also Resource::state(). Two sources of truth for one fact --
// exactly what Resource.hpp [4e] told you never to do.
//
// It is left in on purpose so you hit it yourself in v2, when you forget to
// update one of the two and your statistics go quietly wrong.
//
// The two honest ways out, DECIDE IN v2:
//   (a) SystemState becomes a VIEW -- it stores nothing, and each getter asks
//       the relevant Resource/EntityQueue for the answer. Always correct,
//       slightly slower, needs references to those objects.
//   (b) SystemState stays a SNAPSHOT -- a dumb struct built on demand for
//       logging/tracing/debug output, never read by simulation logic.
// Copy this paragraph into the header so future-you finds the decision pending.

#pragma once

#include "Common.hpp"

class SystemState {
private:
    int m_numberInSystem{0};
    int m_numberInQueue{0};
    ResourceState m_serverStatus{ResourceState::Idle};

public:
    SystemState() = default;

    int numberInSystem() const { return m_numberInSystem; }
    int numberInQueue() const { return m_numberInQueue; }
    ResourceState serverStatus() const { return m_serverStatus; }

    void setNumberInSystem(int count) { m_numberInSystem = count; }
    void setNumberInQueue(int count) { m_numberInQueue = count; }
    void setServerStatus(ResourceState status) { m_serverStatus = status; }

    void reset();
};

