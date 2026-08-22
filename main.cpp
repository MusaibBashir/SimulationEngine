// ============================================================================
// main.cpp  --  v3 demonstrations and acceptance checks
// ============================================================================
// Five scenarios, each one checking something the previous version could not:
//   1. M/M/1        -- validated against Little's Law and closed-form theory
//   2. M/M/3        -- validated against Erlang-C
//   3. Deterministic -- hand-checkable, and writes a full trace file
//   4. Restaurant    -- a CHAIN of stations: host -> waiters -> cashier
//   5. Replication   -- same seed twice, identical output
// ============================================================================

#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>
#include "SimulationSystem.hpp"

namespace {

void heading(const char* text) {
    std::cout << "\n############ " << text << " ############\n";
}

// Build a plain single-queue model. Returns nothing -- it configures `sim`.
void buildSingleServer(SimulationSystem& sim,
                       int capacity,
                       std::unique_ptr<IDistribution> interarrival,
                       std::unique_ptr<IDistribution> service) {
    Model& m = sim.model();
    m.setInterarrival(std::move(interarrival));
    m.addStation("Server", capacity, QueueDiscipline::FIFO, std::move(service));
    m.setEntry("Server");
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // ---------------------------------------------------------------------
    // 1. M/M/1.  lambda = 1.0, mu = 1.25, rho = 0.8
    //    Theory: Wq = rho/(mu-lambda) = 3.2000, Lq = 3.2000, util = 0.8000
    // ---------------------------------------------------------------------
    heading("1. M/M/1");
    {
        SimulationSystem sim(12345u);
        buildSingleServer(sim, 1,
                          std::make_unique<Exponential>(1.0),
                          std::make_unique<Exponential>(0.8));
        sim.setTermination(std::make_unique<TimeLimit>(20000.0));
        sim.initialise();
        sim.run();
        sim.report();

        // LITTLE'S LAW: Lq = lambda_eff * Wq. Holds for ANY queueing system,
        // which makes it a far better check than comparing to M/M/1 theory.
        const Station& st  = sim.model().stationAt(0);
        const SimTime  T   = sim.clock().now();
        const double lam   = st.stats().numberArrived() / T;
        const double Lq    = st.stats().timeAverageQueueLength(T);
        const double Wq    = st.stats().averageWaitingTime();
        std::cout << "Little's Law   : Lq " << Lq << " vs lambda*Wq " << lam * Wq
                  << "  (rel err " << (Lq > 0 ? std::fabs(Lq - lam * Wq) / Lq : 0.0) << ")\n";
        std::cout << "theory         : Wq 3.2000  Lq 3.2000  util 0.8000\n";
    }

    // ---------------------------------------------------------------------
    // 2. M/M/3.  lambda = 1.0, mu = 0.5, c = 3, a = 2, rho = 0.6667
    //    Erlang-C: P(wait) = 0.4444, Wq = 0.8889, Lq = 0.8889
    // ---------------------------------------------------------------------
    heading("2. M/M/3 -- multiple servers at one station");
    {
        SimulationSystem sim(2024u);
        buildSingleServer(sim, 3,
                          std::make_unique<Exponential>(1.0),
                          std::make_unique<Exponential>(2.0));
        sim.setTermination(std::make_unique<TimeLimit>(100000.0));
        sim.initialise();
        sim.run();
        sim.report();
        std::cout << "theory (Erlang-C): Wq 0.8889  Lq 0.8889  util 0.6667\n";
    }

    // ---------------------------------------------------------------------
    // 3. DETERMINISTIC -- the run you can check by hand.
    //
    //    interarrival : 2, 4, 1, 3, 5, ...   (first arrival is at t = 0)
    //    service      : 3, 2, 4, 1, 2, ...
    //
    //    Worked by hand, single server, FIFO:
    //      t=0  C1 arrives, free       -> service 3, departs t=3
    //      t=2  C2 arrives, busy       -> queues
    //      t=3  C1 exits (wait 0)      -> C2 starts, waited 1, service 2
    //      t=5  C2 exits (wait 1)
    //      t=6  C3 arrives, free       -> service 4, departs t=10
    //      t=7  C4 arrives, busy       -> queues
    //      t=10 C3 exits (wait 0)      -> C4 starts, waited 3, service 1
    //           C5 also arrives at t=10 -- the FEL TIE-BREAK decides the order,
    //           and the departure wins because it was scheduled first
    //      t=11 C4 exits (wait 3)      -> C5 starts, waited 1, service 2
    //      t=13 C5 exits (wait 1)
    //    => 5 served, waits 0,1,0,3,1, average 1.0000
    // ---------------------------------------------------------------------
    heading("3. Deterministic -- hand-checkable, with a trace file");
    {
        SimulationSystem sim(1u);
        buildSingleServer(sim, 1,
                          std::make_unique<Deterministic>(std::vector<SimTime>{2, 4, 1, 3, 5}),
                          std::make_unique<Deterministic>(std::vector<SimTime>{3, 2, 4, 1, 2}));
        sim.setTermination(std::make_unique<EntityLimit>(5));
        sim.enableTrace("trace_deterministic.md", TraceLevel::Events);
        sim.initialise();
        sim.run();
        sim.report();
        std::cout << "hand-worked    : 5 served, waits 0,1,0,3,1, average 1.0000\n";
        std::cout << "trace written  : trace_deterministic.md\n";
    }

    // ---------------------------------------------------------------------
    // 4. A RESTAURANT -- a chain of stations, which v2 could not express.
    //      Host (1)  ->  Waiters (3)  ->  Cashier (1)
    //    Each has its own capacity, its own queue discipline and its own
    //    service distribution. The engine knows none of that.
    // ---------------------------------------------------------------------
    heading("4. Restaurant -- Host -> Waiters(3) -> Cashier");
    {
        SimulationSystem sim(7u);
        Model& m = sim.model();
        m.setInterarrival(std::make_unique<Exponential>(15.0));  // a party every ~15 min
        // Stability check BEFORE running: waiters mean ~38.3 min over 3 servers
        // is 12.8 min of work per party, against 15 min between parties, so
        // rho ~ 0.85. Push arrivals to every 4 min and rho ~ 3.2 -- the queue
        // grows without bound and every average is meaningless. Always do this
        // arithmetic first; the simulator will happily model an unstable system
        // and report confident numbers about it.
        m.addStation("Host",    1, QueueDiscipline::FIFO,     std::make_unique<Exponential>(2.0));
        m.addStation("Waiters", 3, QueueDiscipline::FIFO,     std::make_unique<Triangular>(20.0, 35.0, 60.0));
        m.addStation("Cashier", 1, QueueDiscipline::Priority, std::make_unique<Uniform>(1.0, 4.0));
        m.connect("Host", "Waiters");
        m.connect("Waiters", "Cashier");
        m.setEntry("Host");

        // Composite stopping rule -- the v1 hardcoded OR, now an object.
        auto stop = std::make_unique<AnyOf>();
        stop->add(std::make_unique<TimeLimit>(6000.0));     // a long service period
        stop->add(std::make_unique<EntityLimit>(100000));
        sim.setTermination(std::move(stop));

        sim.enableTrace("trace_restaurant.md", TraceLevel::Events);
        sim.initialise();
        sim.run();
        sim.report();
        std::cout << "trace written  : trace_restaurant.md\n";
    }

    // ---------------------------------------------------------------------
    // 5. Replication reproducibility. Same seed, same process, twice.
    // ---------------------------------------------------------------------
    heading("5. Replication reproducibility");
    {
        SimulationSystem sim(12345u);
        buildSingleServer(sim, 1,
                          std::make_unique<Exponential>(1.0),
                          std::make_unique<Exponential>(0.8));
        sim.setTermination(std::make_unique<TimeLimit>(2000.0));

        sim.initialise(); sim.run();
        const int    n1 = sim.statistics().numberServed();
        const double w1 = sim.statistics().averageWaitingTime();

        sim.initialise(); sim.run();
        const int    n2 = sim.statistics().numberServed();
        const double w2 = sim.statistics().averageWaitingTime();

        std::cout << "rep 1: served " << n1 << ", avg wait " << w1 << "\n";
        std::cout << "rep 2: served " << n2 << ", avg wait " << w2 << "\n";
        std::cout << "identical: " << std::boolalpha << (n1 == n2 && w1 == w2) << "\n";
    }

    std::cout << "\n";
    return 0;
}
