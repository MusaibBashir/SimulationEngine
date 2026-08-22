// ============================================================================
// 11 — Building a real flowchart: Arena's Basic Process blocks
// ============================================================================
// Everything up to here was a chain: arrive, get served, maybe get served
// again, leave. Real systems branch, wait for groups, scrap things and count
// them. v6 gives you the blocks to say all of that.
//
//   Process   seize a resource, delay, release
//   Delay     hold for a time, no resource (transport, curing, paperwork)
//   Assign    set attributes on the entity
//   Decide    branch, by chance or by a condition on the entity
//   Batch     accumulate N entities into one (packing, palletising)
//   Separate  split a batch back out, or duplicate an entity
//   Record    tally a value without changing anything
//   Dispose   leave the system
//
// The line modelled below:
//
//   arrive -> Stamp -> [Machining x2] -> (conveyor) -> [Inspect]
//                                                         |
//                                          8% fail -> Record -> Dispose(Scrapped)
//                                                         |
//                                          92% pass -> Batch(4) -> [Packing] ->
//                                                      Record(time) -> Dispose(Shipped)
// ============================================================================

#include <iostream>
#include <iomanip>
#include "des.hpp"

using namespace des;

int main() {
    std::cout << std::fixed << std::setprecision(4);

    SimulationSystem sim(4242u);

    sim.model()
        // Parts arrive every ~2 minutes, each carrying a weight we invented.
        .arrivals(exponential(2.0))
        .attribute("weight", uniform(0.5, 2.0))

        // ASSIGN: stamp a lot number on the way in. Instant -- costs no
        // simulated time, which is the point of an Assign.
        .assign("Stamp", "lotNumber", uniform(1.0, 20.0))

        // PROCESS: seize one of two machines, hold for Exponential(1.5),
        // release. This is the seize-delay-release block.
        .station("Machining", /*machines=*/2, FIFO, exponential(1.5))

        // DELAY: a conveyor. Takes time but nothing competes for it, so a
        // hundred parts can be on it at once. Modelling this as a Process with
        // huge capacity would work and would also report a meaningless
        // utilisation for a belt.
        .delay("Conveyor", triangular(0.5, 1.0, 2.0))

        // PROCESS: one inspector.
        .station("Inspect", 1, FIFO, exponential(1.0))

        // DECIDE by chance: 8% of parts fail.
        .decideByChance("Inspection", 0.08)

        // The failing branch: count it, then throw it away.
        .record("ScrapCount")
        .dispose("Scrapped")

        // The passing branch: collect four parts into one carton. PERMANENT,
        // because once packed the individual parts stop being things the model
        // tracks -- the carton is the entity from here on.
        .batch("Packing", /*size=*/4, /*permanent=*/true)
        .station("Packer", 1, FIFO, exponential(3.0))
        .recordTimeInSystem("ShippedAge")
        .dispose("Shipped")

        // --- the wiring ---------------------------------------------------
        .route("Stamp", "Machining")
        .route("Machining", "Conveyor")
        .route("Conveyor", "Inspect")
        .route("Inspect", "Inspection")
        .routeTrue("Inspection", "ScrapCount")   // TRUE  = failed, 8%
        .route("Inspection", "Packing")          // FALSE = passed, 92%
        .route("ScrapCount", "Scrapped")
        .route("Packing", "Packer")
        .route("Packer", "ShippedAge")
        .route("ShippedAge", "Shipped")
        .entryAt("Stamp");

    sim.stopAt(20000.0).warmUpFor(1000.0);
    sim.traceTo("trace_11.md", TraceLevel::Events);
    sim.execute().report();

    // --- reading the block counters ---------------------------------------
    Model& m = sim.model();
    const auto& scrap    = m.nodeAs<RecordNode>("ScrapCount");
    const auto& age      = m.nodeAs<RecordNode>("ShippedAge");
    const auto& packing  = m.nodeAs<BatchNode>("Packing");
    const auto& shipped  = m.nodeAs<DisposeNode>("Shipped");
    const auto& scrapped = m.nodeAs<DisposeNode>("Scrapped");
    const auto& inspect  = m.nodeAs<DecideNode>("Inspection");

    std::cout << "\n--- flowchart counters ------------------------------------\n";
    std::cout << "parts inspected          : " << (inspect.tookTrue() + inspect.tookFalse()) << "\n";
    std::cout << "  failed (target 8%)     : " << scrap.count() << "  ("
              << 100.0 * inspect.tookTrue() / (inspect.tookTrue() + inspect.tookFalse()) << "%)\n";
    std::cout << "cartons packed           : " << packing.batchesFormed()
              << "  (of 4 parts each)\n";
    std::cout << "parts still waiting to fill a carton: " << packing.waitingForBatch() << "\n";
    std::cout << "cartons shipped          : " << shipped.count() << "\n";
    std::cout << "parts scrapped           : " << scrapped.count() << "\n";
    std::cout << "carton age at shipping   : mean " << age.average()
              << ", min " << age.minimum() << ", max " << age.maximum() << "\n";

    std::cout << R"(
--- what to notice ------------------------------------------------------
CARTON AGE IS NOT CARTON SERVICE TIME. A carton's clock starts when its
OLDEST part arrived, not when the fourth one showed up -- so it includes
the time the first part sat waiting for three companions. That is almost
always the number you actually wanted, and it is easy to get silently
wrong by starting the clock at "now" when the batch forms.

THE STABILITY CHECK FOLLOWS THE FLOWCHART. Packer sees one carton per
four parts, and only 92% of parts get that far, so it faces about an
eighth of the arrival rate. The engine works that out by walking the graph
-- branch probabilities, batch sizes and all -- rather than assuming every
block sees every entity. Try setting Packer's service time to 30 minutes:
it refuses, and tells you the load it computed.

PERMANENT vs TEMPORARY BATCH. This model uses permanent: the parts are
consumed and the carton is the entity from here on. Use temporary when the
grouping is transport and you want the members back later -- then a
Separate block splits them out again.

Open trace_11.md and follow one part through: Assign, Seize, Release,
Delay, Seize, Release, Decide, Batch, and finally the carton's Seize and
Exit. The flowchart is legible in the log, which is the real test of
whether you built the model you meant to.
)";

    // ---------------------------------------------------------------------
    // A second, smaller model: TEMPORARY batching and Separate, plus a
    // condition-based Decide.
    // ---------------------------------------------------------------------
    std::cout << "\n########  temporary batch + separate + decide-by-condition  ########\n";
    {
        SimulationSystem s2(7u);
        s2.model()
            .arrivals(exponential(3.0))
            .attribute(attr::priority, uniform(1.0, 10.0))

            // Group three documents into a folder, TEMPORARY so they survive.
            .batch("Folder", 3, /*permanent=*/false)
            .station("Courier", 1, FIFO, exponential(4.0))
            // ...then unpack the folder: every document carries on alone.
            .separate("Unpack")

            // DECIDE BY CONDITION: a predicate on the entity, not a coin flip.
            // Deterministic given the entity -- the same document always goes
            // the same way, which chance-based branching cannot express.
            .decideByCondition("Urgent",
                               [](const Entity& e) { return e.attribute(attr::priority) > 7.0; })
            .station("Express", 1, PRIORITY, exponential(1.0))
            .station("Standard", 1, FIFO, exponential(2.0))
            .dispose("Filed")

            .route("Folder", "Courier")
            .route("Courier", "Unpack")
            .route("Unpack", "Urgent")
            .routeTrue("Urgent", "Express")     // priority > 7
            .route("Urgent", "Standard")        // everyone else
            .route("Express", "Filed")
            .route("Standard", "Filed")
            .entryAt("Folder");

        s2.stopAt(20000.0).warmUpFor(1000.0);
        s2.execute().report();

        const auto& urgent = s2.model().nodeAs<DecideNode>("Urgent");
        std::cout << "\nrouted express : " << urgent.tookTrue()
                  << "   standard : " << urgent.tookFalse()
                  << "   (priority is Uniform(1,10), so ~30% should be > 7)\n";
        std::cout << R"(
The Separate block put every document back into circulation, so three
entities leave the courier for each one that entered it. The engine cannot
compute exact visit ratios through a condition-based Decide or a batch
split -- the split is an OUTPUT of the run -- so it says so instead of
guessing, and relaxes the stability check accordingly. Check those two
stations' rho yourself.
)";
    }
    return 0;
}
