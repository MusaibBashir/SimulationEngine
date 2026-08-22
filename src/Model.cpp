// ============================================================================
// Model.cpp
// ============================================================================

#include "Model.hpp"
#include "QueueRule.hpp"
#include <cassert>
#include <sstream>
#include <set>

Station* Model::addStation(const std::string& name, int capacity,
                           QueueDiscipline discipline,
                           std::unique_ptr<IDistribution> service) {
    return addStation(name, capacity, makeQueueRule(discipline), std::move(service));
}

Station* Model::addStation(const std::string& name, int capacity,
                           std::unique_ptr<IQueueRule> rule,
                           std::unique_ptr<IDistribution> service) {
    assert(station(name) == nullptr && "duplicate station name");
    auto s = std::make_unique<Station>(name, capacity, std::move(rule), std::move(service));
    Station* raw = s.get();
    m_stations.push_back(std::move(s));
    if (m_entry == nullptr) m_entry = raw;   // first station added is the default entry
    return raw;
}

void Model::connect(const std::string& from, const std::string& to) {
    Station* f = station(from);
    Station* t = station(to);
    assert(f != nullptr && "connect(): unknown source station");
    assert(t != nullptr && "connect(): unknown destination station");
    assert(f != t && "connect(): a station cannot route to itself");
    f->setNext(t);
}

void Model::setInterarrival(std::unique_ptr<IDistribution> d) {
    assert(d != nullptr);
    m_interarrival = std::move(d);
}

void Model::assignOnArrival(const std::string& name, std::unique_ptr<IDistribution> d) {
    assert(d != nullptr);
    assert(name != "waitTime" && name != "stationEntry" && name != "waitHere" &&
           "that attribute name is reserved by the engine");
    m_arrivalAttributes.push_back(ArrivalAttribute{name, std::move(d)});
}

void Model::setEntry(const std::string& name) {
    Station* s = station(name);
    assert(s != nullptr && "setEntry(): unknown station");
    m_entry = s;
}

Station* Model::station(const std::string& name) {
    for (auto& s : m_stations) {
        if (s->name() == name) return s.get();
    }
    return nullptr;
}

const Station* Model::station(const std::string& name) const {
    for (const auto& s : m_stations) {
        if (s->name() == name) return s.get();
    }
    return nullptr;
}

void Model::reset() {
    for (auto& s : m_stations) s->reset();
    if (m_interarrival) m_interarrival->reset();
    for (auto& a : m_arrivalAttributes) a.distribution->reset();
}

void Model::validate() const {
    assert(!m_stations.empty() && "model has no stations");
    assert(m_entry != nullptr && "model has no entry station -- call setEntry()");
    assert(m_interarrival != nullptr && "model has no interarrival distribution");

    // Walk the route from the entry and make sure it terminates. A cycle would
    // send entities round forever, the run would never drain, and the symptom
    // would be a simulation that simply does not stop -- with no clue why.
    // Checking it here costs nothing and turns a hang into a message.
    std::set<const Station*> seen;
    const Station* s = m_entry;
    while (s != nullptr) {
        const bool fresh = seen.insert(s).second;
        assert(fresh && "routing loop: entities would never leave the system");
        (void)fresh;
        s = s->next();
    }
}

std::string Model::describe() const {
    std::ostringstream os;
    os << "arrivals ~ " << (m_interarrival ? m_interarrival->describe() : "<none>") << "\n";
    for (const auto& s : m_stations) os << "  " << s->describe() << "\n";
    for (const auto& a : m_arrivalAttributes)
        os << "  attribute " << a.name << " ~ " << a.distribution->describe() << "\n";
    os << "  entry: " << (m_entry ? m_entry->name() : "<none>");
    return os.str();
}
