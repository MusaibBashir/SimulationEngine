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

#include <iostream>
#include "SimulationSystem.hpp"
#include "Common.hpp"

int main() {
    // [2a] Build a TerminationCondition: 480.0 minutes (an eight-hour day) and 1000 entities
    TerminationCondition term(480.0, 1000);

    // [2b] Construct a SimulationSystem from it
    SimulationSystem sim(term);

    // [2c] Resource* teller = sim.addResource("Teller", 1);
    Resource* teller = sim.addResource("Teller", 1);

    // [2d] EntityQueue* line = sim.addQueue("TellerQueue", QueueDiscipline::FIFO);
    EntityQueue* line = sim.addQueue("TellerQueue", QueueDiscipline::FIFO);

    // [2e] Entity* a = sim.createEntity(); Entity* b = sim.createEntity();
    Entity* a = sim.createEntity();
    Entity* b = sim.createEntity();
    a->setAttribute("priority", 2.0);
    b->setAttribute("priority", 5.0);

    // [2f] Build one EventNotice: (EventType::Arrival, 5.0, a, teller)
    EventNotice notice(EventType::Arrival, 5.0, a, teller);

    // [2g] Print, one per line with std::cout:
    std::cout << a->id() << " and " << b->id() << "\n";
    std::cout << a->attribute("priority") << "\n";
    std::cout << b->hasAttribute("dueDate") << "\n";
    std::cout << teller->name() << ", " << teller->capacity() << ", " << teller->unitsAvailable() << "\n";
    std::cout << line->length() << ", " << std::boolalpha << line->isEmpty() << "\n";
    std::cout << notice.time() << "\n";
    std::cout << sim.clock().now() << "\n";

    // [3] Validation check for FEL comparator
    FutureEventList fel;
    fel.schedule(EventNotice(EventType::Arrival, 10.0));
    fel.schedule(EventNotice(EventType::Arrival, 3.0));
    fel.schedule(EventNotice(EventType::Arrival, 7.0));

    while (!fel.isEmpty()) {
        EventNotice top = fel.popImminent();
        std::cout << top.time() << "\n";
    }

    return 0;
}

