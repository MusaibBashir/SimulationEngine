// ============================================================================
// 16_expressions.cpp  --  v10: a model written as TEXT
// ============================================================================
// Everything before this example builds a model out of C++ objects: a service
// time is exponential(0.8), a condition is a lambda. That works, and it means
// changing a model requires a compiler.
//
// v10 lets every one of those fields be a string instead. That is not a
// convenience -- it is what makes the next two versions possible, because a
// spreadsheet cell can hold "EXPO(0.8)" and cannot hold a lambda.
//
// Four things this example shows that v9 could not say at all:
//
//   1. A duration computed from the entity      "size * 0.5"
//   2. A condition reading live model state     "NQ(Machine) > 3"
//   3. A global variable, read and written      "Rejected + 1"
//   4. Arena's own spellings                    EXPO, TRIA, DISC
//
// Run it and read the report: the interesting line is how many parts were
// diverted because the machine queue was long, which is a decision no lambda
// could have made -- a lambda sees the entity, never the queue.
// ============================================================================

#include <iomanip>
#include <iostream>
#include "des.hpp"

using namespace des;

int main() {
    std::cout << std::fixed << std::setprecision(4);

    SimulationSystem sim(2026u);
    Model& m = sim.model();

    // A global. Attributes travel with one entity; this belongs to the shop.
    m.variable("Rejected", 0.0);

    // Every part carries a size, drawn on arrival.
    m.attribute("size", uniform(1.0, 4.0));

    m.arrivals("EXPO(2.0)")

     // Machining takes longer for bigger parts. The service time READS THE
     // ENTITY -- this is the field that could not be written before v10.
     .station("Machine", 1, FIFO, "size * 0.5")

     // Inspection time is Arena's triangular, spelled Arena's way.
     .station("Inspect", 1, FIFO, "TRIA(0.3, 0.5, 1.2)")

     // A condition on LIVE MODEL STATE. When the machine queue backs up,
     // parts are sent straight to rework rather than joining it.
     .decideWhen("Busy?", "NQ(Machine) > 3")
     .station("Rework", 1, FIFO, "EXPO(1.0)")

     // Pass or fail, with the failure count kept in a variable so the report
     // can quote it. "Rejected + 1" is an expression with a global on both
     // sides -- the thing an attribute structurally cannot do.
     .decideByChance("Pass?", 0.85)
     .assignVariable("CountFail", "Rejected", "Rejected + 1")
     .dispose("Shipped")
     .dispose("Scrapped")

     .route("Busy?", "Machine")           // false branch: the normal path
     .routeTrue("Busy?", "Rework")        // true branch: too long a queue
     .route("Machine", "Inspect")
     .route("Rework", "Inspect")
     .route("Inspect", "Pass?")
     .routeTrue("Pass?", "Shipped")
     .route("Pass?", "CountFail")
     .route("CountFail", "Scrapped")
     .entryAt("Busy?");

    sim.stopAt(480.0);            // one eight-hour shift
    sim.initialise();

    std::cout << "--- the model as the engine understands it ----------------\n";
    std::cout << m.describe() << "\n";

    // The stability check now has three answers, not two. A service time of
    // "size * 0.5" has no mean the engine can work out in advance, so it says
    // so rather than passing silently or refusing a perfectly good model.
    const Model::StabilityReport stability = m.stability();
    std::cout << "--- stability ---------------------------------------------\n";
    if (stability.checked) {
        std::cout << "every block verified; highest offered load "
                  << stability.maxUtilisation << "\n\n";
    } else {
        std::cout << "could NOT verify:";
        for (const std::string& name : stability.unverifiable) std::cout << " " << name;
        std::cout << "\n(their service times are expressions whose mean cannot be\n"
                     " computed before the run -- not the same as 'no load')\n\n";
    }

    sim.run();
    sim.report();

    const DecideNode& busy = m.nodeAs<DecideNode>("Busy?");
    std::cout << "\n--- what the expressions decided --------------------------\n";
    std::cout << "diverted to rework (NQ(Machine) > 3) : " << busy.tookTrue()  << "\n";
    std::cout << "machined normally                    : " << busy.tookFalse() << "\n";
    std::cout << "parts rejected (variable)            : "
              << m.variables().get("Rejected") << "\n";
    std::cout << "time-average of Rejected             : "
              << sim.variableAverage("Rejected") << "\n";

    std::cout << "\nNone of the three lines above was expressible in v9: the first\n"
                 "needs a condition that can see a queue, the second a service time\n"
                 "that can read the entity, the third a global the model can write.\n";
    return 0;
}
