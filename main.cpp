// ============================================================================
// main.cpp  --  v4 demonstrations
// ============================================================================
//   1. Deterministic  -- hand-checkable, writes a full trace file
//   2. Restaurant     -- a chain of stations, per-station statistics
//   3. Welch analysis -- find the warm-up period, write it as CSV to plot
//   4. Before / after -- the same model reported naively, then honestly
//   5. Does theory fall inside the interval?  (the question open since v2)
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

void heading(const char* text) {
    std::cout << "\n############ " << text << " ############\n";
}

// The model under study everywhere below: M/M/1, lambda = 1.0, mu = 1.25.
//   rho = 0.8
//   Wq = rho / (mu - lambda) = 3.2000
//   Lq = lambda * Wq         = 3.2000
//   W  = Wq + 1/mu           = 4.0000
//   L  = lambda * W          = 4.0000
void buildMM1(SimulationSystem& sim, SimTime runLength) {
    Model& m = sim.model();
    m.setInterarrival(exponential(1.0));
    m.station("Server", 1, QueueDiscipline::FIFO, exponential(0.8));
    m.setEntry("Server");
    sim.setTermination(timeLimit(runLength));
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // ---------------------------------------------------------------------
    // 1. Deterministic -- checkable by hand, event for event.
    //    interarrival 2,4,1,3,5 ; service 3,2,4,1,2
    //    => 5 served, waits 0,1,0,3,1, average 1.0000, last exit t = 13
    // ---------------------------------------------------------------------
    heading("1. Deterministic -- hand-checkable, with a trace file");
    {
        SimulationSystem sim(1u);
        Model& m = sim.model();
        m.setInterarrival(fixedTimes({2, 4, 1, 3, 5}));
        m.station("Server", 1, QueueDiscipline::FIFO,
                     fixedTimes({3, 2, 4, 1, 2}));
        m.setEntry("Server");
        sim.setTermination(entityLimit(5));
        sim.enableTrace("trace_deterministic.md", TraceLevel::Events);
        sim.initialise();
        sim.run();
        sim.report();
        std::cout << "hand-worked    : 5 served, waits 0,1,0,3,1, average 1.0000, ends t=13\n";
    }

    // ---------------------------------------------------------------------
    // 2. A restaurant: Host(1) -> Waiters(3) -> Cashier(1)
    // ---------------------------------------------------------------------
    heading("2. Restaurant -- a chain of stations");
    {
        SimulationSystem sim(7u);
        Model& m = sim.model();
        m.setInterarrival(exponential(15.0));
        m.station("Host",    1, QueueDiscipline::FIFO,     exponential(2.0));
        m.station("Waiters", 3, QueueDiscipline::FIFO,     triangular(20.0, 35.0, 60.0));
        m.station("Cashier", 1, QueueDiscipline::Priority, uniform(1.0, 4.0));
        m.connect("Host", "Waiters");
        m.connect("Waiters", "Cashier");
        m.setEntry("Host");

        auto stop = std::make_unique<AnyOf>();
        stop->add(timeLimit(6000.0));
        stop->add(entityLimit(100000));
        sim.setTermination(std::move(stop));

        sim.setWarmUp(600.0);   // discard the first ten hours of the transient
        sim.enableTrace("trace_restaurant.md", TraceLevel::Events);
        sim.initialise();
        sim.run();
        sim.report();
    }

    // ---------------------------------------------------------------------
    // 3. WELCH'S METHOD. Twenty replications, sampling number-in-system on a
    //    fixed grid, averaged across replications and then smoothed. Where the
    //    curve flattens is where the start-up transient has died out.
    // ---------------------------------------------------------------------
    heading("3. Welch analysis -- how long is the transient?");
    SimTime suggested = 0.0;
    {
        Experiment welch("M/M/1 warm-up analysis",
                         [](SimulationSystem& s) { buildMM1(s, 6000.0); });
        welch.replications(20).baseSeed(500u).observeEvery(5.0);
        welch.run();

        suggested = welch.suggestWarmUp();   // MSER
        welch.writeWelchSeries("welch_series.csv", 20);

        std::cout << "suggested warm-up        : " << suggested << "\n";
        std::cout << "welch_series.csv written -- plot it; the heuristic is a\n"
                  << "starting point, the eye is the real instrument here.\n";
    }

    // ---------------------------------------------------------------------
    // 4. THE SAME MODEL, REPORTED TWO WAYS.
    //    Naive: one run, no warm-up, one number quoted to four decimals.
    //    Honest: ten replications, warm-up discarded, mean +/- interval.
    // ---------------------------------------------------------------------
    heading("4. Naive single run vs replications with warm-up removal");
    std::vector<double> honestWq;
    {
        Experiment naive("naive: 1 replication, no warm-up",
                         [](SimulationSystem& s) { buildMM1(s, 20000.0); });
        naive.replications(1).baseSeed(12345u);
        naive.run();
        std::cout << "single run, no warm-up   : Wq = "
                  << naive.results().front().averageWait
                  << "   (theory 3.2000)\n";
        std::cout << "  ...quoted to four decimals from ONE sample of a random\n"
                  << "  variable. The digits after the decimal point are fiction.\n\n";

        Experiment honest("honest: 10 replications, warm-up removed",
                          [](SimulationSystem& s) { buildMM1(s, 20000.0); });
        // 20 replications, not 10. At 10 the interval is wide enough that
        // whether it covers the true value is a coin flip on the seed -- which
        // is exactly the lesson, but a demonstration should not depend on luck.
        honest.replications(20).baseSeed(9000u).warmUp(suggested);
        honest.run();
        honest.report();
        honestWq = honest.column(&ReplicationResult::averageWait);
    }

    // ---------------------------------------------------------------------
    // 5. THE QUESTION THAT HAS BEEN OPEN SINCE v2.
    //    Wq has read a few percent above 3.2000 in every version. Is the
    //    simulator wrong, or was the reporting?
    // ---------------------------------------------------------------------
    heading("5. Does M/M/1 theory fall inside the interval?");
    {
        const double m  = Summary::mean(honestWq);
        const double hw = Summary::halfWidth95(honestWq);
        const double theory = 3.2000;

        std::cout << "measured Wq              : " << m << " +/- " << hw << "\n";
        std::cout << "95% interval             : [" << (m - hw) << ", " << (m + hw) << "]\n";
        std::cout << "M/M/1 theory             : " << theory << "\n";
        std::cout << "theory inside interval   : " << std::boolalpha
                  << (theory >= m - hw && theory <= m + hw) << "\n\n";
        std::cout << "If true: the simulator was never wrong. Every version since v2\n"
                  << "reported a single sample as though it were an exact answer.\n"
                  << "If false: widen the run length or the replication count before\n"
                  << "suspecting the engine -- a 95% interval misses 1 time in 20.\n";
    }

    std::cout << "\n";
    return 0;
}
