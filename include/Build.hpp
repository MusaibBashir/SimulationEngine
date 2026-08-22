// ============================================================================
// Build.hpp  --  v5: the words you write when you build a model
// ============================================================================
// No new behaviour lives here. Every function is a one-line wrapper around a
// constructor that already existed. It exists because this:
//
//     m.setInterarrival(std::make_unique<Exponential>(1.0));
//     m.addStation("Teller", 1, QueueDiscipline::FIFO,
//                  std::make_unique<Exponential>(0.8));
//     sim.setTermination(std::make_unique<TimeLimit>(480.0));
//
// says the same thing as this:
//
//     m.arrivals(exponential(1.0));
//     m.station("Teller", 1, FIFO, exponential(0.8));
//     sim.stopAt(480.0);
//
// and the second one is about the queueing system rather than about C++ memory
// management. `std::make_unique<Exponential>` is noise: the reader already knows
// it is heap-allocated and owned, because everything here is.
//
// This is the cheapest kind of API design -- a naming layer over an interface
// that was already correct -- and usually the highest-value.

#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Common.hpp"
#include "Distribution.hpp"
#include "TerminationRule.hpp"

namespace des {

// --- distributions ----------------------------------------------------------
// Lower-case, because these read as verbs at the call site: "service is
// exponential with mean 0.8". Types stay capitalised.

inline std::unique_ptr<IDistribution> exponential(SimTime mean) {
    return std::make_unique<Exponential>(mean);
}
inline std::unique_ptr<IDistribution> constant(SimTime value) {
    return std::make_unique<Constant>(value);
}
inline std::unique_ptr<IDistribution> uniform(SimTime low, SimTime high) {
    return std::make_unique<Uniform>(low, high);
}
inline std::unique_ptr<IDistribution> triangular(SimTime low, SimTime mode, SimTime high) {
    return std::make_unique<Triangular>(low, mode, high);
}
// Takes an initializer_list so you can write fixedTimes({2, 4, 1, 3}) without
// spelling out std::vector<SimTime>.
inline std::unique_ptr<IDistribution> fixedTimes(std::initializer_list<SimTime> values,
                                                 bool repeat = true) {
    return std::make_unique<Deterministic>(std::vector<SimTime>(values), repeat);
}

// --- termination rules ------------------------------------------------------

inline std::unique_ptr<ITerminationRule> timeLimit(SimTime t) {
    return std::make_unique<TimeLimit>(t);
}
inline std::unique_ptr<ITerminationRule> entityLimit(int n) {
    return std::make_unique<EntityLimit>(n);
}
inline std::unique_ptr<ITerminationRule> whenDrained() {
    return std::make_unique<DrainedRule>();
}

// Variadic, so `anyOf(timeLimit(500), entityLimit(50), whenDrained())` works for
// any number of rules instead of three lines of push-backs.
inline std::unique_ptr<ITerminationRule> anyOf() { return std::make_unique<AnyOf>(); }

template <typename... Rest>
std::unique_ptr<ITerminationRule> anyOf(std::unique_ptr<ITerminationRule> first, Rest&&... rest) {
    auto tail = anyOf(std::forward<Rest>(rest)...);
    auto composite = std::make_unique<AnyOf>();
    composite->add(std::move(first));
    composite->add(std::move(tail));
    return composite;
}

// --- queue disciplines ------------------------------------------------------
// QueueDiscipline is still a scoped enum -- that has not changed and should not.
// These are named constants so call sites read `FIFO` rather than
// `QueueDiscipline::FIFO`, which is a lot of ceremony for a very common word.
constexpr QueueDiscipline FIFO     = QueueDiscipline::FIFO;
constexpr QueueDiscipline LIFO     = QueueDiscipline::LIFO;
constexpr QueueDiscipline PRIORITY = QueueDiscipline::Priority;
constexpr QueueDiscipline SPT      = QueueDiscipline::SPT;
constexpr QueueDiscipline EDD      = QueueDiscipline::EDD;
constexpr QueueDiscipline RANDOM   = QueueDiscipline::Random;

// --- attribute names --------------------------------------------------------
// The disciplines look up attributes BY STRING, so a typo silently means
// "attribute absent", which reads as 0.0 for every entity, which ties every
// comparison, which turns SPT into FIFO without a word of complaint.
//
// These constants make that a compile error instead. Use them.
namespace attr {
inline const std::string priority    = "priority";
inline const std::string serviceTime = "serviceTime";
inline const std::string dueDate     = "dueDate";
// Reserved by the engine -- assignOnArrival rejects these.
inline const std::string waitTime     = "waitTime";
inline const std::string waitHere     = "waitHere";
inline const std::string stationEntry = "stationEntry";
}  // namespace attr

}  // namespace des
