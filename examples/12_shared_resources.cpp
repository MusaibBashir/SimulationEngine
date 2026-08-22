// ============================================================================
// 12 — Shared resources, balking, reneging, and N-way branching
// ============================================================================
// Up to v6 a Process owned its servers, so "two nurses covering both triage and
// the vaccination room" could not be said. You could give triage two nurses and
// vaccination two nurses, but then you had four.
//
// v7 separates the two ideas:
//     resource(name, capacity)   -- the people or machines, declared once
//     stationUsing(block, resource, ...) -- a block that seizes them
//
// Several blocks can seize the same resource and compete for it. When a unit is
// freed, it goes to whichever waiting queue holds the entity that has been
// waiting LONGEST -- first-come first-served across the whole resource.
//
// This example also adds the two ways people refuse to wait:
//     BALKING   -- will not join a queue that is already too long
//     RENEGING  -- joins, waits, gives up
//
// A walk-in clinic:
//
//   arrive -> [Reception] -> [Triage*] -> Severity ---20%--> [Vaccination*]
//                                              |                    |
//                                          else 80%            Discharge
//                                              v
//                                          [Consult**] -> Discharge
//
//   * Triage and Vaccination share the NURSE pool (2 nurses)
//   ** Consult uses the DOCTOR pool (2 doctors), and patients renege
//   Triage balks when its queue reaches 6
// ============================================================================

#include <iostream>
#include <iomanip>
#include "des.hpp"

using namespace des;

int main() {
    std::cout << std::fixed << std::setprecision(4);

    SimulationSystem sim(2718u);

    sim.model()
        .arrivals(exponential(4.0))          // a patient every ~4 minutes

        // --- the people, declared once and shared -------------------------
        .resource("Nurse",  2)
        .resource("Doctor", 2)
        .resource("Clerk",  1)

        // --- the flowchart -------------------------------------------------
        .stationUsing("Reception",   "Clerk",  FIFO,     exponential(2.0))
        .stationUsing("Triage",      "Nurse",  PRIORITY, exponential(3.0))
        .decideNWayByChance("Severity")
        .stationUsing("Vaccination", "Nurse",  FIFO,     exponential(5.0))
        .stationUsing("Consult",     "Doctor", PRIORITY, exponential(6.0))
        .dispose("Discharged")
        .dispose("WalkedOut")        // balked at triage
        .dispose("LeftUntreated")    // gave up waiting for a doctor

        // Sicker patients are seen first, so give everyone a severity to sort on.
        .attribute(attr::priority, uniform(1.0, 10.0))

        // --- wiring --------------------------------------------------------
        .route("Reception", "Triage")
        .route("Triage", "Severity")
        .branch("Severity", 0.20, "Vaccination")   // 20% just need a jab
        .route("Severity", "Consult")              // the other 80% see a doctor
        .route("Vaccination", "Discharged")
        .route("Consult", "Discharged")
        .entryAt("Reception")

        // --- v7: refusing to wait ------------------------------------------
        // Nobody joins a triage queue six deep; they go elsewhere.
        .balkAt("Triage", 6, "WalkedOut")
        // Patients waiting for a doctor give up after ~40 minutes on average.
        .renegeAfter("Consult", exponential(40.0), "LeftUntreated");

    std::cout << "--- the model as the engine understands it ----------------\n"
              << sim.model().describe() << "\n\n";

    sim.stopAt(20000.0).warmUpFor(1000.0);
    sim.traceTo("trace_12.md", TraceLevel::Events);
    sim.execute().report();

    // --- reading it back ---------------------------------------------------
    Model& m = sim.model();
    const Station& triage  = *m.station("Triage");
    const Station& consult = *m.station("Consult");
    const RunResults r     = sim.results();

    std::cout << "\n--- refusing to wait --------------------------------------\n";
    std::cout << "balked at triage         : " << triage.balked()
              << "   (queue cap 6)\n";
    std::cout << "gave up waiting for a doctor: " << consult.reneged() << "\n";
    std::cout << "discharged               : "
              << m.nodeAs<DisposeNode>("Discharged").count() << "\n";

    std::cout << "\n--- the shared nurse pool ---------------------------------\n";
    const double triageShare = r.station("Triage").utilisation;
    const double vaccShare   = r.station("Vaccination").utilisation;
    std::cout << "Triage's share of the nurses      : " << triageShare << "\n";
    std::cout << "Vaccination's share of the nurses : " << vaccShare << "\n";
    std::cout << "nurses busy overall               : " << (triageShare + vaccShare) << "\n";
    std::cout << "doctors busy                      : "
              << r.station("Consult").utilisation << "\n";

    std::cout << R"(
--- what to notice ------------------------------------------------------
THE TWO NURSE FIGURES ADD UP. Each block reports the fraction of the POOL
it consumed, so triage's share plus vaccination's share is the pool's
utilisation. That is the number to look at when deciding whether to hire a
third nurse -- neither block's figure alone tells you anything, because
neither block owns a nurse.

(This is also where v7 had a real bug. Each block was measuring
`resource().unitsBusy()`, which for a shared resource is the total across
everyone using it -- so both blocks reported the whole pool's utilisation
and the two summed to double the truth. A block must measure what IT
holds. The unit test that caught it just checks that the shares sum to the
pool.)

BALKING CHANGES WHAT YOU MEASURE, NOT JUST WHO IS HAPPY. Without it the
triage queue would absorb every arrival and report an average wait nobody
would actually have tolerated. With a balk threshold the queue is capped
and the demand you turned away is counted separately -- which is usually
the number the decision hangs on.

RENEGING IS LAZILY CANCELLED. A patience timer is scheduled when a patient
sits down and nothing cancels it when they are called -- a binary heap
cannot remove an arbitrary element, which FutureEventList has said since
v1. The event fires anyway and asks "is this patient still waiting?" If
not, it is a stale timer and is ignored. One extra event per queued entity,
and far simpler than any structure that supports real cancellation.

THE STABILITY CHECK NOW SUMS ACROSS A SHARED RESOURCE. Triage needs about
0.375 of the nurse pool and vaccination about 0.125. Each is comfortably
under 1 alone, and the engine checks the TOTAL -- because two blocks at
0.6 each are fine separately and impossible together. Try changing the
nurse count to 1 and it refuses, naming the pool rather than a block.

N-WAY DECIDE. `branch()` adds as many outcomes as you like and whatever is
left over falls through to `route()`. One random draw is walked against a
cumulative probability, so the split is exactly what you asked for -- and
a Decide is all-chance or all-condition, never mixed, because mixing them
has no coherent reading.
)";
    return 0;
}
