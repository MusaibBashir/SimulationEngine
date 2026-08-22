// ============================================================================
// Model.hpp  --  v3 step 4: WHAT is simulated, separate from the machinery
// ============================================================================
// *** THIS IS THE ARCHITECTURE STEP. ***
// Everything before it was object-oriented programming: classes with invariants,
// a hierarchy or two. This is design: the engine (clock, FEL, event loop) no
// longer knows a single thing about tellers, restaurants or queue names. It
// takes a Model and runs it.
//
// The test of whether it worked: you can describe a completely different system
// -- a three-stage restaurant, a job shop -- without editing SimulationSystem.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Common.hpp"
#include "Station.hpp"
#include "Distribution.hpp"

class Model {
public:
    // v4.1: an attribute every arriving entity is given, drawn from its own
    // distribution. This is how a model says "customers have a priority",
    // "jobs have a due date", "parts have a processing time" -- and it is what
    // makes the Priority, SPT and EDD disciplines do anything at all. Without
    // it every entity reads 0.0 for those attributes and all three degenerate
    // to FIFO, silently.
    struct ArrivalAttribute {
        std::string name;
        std::unique_ptr<IDistribution> distribution;
    };

private:
    std::vector<std::unique_ptr<Station>> m_stations;   // the Model OWNS them
    std::vector<ArrivalAttribute> m_arrivalAttributes;
    std::unique_ptr<IDistribution> m_interarrival;
    Station* m_entry{nullptr};                          // where arrivals land

public:
    Model() = default;
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // --- building ---
    Station* addStation(const std::string& name, int capacity,
                        QueueDiscipline discipline,
                        std::unique_ptr<IDistribution> service);

    Station* addStation(const std::string& name, int capacity,
                        std::unique_ptr<IQueueRule> rule,
                        std::unique_ptr<IDistribution> service);

    // Route entities from one station to the next. Not calling connect() leaves
    // a station as an exit, which is the sensible default for a single queue.
    void connect(const std::string& from, const std::string& to);

    void setInterarrival(std::unique_ptr<IDistribution> d);

    // Give every arriving entity an attribute drawn from `d`.
    void assignOnArrival(const std::string& name, std::unique_ptr<IDistribution> d);
    const std::vector<ArrivalAttribute>& arrivalAttributes() const { return m_arrivalAttributes; }
    void setEntry(const std::string& name);

    // --- access ---
    Station* station(const std::string& name);
    const Station* station(const std::string& name) const;
    Station* entry() const { return m_entry; }
    IDistribution& interarrival() { return *m_interarrival; }
    std::size_t stationCount() const { return m_stations.size(); }
    Station& stationAt(std::size_t i) { return *m_stations[i]; }
    const Station& stationAt(std::size_t i) const { return *m_stations[i]; }

    void reset();

    // Check the model makes sense BEFORE the run rather than crashing during
    // it. A model with no entry, or no interarrival distribution, or a routing
    // loop that never exits, is a modelling mistake -- catch it here where the
    // message can name the problem.
    void validate() const;

    std::string describe() const;
};
