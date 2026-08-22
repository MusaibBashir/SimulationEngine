// ============================================================================
// 09 — Using the simulator to actually decide something.
// ============================================================================
// This is what the whole engine is for. Everything up to here has measured a
// system; this example CHOOSES one.
//
// A call centre. Agents cost money. Customers waiting cost goodwill. How many
// agents should we staff?
//
// The pattern generalises to almost any coursework question: sweep a decision
// variable, get a confidence interval at each setting, combine with a cost
// model, and pick -- while being honest about which differences are real.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

const double COST_PER_AGENT_HOUR   = 25.0;   // wages
const double COST_PER_WAITING_HOUR = 60.0;   // what a queued customer costs us
const SimTime RUN_MINUTES          = 12000.0;

struct Option {
    int    agents;
    double wq, wqHalf;      // mean wait and its 95% half-width, minutes
    double lq;              // mean number waiting
    double rho;
    double costPerHour;
};

Option evaluate(int agents) {
    Experiment e("staffing", [agents](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));       // a call/min
        m.addStation("Agents", agents, QueueDiscipline::FIFO,
                     exponential(3.5));            // 3.5 min/call
        m.setEntry("Agents");
        s.setTermination(timeLimit(RUN_MINUTES));
    });
    e.replications(10).baseSeed(11000u).warmUp(2000.0);
    e.run();

    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    const std::vector<double> lq = e.column(&ReplicationResult::Lq);
    const std::vector<double> ut = e.column(&ReplicationResult::utilisation);

    Option o;
    o.agents = agents;
    o.wq     = Summary::mean(wq);
    o.wqHalf = Summary::halfWidth95(wq);
    o.lq     = Summary::mean(lq);
    o.rho    = Summary::mean(ut);
    // Wages plus the cost of however many people are sitting in the queue.
    o.costPerHour = agents * COST_PER_AGENT_HOUR + o.lq * COST_PER_WAITING_HOUR;
    return o;
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Call centre: 1 call/min arriving, 3.5 min per call.\n";
    std::cout << "Work arriving = 3.5 agent-minutes per minute, so 4 agents is\n";
    std::cout << "the bare minimum to keep up (rho = 3.5/4 = 0.875) and 3 would\n";
    std::cout << "have rho > 1 -- an unbounded queue. We start at 4.\n\n";

    std::cout << "  agents   mean wait (min)      Lq     rho    cost/hr\n";
    std::cout << "  ---------------------------------------------------\n";

    std::vector<Option> options;
    for (int c = 4; c <= 8; ++c) {
        const Option o = evaluate(c);
        options.push_back(o);
        std::cout << "  " << std::setw(6) << o.agents
                  << std::setw(10) << o.wq << " +/- " << std::setw(5) << o.wqHalf
                  << std::setw(9)  << o.lq
                  << std::setw(8)  << o.rho
                  << std::setw(11) << o.costPerHour << "\n";
    }

    const Option* best = &options.front();
    for (const Option& o : options) if (o.costPerHour < best->costPerHour) best = &o;

    std::cout << "\n  cheapest on this cost model: " << best->agents << " agents at "
              << best->costPerHour << "/hr\n";

    std::cout << R"(
--- what to notice ------------------------------------------------------
The wait collapses between 4 and 6 agents and then barely moves. That
shape -- steep, then flat -- is universal in queueing, and it is why
"just add one more server" works spectacularly right up until it does
nothing at all.

BEFORE YOU QUOTE THE WINNER, CHECK THE INTERVALS. If two staffing levels
have overlapping intervals on the quantity you care about, your simulation
has NOT shown one is better. Either accept they are indistinguishable, or
run more replications.

Also notice how much the answer depends on COST_PER_WAITING_HOUR, which is
a number somebody made up. Simulation gives you the queueing behaviour
exactly; it cannot give you the value of a customer's time. Say which of
your inputs are measured and which are assumed, and show how the answer
moves when the assumed ones change. That sensitivity analysis is usually
worth more marks than the simulation itself.
--------------------------------------------------------------------------
)";
    return 0;
}
