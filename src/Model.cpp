// ============================================================================
// Model.cpp
// ============================================================================

#include "Model.hpp"
#include "QueueRule.hpp"
#include <cassert>
#include <set>
#include <sstream>

namespace des {

// ------------------------------------------------------------- building --

void Model::requireUnique(const std::string& name) const {
    if (node(name) != nullptr)
        throw ModelError("a block named '" + name + "' already exists");
}

INode* Model::add(std::unique_ptr<INode> n) {
    INode* raw = n.get();
    m_nodes.push_back(std::move(n));
    if (m_entry == nullptr) m_entry = raw;   // first block added is the default entry
    return raw;
}

Model& Model::arrivals(std::unique_ptr<IDistribution> d) {
    if (!d) throw ModelError("arrivals: null distribution");
    m_interarrival = std::move(d);
    return *this;
}

Model& Model::attribute(const std::string& name, std::unique_ptr<IDistribution> d) {
    if (!d) throw ModelError("attribute '" + name + "': null distribution");
    if (name == "waitTime" || name == "stationEntry" || name == "waitHere")
        throw ModelError("attribute: '" + name + "' is reserved by the engine");
    m_arrivalAttributes.push_back(ArrivalAttribute{name, std::move(d)});
    return *this;
}

Model& Model::resource(const std::string& name, int capacity) {
    if (resourceNamed(name) != nullptr)
        throw ModelError("a resource named '" + name + "' already exists");
    if (capacity < 1) throw ModelError("resource '" + name + "' needs capacity >= 1");
    m_resources.push_back(std::make_unique<Resource>(name, capacity));
    return *this;
}

Resource* Model::resourceNamed(const std::string& name) {
    for (auto& r : m_resources) if (r->name() == name) return r.get();
    return nullptr;
}
const Resource* Model::resourceNamed(const std::string& name) const {
    for (const auto& r : m_resources) if (r->name() == name) return r.get();
    return nullptr;
}

Model& Model::station(const std::string& name, int capacity,
                      QueueDiscipline discipline, std::unique_ptr<IDistribution> service) {
    // A private resource named after the block. Every model written before v7
    // takes this path and behaves exactly as it did.
    requireUnique(name);
    if (capacity < 1) throw ModelError("process '" + name + "' needs capacity >= 1");
    const std::string resName = name;
    if (resourceNamed(resName) == nullptr) resource(resName, capacity);
    return stationUsing(name, resName, discipline, std::move(service), 1);
}

Model& Model::stationUsing(const std::string& name, const std::string& resourceName,
                           QueueDiscipline discipline, std::unique_ptr<IDistribution> service,
                           int units) {
    requireUnique(name);
    if (!service) throw ModelError("process '" + name + "': null service distribution");
    Resource* r = resourceNamed(resourceName);
    if (r == nullptr)
        throw ModelError("process '" + name + "': no resource named '" + resourceName +
                         "' -- declare it with resource() first");
    auto s = std::make_unique<Station>(name, r, units, makeQueueRule(discipline), std::move(service));
    Station* raw = s.get();
    add(std::move(s));
    m_processes.push_back(raw);
    return *this;
}

Model& Model::balkAt(const std::string& processName, std::size_t queueLength,
                     const std::string& balkTo) {
    INode* target = nullptr;
    if (!balkTo.empty()) {
        target = node(balkTo);
        if (!target) throw ModelError("balkAt: no block named '" + balkTo + "'");
    }
    nodeAs<Station>(processName).setBalking(queueLength, target);
    return *this;
}

Model& Model::renegeAfter(const std::string& processName,
                          std::unique_ptr<IDistribution> patience,
                          const std::string& renegeTo) {
    INode* target = nullptr;
    if (!renegeTo.empty()) {
        target = node(renegeTo);
        if (!target) throw ModelError("renegeAfter: no block named '" + renegeTo + "'");
    }
    nodeAs<Station>(processName).setReneging(std::move(patience), target);
    return *this;
}

Model& Model::decideNWayByChance(const std::string& name) {
    requireUnique(name);
    add(std::make_unique<DecideNode>(name, true));
    return *this;
}

Model& Model::decideNWayByCondition(const std::string& name) {
    requireUnique(name);
    add(std::make_unique<DecideNode>(name, false));
    return *this;
}

Model& Model::branch(const std::string& decideName, double probability, const std::string& to) {
    INode* target = node(to);
    if (!target) throw ModelError("branch: no block named '" + to + "'");
    nodeAs<DecideNode>(decideName).addBranch(probability, target);
    return *this;
}

Model& Model::branch(const std::string& decideName, DecideNode::Condition condition,
                     const std::string& to) {
    INode* target = node(to);
    if (!target) throw ModelError("branch: no block named '" + to + "'");
    nodeAs<DecideNode>(decideName).addBranch(std::move(condition), target);
    return *this;
}

Model& Model::delay(const std::string& name, std::unique_ptr<IDistribution> duration) {
    requireUnique(name);
    add(std::make_unique<DelayNode>(name, std::move(duration)));
    return *this;
}

Model& Model::assign(const std::string& name, const std::string& attributeName,
                     std::unique_ptr<IDistribution> value) {
    // Calling assign() twice with the same block name adds a second attribute to
    // the SAME block rather than erroring -- which is how people expect an
    // Assign block with several fields to be written.
    if (INode* existing = node(name)) {
        auto* a = dynamic_cast<AssignNode*>(existing);
        if (!a) throw ModelError("block '" + name + "' exists and is not an Assign");
        a->set(attributeName, std::move(value));
        return *this;
    }
    auto a = std::make_unique<AssignNode>(name);
    a->set(attributeName, std::move(value));
    add(std::move(a));
    return *this;
}

Model& Model::decideByChance(const std::string& name, double p) {
    requireUnique(name);
    add(std::make_unique<DecideNode>(name, p));
    return *this;
}

Model& Model::decideByCondition(const std::string& name, DecideNode::Condition c) {
    requireUnique(name);
    add(std::make_unique<DecideNode>(name, std::move(c)));
    return *this;
}

Model& Model::batch(const std::string& name, std::size_t size, bool permanent) {
    requireUnique(name);
    add(std::make_unique<BatchNode>(name, size, permanent));
    return *this;
}

Model& Model::separate(const std::string& name) {
    requireUnique(name);
    add(std::make_unique<SeparateNode>(name));
    return *this;
}

Model& Model::duplicate(const std::string& name, int copies) {
    requireUnique(name);
    add(std::make_unique<SeparateNode>(name, copies));
    return *this;
}

Model& Model::record(const std::string& name) {
    requireUnique(name);
    add(std::make_unique<RecordNode>(name));
    return *this;
}

Model& Model::recordAttribute(const std::string& name, const std::string& attributeName) {
    requireUnique(name);
    add(std::make_unique<RecordNode>(name, attributeName));
    return *this;
}

Model& Model::recordTimeInSystem(const std::string& name) {
    requireUnique(name);
    add(RecordNode::timeInSystem(name));
    return *this;
}

Model& Model::dispose(const std::string& name) {
    requireUnique(name);
    add(std::make_unique<DisposeNode>(name));
    return *this;
}

// -------------------------------------------------------------- wiring --

Model& Model::route(const std::string& from, const std::string& to) {
    INode* f = node(from);
    INode* t = node(to);
    if (!f) throw ModelError("route: no block named '" + from + "'");
    if (!t) throw ModelError("route: no block named '" + to + "'");
    if (f == t) throw ModelError("route: '" + from + "' cannot route to itself");
    f->setNext(t);
    return *this;
}

Model& Model::routeTrue(const std::string& decideName, const std::string& to) {
    INode* t = node(to);
    if (!t) throw ModelError("routeTrue: no block named '" + to + "'");
    nodeAs<DecideNode>(decideName).setTrueBranch(t);
    return *this;
}

Model& Model::entryAt(const std::string& name) {
    INode* n = node(name);
    if (!n) throw ModelError("entryAt: no block named '" + name + "'");
    m_entry = n;
    return *this;
}

// -------------------------------------------------------------- access --

INode* Model::node(const std::string& name) {
    for (auto& n : m_nodes) if (n->name() == name) return n.get();
    return nullptr;
}
const INode* Model::node(const std::string& name) const {
    for (const auto& n : m_nodes) if (n->name() == name) return n.get();
    return nullptr;
}
Station* Model::station(const std::string& name) {
    return dynamic_cast<Station*>(node(name));
}
const Station* Model::station(const std::string& name) const {
    return dynamic_cast<const Station*>(node(name));
}

void Model::reset() {
    // The resources are reset HERE and only here. They are shared, so leaving it
    // to the blocks would mean several blocks each resetting the same resource
    // -- harmless today, and exactly the sort of thing that becomes a bug the
    // moment reset() does more than zero a counter.
    for (auto& r : m_resources) r->reset();
    for (auto& n : m_nodes) n->reset();
    if (m_interarrival) m_interarrival->reset();
    for (auto& a : m_arrivalAttributes) a.distribution->reset();
}

// ------------------------------------------------------------ analysis --

Model::VisitRatios Model::visitRatios() const {
    // Walk the flowchart from the entry, carrying a weight that says how many
    // times an average arriving entity reaches each block.
    //
    //   Decide by chance p : true branch gets w*p, false branch w*(1-p)
    //   Decide by condition: unknown -- flagged, and the split is assumed even
    //   Batch of n         : n entities in, 1 out, so the outflow is w/n
    //   Duplicate x k      : 1 in, k+1 out
    //
    // That last pair is why this cannot be a simple graph walk with weight 1:
    // batching and duplication change the FLOW RATE, and a stability check that
    // ignored them would be wrong in the direction that matters.
    VisitRatios vr;
    if (!m_entry) return vr;

    struct Item { const INode* node; double weight; int depth; };
    std::vector<Item> stack{{m_entry, 1.0, 0}};

    while (!stack.empty()) {
        const Item it = stack.back();
        stack.pop_back();
        if (it.node == nullptr) continue;
        // Depth guard: a cycle is rejected by validate(), but visitRatios() is
        // also callable on a half-built model, and an infinite loop here would
        // be a far worse diagnostic than a slightly wrong number.
        if (it.depth > 512) { vr.exact = false; continue; }
        if (it.weight < 1e-9) continue;

        vr.visits[it.node] += it.weight;

        double out = it.weight;
        if (auto* b = dynamic_cast<const BatchNode*>(it.node))
            out = it.weight / static_cast<double>(b->size());
        if (auto* d = dynamic_cast<const DecideNode*>(it.node)) {
            // v7: N branches. Chance branches carry their own probability and
            // whatever is left over falls through to next(). Condition branches
            // cannot be weighted at all -- the split is an OUTPUT of the run --
            // so the ratios are split evenly and flagged inexact.
            const auto& bs = d->branches();
            double assigned = 0.0;
            if (d->isByChance()) {
                for (const auto& b : bs) {
                    stack.push_back({b.target, it.weight * b.probability, it.depth + 1});
                    assigned += b.probability;
                }
            } else {
                vr.exact = false;
                const double share = bs.empty() ? 0.0 : 1.0 / static_cast<double>(bs.size() + 1);
                for (const auto& b : bs) {
                    stack.push_back({b.target, it.weight * share, it.depth + 1});
                    assigned += share;
                }
            }
            stack.push_back({d->next(), it.weight * (1.0 - assigned), it.depth + 1});
            continue;
        }
        if (dynamic_cast<const DisposeNode*>(it.node)) continue;
        if (auto* s = dynamic_cast<const SeparateNode*>(it.node)) {
            // A duplicate multiplies the flow; a batch-split restores it. The
            // split factor is the batch size, which this node does not know, so
            // it is left at 1 and flagged.
            (void)s;
            vr.exact = false;
        }
        stack.push_back({it.node->next(), out, it.depth + 1});
    }
    return vr;
}

double Model::offeredLoad(const Station& s) const {
    if (!m_interarrival) return 0.0;
    const SimTime meanGap = m_interarrival->mean();
    if (meanGap <= 0.0) return 0.0;
    const double lambda = 1.0 / meanGap;

    SimTime meanService = 0.0;
    if (s.usesServiceAttribute()) {
        // The service time rides on the entity, so ask the distribution that
        // stamps it at arrival. Job shops -- the models most likely to be
        // accidentally unstable -- would otherwise skip the check entirely.
        for (const auto& a : m_arrivalAttributes)
            if (a.name == s.serviceAttributeName()) meanService = a.distribution->mean();
    } else {
        meanService = s.serviceDistribution().mean();
    }
    if (meanService <= 0.0) return 0.0;

    const VisitRatios vr = visitRatios();
    const auto it = vr.visits.find(&s);
    const double visits = (it == vr.visits.end()) ? 1.0 : it->second;

    return lambda * visits * meanService * s.unitsNeeded() / s.resource().capacity();
}

void Model::validate() const {
    if (m_nodes.empty())           throw ModelError("model has no blocks");
    if (m_interarrival == nullptr) throw ModelError("model has no arrival distribution -- call arrivals()");
    if (m_entry == nullptr)        throw ModelError("model has no entry block -- call entryAt()");

    // A routing loop means entities never leave, the run never drains, and the
    // symptom is a program that simply does not stop with no clue why. Follow
    // both branches of every Decide.
    std::set<const INode*> onPath;
    std::vector<const INode*> stack{m_entry};
    std::set<const INode*> seen;
    while (!stack.empty()) {
        const INode* n = stack.back();
        stack.pop_back();
        if (!n || !seen.insert(n).second) continue;
        onPath.insert(n);
        if (auto* d = dynamic_cast<const DecideNode*>(n)) {
            if (d->trueBranch()) stack.push_back(d->trueBranch());
        }
        if (n->next()) stack.push_back(n->next());
    }
    // Cycle detection: a proper DFS colouring, because the reachability walk
    // above cannot tell a diamond (fine) from a loop (fatal).
    std::set<const INode*> visiting, done;
    struct Rec {
        static void dfs(const INode* n, std::set<const INode*>& visiting,
                        std::set<const INode*>& done) {
            if (!n || done.count(n)) return;
            if (!visiting.insert(n).second)
                throw ModelError("routing loop through block '" + n->name() +
                                 "': entities would never leave the system");
            if (auto* d = dynamic_cast<const DecideNode*>(n)) dfs(d->trueBranch(), visiting, done);
            dfs(n->next(), visiting, done);
            visiting.erase(n);
            done.insert(n);
        }
    };
    Rec::dfs(m_entry, visiting, done);

    // The stability check: every process on the route must keep up with the work
    // reaching it. rho >= 1 is not a warning, it is a broken model -- the queue
    // grows for as long as you run, so "average wait" is a function of run
    // length rather than a property of the system.
    // v7: a SHARED resource must be judged on the TOTAL work reaching every
    // block that uses it. Two machines each at rho = 0.6 are fine on their own
    // and impossible if they share one operator -- checking them separately
    // would pass a model that cannot run.
    const VisitRatios vr = visitRatios();
    for (const auto& res : m_resources) {
        double total = 0.0;
        int users = 0;
        for (const Station* st : m_processes) {
            if (&st->resource() != res.get()) continue;
            if (vr.visits.find(st) == vr.visits.end()) continue;
            total += offeredLoad(*st);
            ++users;
        }
        if (users > 1 && total >= 1.0) {
            std::ostringstream os;
            os << "resource '" << res->name() << "' is oversubscribed: the "
               << users << " blocks sharing it need a combined offered load of "
               << total << " (>= 1). Each may look fine alone; together they "
                  "cannot keep up.";
            throw ModelError(os.str());
        }
    }

    for (const Station* st : m_processes) {
        if (vr.visits.find(st) == vr.visits.end()) continue;   // unreachable block
        const double rho = offeredLoad(*st);
        if (rho >= 1.0) {
            std::ostringstream os;
            os << "process '" << st->name() << "' is unstable: offered load rho = "
               << rho << " (>= 1). Work arrives faster than "
               << st->resource().capacity() << " server(s) can do it, so the queue "
                  "grows without bound and every average is meaningless. "
                  "Add capacity, speed up service, or slow arrivals.";
            if (!vr.exact)
                os << " (Flow rates are approximate here: a condition-based Decide "
                      "or a batch split means the true split is an output of the run.)";
            throw ModelError(os.str());
        }
    }
}

std::string Model::describe() const {
    std::ostringstream os;
    os << "arrivals ~ " << (m_interarrival ? m_interarrival->describe() : "<none>") << "\n";
    for (const auto& a : m_arrivalAttributes)
        os << "  attribute " << a.name << " ~ " << a.distribution->describe() << "\n";
    for (const auto& n : m_nodes) os << "  " << n->describe() << "\n";
    os << "  entry: " << (m_entry ? m_entry->name() : "<none>");
    return os.str();
}

}  // namespace des
