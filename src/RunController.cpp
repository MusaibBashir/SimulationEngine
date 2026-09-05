#include "RunController.hpp"

#include <iomanip>
#include "Compiler.hpp"
#include "ModelDocument.hpp"
#include <algorithm>
#include "Build.hpp"
#include "Model.hpp"
#include "ModelError.hpp"
#include "Resource.hpp"
#include "SimulationSystem.hpp"
#include "Station.hpp"
#include "TerminationRule.hpp"
#include "VariableStore.hpp"

namespace des {

std::string describe(RunState state) {
    switch (state) {
        case RunState::Ready:     return "Ready";
        case RunState::Running:   return "Running";
        case RunState::Paused:    return "Paused";
        case RunState::Finished:  return "Finished";
        case RunState::Cancelled: return "Cancelled";
        case RunState::Failed:    return "Failed";
    }
    return "Unknown";
}

RunSnapshot snapshotOf(const SimulationSystem& sim) {
    const Model& model = sim.model();
    const SimTime measured = sim.measuredTime();

    RunSnapshot s;
    s.now            = sim.now();
    s.measuredTime   = measured;
    s.arrived        = sim.statistics().numberArrived();
    s.exited         = sim.statistics().numberServed();
    s.numberInSystem = sim.numberInSystem();

    for (std::size_t i = 0; i < model.stationCount(); ++i) {
        const Station& st = model.stationAt(i);
        BlockSnapshot b;
        b.name        = st.name();
        b.queueLength = static_cast<double>(st.queue().length());
        b.served      = st.stats().numberServed();
        b.utilisation = st.stats().utilisation(measured, st.resource().capacity());
        s.blocks.push_back(std::move(b));
    }

    for (std::size_t i = 0; i < model.resourceCount(); ++i) {
        const Resource& r = model.resourceAt(i);
        s.resources.push_back(ResourceSnapshot{r.name(),
                                               static_cast<double>(r.unitsBusy()),
                                               static_cast<double>(r.capacity())});
    }

    for (const std::string& name : model.variables().names())
        s.variables.push_back(VariableSnapshot{name, model.variables().get(name)});

    return s;
}

namespace {

// The stopping rule a RunSetup describes, or nothing when it describes none.
//
// Null rather than an empty AnyOf, and the difference matters: initialise()
// asserts a rule exists, so an empty AnyOf would satisfy the assert and
// SILENTLY REPLACE whatever the builder set. Experiment's callers set their
// rule inside the builder, and this must leave it alone.
std::unique_ptr<ITerminationRule> ruleFrom(const RunSetup& s) {
    auto any = std::make_unique<AnyOf>();
    if (s.length)          any->add(timeLimit(*s.length));
    if (s.maxEntities)     any->add(entityLimit(*s.maxEntities));
    if (s.stopWhenDrained) any->add(whenDrained());
    if (any->empty()) return nullptr;
    return any;
}

}  // namespace

RunController::RunController(RunSetup setup, ModelBuilder build)
    : m_setup(std::move(setup)), m_build(std::move(build)) {}

RunController::~RunController() = default;

bool RunController::startRun() {
    if (m_replication >= m_setup.replications) {
        m_state = RunState::Finished;
        return false;
    }
    // A DIFFERENT SEED PER REPLICATION, DERIVED FROM ONE BASE SEED. Different,
    // or every run is the same run and the sample has no variance at all.
    // Derived, so the whole study is reproducible from a single number.
    const unsigned seed = m_setup.baseSeed + static_cast<unsigned>(m_replication);
    m_sim = std::make_unique<SimulationSystem>(seed);
    try {
        m_build(*m_sim);
        if (m_setup.separateStreams) m_sim->useSeparateStreams();
        if (m_mirrorHalf)            m_sim->useAntithetic();
        if (m_setup.warmUp > 0.0)          m_sim->setWarmUp(m_setup.warmUp);
        if (m_setup.observeInterval > 0.0) m_sim->setObservationInterval(m_setup.observeInterval);
        if (auto rule = ruleFrom(m_setup)) m_sim->setTermination(std::move(rule));
        m_sim->initialise();
    } catch (const ModelError& bad) {
        m_failure = bad.what();
        m_state   = RunState::Failed;
        m_sim.reset();
        return false;
    }
    return true;
}

void RunController::finishRun() {
    if (!m_sim) return;
    const Station& entry    = m_sim->model().stationAt(0);
    const SimTime  measured = m_sim->measuredTime();

    ReplicationResult res;
    res.seed                = m_setup.baseSeed + static_cast<unsigned>(m_replication);
    res.served              = m_sim->statistics().numberServed();
    res.averageWait         = m_sim->statistics().averageWaitingTime();
    res.averageTimeInSystem = m_sim->statistics().averageTimeInSystem();
    res.Lq                  = m_sim->statistics().timeAverageA(measured);
    res.L                   = m_sim->statistics().timeAverageB(measured);
    res.utilisation         = entry.stats().utilisation(measured, entry.resource().capacity());
    res.measuredTime        = measured;
    if (m_setup.observeInterval > 0.0) m_series.push_back(m_sim->observations());
    // HERE, while the system is still alive: report() is a view of a
    // SimulationSystem, and the system does not survive this function.
    m_lastReport.str(std::string());
    m_sim->report(m_lastReport);
    m_sim.reset();

    if (m_setup.antithetic && !m_mirrorHalf) {
        // The first half of a pair. Hold it and run the mirror.
        m_firstHalf  = res;
        m_mirrorHalf = true;
        return;
    }
    if (m_setup.antithetic) {
        // Average the mirrored run INTO this replication rather than adding it
        // as a second one. The pair is ONE observation: its halves are
        // negatively correlated, so treating them as independent would
        // understate the interval, which is the one direction that matters.
        res.averageWait         = 0.5 * (m_firstHalf.averageWait + res.averageWait);
        res.averageTimeInSystem = 0.5 * (m_firstHalf.averageTimeInSystem + res.averageTimeInSystem);
        res.Lq                  = 0.5 * (m_firstHalf.Lq + res.Lq);
        res.L                   = 0.5 * (m_firstHalf.L  + res.L);
        res.utilisation         = 0.5 * (m_firstHalf.utilisation + res.utilisation);
        res.served              = (m_firstHalf.served + res.served) / 2;
        res.seed                = m_firstHalf.seed;
        m_mirrorHalf = false;
    }
    m_results.push_back(res);
    ++m_replication;
}

std::size_t RunController::advance(std::size_t maxEvents) {
    if (m_state == RunState::Paused || m_state == RunState::Finished ||
        m_state == RunState::Cancelled || m_state == RunState::Failed) return 0;

    if (m_state == RunState::Ready) {
        m_state = RunState::Running;
        if (!startRun()) return 0;
    }

    std::size_t done = 0;
    while (done < maxEvents) {
        if (m_sim && m_sim->stepOnce()) { ++done; ++m_events; continue; }
        finishRun();
        if (!startRun()) break;
    }
    return done;
}

void RunController::pause() {
    if (m_state == RunState::Running) m_state = RunState::Paused;
}

void RunController::resume() {
    if (m_state == RunState::Paused) m_state = RunState::Running;
}

void RunController::cancel() {
    if (m_state != RunState::Finished) m_state = RunState::Cancelled;
}

void RunController::runToCompletion() {
    while (m_state == RunState::Ready || m_state == RunState::Running)
        advance(4096);
}

RunProgress RunController::progress() const {
    RunProgress p;
    p.state           = m_state;
    p.replications    = m_setup.replications;
    p.eventsProcessed = m_events;
    // 1-based, and it counts REPLICATIONS rather than runs: with antithetic
    // pairing one replication is two runs, and `replications(n)` means n pairs
    // to the caller who asked for it.
    //
    // CLAMPED, because m_replication has already been incremented past the
    // last one by the time the study finishes. Unclamped it printed
    // 'replication 4 of 3' -- found by running des run by hand, not by a
    // test, which is why there is now a test.
    p.replication = (m_state == RunState::Ready)
                        ? 0
                        : std::min(m_replication + 1, m_setup.replications);
    if (m_sim) {
        p.now = m_sim->now();
        if (const ITerminationRule* rule = m_sim->termination())
            p.fraction = rule->progress(*m_sim);
    }
    return p;
}

RunSnapshot RunController::snapshot() const {
    return m_sim ? snapshotOf(*m_sim) : RunSnapshot{};
}

std::unique_ptr<RunController>
RunController::fromDocument(const ModelDocument& doc, std::vector<Diagnostic>& out,
                            std::optional<SimTime> lengthOverride) {
    RunSetup setup = readRunSetup(doc, out);
    if (lengthOverride) setup.length = lengthOverride;

    // Compile once, HERE, so a bad cell is reported before a single event runs
    // rather than from inside the first replication, where a caller has no
    // diagnostics list to receive it.
    {
        Model probe;
        if (!compileInto(doc, probe, out)) return nullptr;
    }

    ModelDocument copy = doc;
    auto build = [copy](SimulationSystem& sim) {
        std::vector<Diagnostic> ignored;
        // Unreachable in practice: the same document compiled a moment ago.
        // A ModelError rather than a silent return because reaching it would
        // mean compileInto is not a function of its input, which is a
        // programmer error and not a user's.
        if (!compileInto(copy, sim.model(), ignored))
            throw ModelError("the document stopped compiling between replications");
    };
    return std::unique_ptr<RunController>(new RunController(setup, std::move(build)));
}

void RunController::report(std::ostream& os) const {
    if (m_results.size() == 1) {
        os << m_lastReport.str();
        return;
    }
    os << "\n=== " << m_results.size() << " replications ===\n";
    os << std::fixed << std::setprecision(4);
    const auto line = [&](const char* label, double ReplicationResult::* field) {
        std::vector<double> xs;
        for (const ReplicationResult& r : m_results) xs.push_back(r.*field);
        os << "  " << std::setw(24) << std::left << label << std::right
           << std::setw(12) << Summary::mean(xs)
           << "  +/- " << std::setw(10) << Summary::halfWidth95(xs) << "\n";
    };
    line("average wait",   &ReplicationResult::averageWait);
    line("time in system", &ReplicationResult::averageTimeInSystem);
    line("Lq",             &ReplicationResult::Lq);
    line("L",              &ReplicationResult::L);
    line("utilisation",    &ReplicationResult::utilisation);
    os << "===========================================================\n";
}

}  // namespace des
