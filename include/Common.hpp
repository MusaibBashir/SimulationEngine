// ============================================================================
// Common.hpp  --  the shared vocabulary. No classes live here.
// ============================================================================
// Everything in the project speaks these types. Written once, here, so that
// nobody re-declares them and nobody disagrees about what "time" means.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
//     One line, the pragma form (not the #ifndef / #define / #endif trio).
//
// [2] Two type aliases. Use the `using X = Y;` form, not `typedef`.
//
//     [2a] SimTime  ->  double
//          Every variable that holds a point in time or a duration uses this,
//          never a bare double. Reason: if you later switch to integer clock
//          ticks, you edit ONE line in this file instead of forty lines
//          everywhere else. This is the cheapest system-design win available.
//
//     [2b] EntityId  ->  int
//          Same reasoning. Also makes function signatures self-documenting:
//          `f(EntityId, SimTime)` reads better than `f(int, double)`.
//
// [3] enum class EventType
//     Values: Arrival, StartService, Departure, EndSimulation
//     MUST be `enum class`, not plain `enum`. A scoped enum will not silently
//     convert itself to an int, so `if (type == 0)` becomes a compile error
//     instead of a bug. You write EventType::Arrival at every use site.
//
// [4] enum class QueueDiscipline
//     Values: FIFO, LIFO, Priority, Random, SPT, EDD
//     Straight off the theory table -- "the rule deciding who is served next".
//     v1 only STORES which one is selected. v2 makes EntityQueue::pop()
//     actually obey it.
//
// [5] enum class ResourceState
//     Values: Idle, Busy
//     Used by Resource and by SystemState.
//
// ---------------- WHAT DOES **NOT** GO IN HERE ----------------
// No #include of any of your own headers. No functions. No classes.
// The moment this file includes something of yours, it stops being the
// bottom of the dependency graph and starts being a circular-include factory.

#pragma once

namespace des {


using SimTime=double;
using EntityId=int;

// v4 adds WarmUpEnd and Observe. The run() switch is now six cases -- see the
// note at the bottom of SimulationSystem::run() for why it is still a switch.
enum class EventType {Arrival, StartService, Departure, EndSimulation, WarmUpEnd, Observe};
enum class QueueDiscipline {FIFO, LIFO, Priority, Random, SPT, EDD};
enum class ResourceState {Idle, Busy};

}  // namespace des
