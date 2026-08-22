// ============================================================================
// Model.cpp
// ============================================================================

#include "Model.hpp"
#include "QueueRule.hpp"
#include <cassert>
#include <sstream>
#include <set>

namespace des {


Station* Model::addStation(const std::string& name, int capacity,
                           QueueDiscipline discipline,
                           std::unique_ptr<IDistribution> service) {
    return addStation(name, capacity, makeQueueRule(discipline), std::move(service));
}

Model& Model::arrivals(std::unique_ptr<IDistribution> d) {
    setInterarrival(std::move(d));
    return *this;
}

Model& Model::station(const std::string& name, int capacity,
                      QueueDiscipline discipline, std::unique_ptr<IDistribution> service) {
    addStation(name, capacity, discipline, std::move(service));
    return *this;
}

Model& Model::route(const std::string& from, const std::string& to) {
    connect(from, to);
    return *this;
}

Model& Model::entryAt(const std::string& name) {
    setEntry(name);
    return *this;
}

Model& Model::attribute(const std::string& name, std::unique_ptr<IDistribution> d) {
    assignOnArrival(name, std::move(d));
    return *this;
}

double Model::offeredLoad(const Station& s) const {
    if (!m_interarrival) return 0.0;
    const SimTime meanGap = m_interarrival->mean();
    if (meanGap <= 0.0) return 0.0;
    // Arrival rate at every station is the system arrival rate: this engine has
    // no branching, so each entity visits each station on its route exactly
    // once. Add probabilistic routing later and this needs visit ratios.
    const double lambda = 1.0 / meanGap;
    SimTime meanService = 0.0;
    if (s.usesServiceAttribute()) {
        // The service time rides on the entity, so ask the distribution that
        // stamps it at arrival. Without this lookup, job-shop models (the ones
        // most likely to be accidentally unstable) would skip the check
        // entirely -- which is precisely backwards.
        for (const auto& a : m_arrivalAttributes)
            if (a.name == s.serviceAttributeName()) meanService = a.distribution->mean();
    } else {
        meanService = s.serviceDistribution().mean();
    }
    if (meanService <= 0.0) return 0.0;
    return lambda * meanService / s.resource().capacity();
}

Station* Model::addStation(const std::string& name, int capacity,
                           std::unique_ptr<IQueueRule> rule,
                           std::unique_ptr<IDistribution> service) {
    if (station(name) != nullptr)
        throw ModelError("addStation: a station named '" + name + "' already exists");
    if (capacity < 1)
        throw ModelError("addStation: '" + name + "' needs capacity >= 1");
    auto s = std::make_unique<Station>(name, capacity, std::move(rule), std::move(service));
    Station* raw = s.get();
    m_stations.push_back(std::move(s));
    if (m_entry == nullptr) m_entry = raw;   // first station added is the default entry
    return raw;
}

void Model::connect(const std::string& from, const std::string& to) {
    Station* f = station(from);
    Station* t = station(to);
    if (f == nullptr) throw ModelError("connect: no station named '" + from + "'");
    if (t == nullptr) throw ModelError("connect: no station named '" + to + "'");
    if (f == t)       throw ModelError("connect: '" + from + "' cannot route to itself");
    f->setNext(t);
}

void Model::setInterarrival(std::unique_ptr<IDistribution> d) {
    assert(d != nullptr);
    m_interarrival = std::move(d);
}

void Model::assignOnArrival(const std::string& name, std::unique_ptr<IDistribution> d) {
    if (d == nullptr) throw ModelError("assignOnArrival: null distribution for '" + name + "'");
    if (name == "waitTime" || name == "stationEntry" || name == "waitHere")
        throw ModelError("assignOnArrival: '" + name + "' is reserved by the engine");
    m_arrivalAttributes.push_back(ArrivalAttribute{name, std::move(d)});
}

void Model::setEntry(const std::string& name) {
    Station* s = station(name);
    if (s == nullptr) throw ModelError("setEntry: no station named '" + name + "'");
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
    if (m_stations.empty())        throw ModelError("model has no stations");
    if (m_interarrival == nullptr) throw ModelError("model has no arrival distribution -- call arrivals()");
    if (m_entry == nullptr)        throw ModelError("model has no entry station -- call entryAt()");

    // Walk the route from the entry and make sure it terminates. A cycle would
    // send entities round forever, the run would never drain, and the symptom
    // would be a simulation that simply does not stop -- with no clue why.
    // Checking it here costs nothing and turns a hang into a message.
    std::set<const Station*> seen;
    const Station* s = m_entry;
    while (s != nullptr) {
        if (!seen.insert(s).second)
            throw ModelError("routing loop through station '" + s->name() +
                             "': entities would never leave the system");
        s = s->next();
    }

    // *** THE STABILITY CHECK. ***
    // Every station on the route must be able to keep up with the work arriving
    // at it. This is the check the examples used to tell you to do by hand, and
    // the one people skip. rho >= 1 is not a warning, it is a broken model: the
    // queue grows for as long as you run it, so "average wait" is a function of
    // run length rather than a property of the system.
    for (const auto& st : m_stations) {
        const double rho = offeredLoad(*st);
        if (rho >= 1.0) {
            std::ostringstream os;
            os << "station '" << st->name() << "' is unstable: offered load rho = "
               << rho << " (>= 1). Work arrives faster than "
               << st->resource().capacity() << " server(s) can do it, so the queue "
                  "grows without bound and every average is meaningless. "
                  "Add capacity, speed up service, or slow arrivals.";
            throw ModelError(os.str());
        }
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

}  // namespace des
