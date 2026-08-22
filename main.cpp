// ============================================================================
// main.cpp  --  the v1 acceptance test
// ============================================================================
// Its ONLY job is to prove the objects exist and can be built. Nothing
// simulates. If this prints, v1 is DONE -- resist adding more.
//
// [1] Includes: <iostream>, "SimulationSystem.hpp", "Common.hpp".
//     (SimulationSystem.hpp already drags in Entity, Resource, EntityQueue.)
//
// [2] int main()
//
//     [2a] Build a TerminationCondition: 480.0 minutes (an eight-hour day) and
//          1000 entities.
//
//     [2b] Construct a SimulationSystem from it.
//          // If you accidentally write `SimulationSystem sim2 = sim;` here, you
//          // should get a COMPILE ERROR -- that is the deleted copy constructor
//          // from SimulationSystem.hpp [6] doing its job. Try it once on purpose
//          // and read the error message; then delete the line.
//
//     [2c] Resource* teller = sim.addResource("Teller", 1);
//     [2d] EntityQueue* line = sim.addQueue("TellerQueue", QueueDiscipline::FIFO);
//
//     [2e] Entity* a = sim.createEntity();
//          Entity* b = sim.createEntity();
//          a->setAttribute("priority", 2.0);
//          b->setAttribute("priority", 5.0);
//
//     [2f] Build one EventNotice: (EventType::Arrival, 5.0, a, teller).
//
//     [2g] Print, one per line with std::cout:
//            a->id() and b->id()             -> expect 1 and 2
//            a->attribute("priority")        -> expect 2
//            b->hasAttribute("dueDate")      -> expect 0/false
//            teller->name(), teller->capacity(), teller->unitsAvailable()
//                                            -> expect Teller, 1, 1
//            line->length(), line->isEmpty() -> expect 0, true
//            notice.time()                   -> expect 5
//            sim.clock().now()               -> expect 0
//
//     [2h] return 0;
//
// [3] THE ONE EXTRA CHECK WORTH DOING IN v1 -- it validates the FEL comparator
//     before any simulation depends on it:
//       - build three EventNotices with times 10.0, 3.0, 7.0
//       - schedule all three into a FutureEventList
//       - popImminent() three times, printing each time
//       - you MUST see 3, 7, 10.
//     If you see 10, 7, 3 your operator> is backwards, or you passed std::less.
//     Finding that here costs two minutes. Finding it in v2, through wrong
//     statistics, costs an evening.
//     (This needs popImminent() implemented, so it is technically v2 work --
//     it is the one stub worth filling early.)
//
// ---------------- v1 IS COMPLETE WHEN ----------------
//   - it compiles with -Wall -Wextra and no warnings
//   - it prints the expected values above
//   - nothing simulates
//
// ---------------- v2 ADDITION ----------------
// The v1 storage check above is kept (it is fast and catches structural
// breakage). Below it, main now:
//   - re-checks FEL ordering, and adds a TIE-BREAK check for equal event times
//   - builds and runs an actual M/M/1 model
//   - prints the report and validates it against Little's Law and against the
//     closed-form M/M/1 steady-state result

#include <iostream>
#include <iomanip>
#include <cmath>
#include "SimulationSystem.hpp"
#include "Common.hpp"

int main() {
    // ---------------------------------------------------------------------
    // v1 acceptance test: the objects exist and store what they were given.
    // ---------------------------------------------------------------------
    {
        TerminationCondition term(480.0, 1000);
        SimulationSystem sim(term);

        Resource*    teller = sim.addResource("Teller", 1);
        EntityQueue* line   = sim.addQueue("TellerQueue", QueueDiscipline::FIFO);

        Entity* a = sim.createEntity();
        Entity* b = sim.createEntity();
        a->setAttribute("priority", 2.0);
        b->setAttribute("priority", 5.0);

        const EventNotice notice(EventType::Arrival, 5.0, a, teller);

        std::cout << "--- v1 storage check --------------------------------------\n";
        std::cout << "entity ids               : " << a->id() << ", " << b->id() << "\n";
        std::cout << "a.priority               : " << a->attribute("priority") << "\n";
        std::cout << "b.hasAttribute(dueDate)  : " << std::boolalpha << b->hasAttribute("dueDate") << "\n";
        std::cout << "resource                 : " << teller->name() << ", cap "
                  << teller->capacity() << ", free " << teller->unitsAvailable() << "\n";
        std::cout << "queue                    : len " << line->length()
                  << ", empty " << line->isEmpty() << "\n";
        std::cout << "notice time              : " << notice.time() << "\n";
        std::cout << "clock                    : " << sim.clock().now() << "\n";
    }

    // ---------------------------------------------------------------------
    // FEL ordering check. Pushed as 10, 3, 7 -- must come back 3, 7, 10.
    // If this is wrong, operator> is inverted and nothing below can be trusted.
    // ---------------------------------------------------------------------
    {
        FutureEventList fel;
        fel.schedule(EventNotice(EventType::Arrival, 10.0));
        fel.schedule(EventNotice(EventType::Arrival,  3.0));
        fel.schedule(EventNotice(EventType::Arrival,  7.0));

        std::cout << "\n--- FEL ordering check ------------------------------------\n";
        std::cout << "expect 3 7 10            : ";
        while (!fel.isEmpty()) std::cout << fel.popImminent().time() << " ";
        std::cout << "\n";
    }

    // ---------------------------------------------------------------------
    // v2: FEL TIE-BREAK check. Three events at the SAME time must come back in
    // the order they were scheduled. Without this, two runs with the same seed
    // can diverge and "reproducible" is a claim you cannot make.
    // ---------------------------------------------------------------------
    {
        FutureEventList fel;
        fel.schedule(EventNotice(EventType::Arrival,   5.0));
        fel.schedule(EventNotice(EventType::Departure, 5.0));
        fel.schedule(EventNotice(EventType::Arrival,   5.0));

        std::cout << "expect Arr Dep Arr       : ";
        while (!fel.isEmpty()) {
            const EventNotice n = fel.popImminent();
            std::cout << (n.type() == EventType::Arrival ? "Arr " : "Dep ");
        }
        std::cout << "\n";
    }

    // ---------------------------------------------------------------------
    // v2: an actual M/M/1 run.
    //   mean interarrival 1.0 -> lambda = 1.00
    //   mean service      0.8 -> mu     = 1.25
    //   rho = lambda/mu = 0.8
    // Steady-state M/M/1 theory:
    //   Wq = rho / (mu - lambda) = 0.8 / 0.25 = 3.20
    //   Lq = lambda * Wq         = 3.20
    //   utilisation = rho        = 0.80
    // A finite run will not match exactly -- that is sampling error, not a bug.
    // ---------------------------------------------------------------------
    {
        TerminationCondition term(20000.0, 1000000);
        SimulationSystem sim(term, /*seed=*/12345u);

        sim.addResource("Teller", 1);
        sim.addQueue("TellerQueue", QueueDiscipline::FIFO);
        sim.setModel("Teller", "TellerQueue");
        sim.setMeanInterarrival(1.0);
        sim.setMeanService(0.8);

        sim.initialise();
        sim.run();

        std::cout << "\n";
        sim.report();

        // -----------------------------------------------------------------
        // LITTLE'S LAW: Lq = lambda_eff * Wq. The single most useful test in
        // the project. Disagreement means the accumulators are wrong.
        // -----------------------------------------------------------------
        const Statistics& st   = sim.statistics();
        const SimTime total    = sim.clock().now();
        const double lambdaEff = st.numberArrived() / total;
        const double Lq        = st.timeAverageQueueLength(total);
        const double Wq        = st.averageWaitingTime();

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "\n--- Little's Law check ------------------------------------\n";
        std::cout << "lambda_eff               : " << lambdaEff << "\n";
        std::cout << "Lq measured              : " << Lq << "\n";
        std::cout << "lambda_eff * Wq          : " << lambdaEff * Wq << "\n";
        std::cout << "relative error           : "
                  << (Lq > 0.0 ? std::fabs(Lq - lambdaEff * Wq) / Lq : 0.0) << "\n";
        std::cout << "theory rho=0.8           : Wq 3.2000  Lq 3.2000  util 0.8000\n";
        std::cout << "-----------------------------------------------------------\n";
    }

    return 0;
}
