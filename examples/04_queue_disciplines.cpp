// ============================================================================
// 04 — Queue disciplines: who gets served next?
// ============================================================================
// A single machine, jobs arriving at random, each with its own processing time
// and due date. The machine is a bottleneck, so a queue forms -- and the ORDER
// you take jobs out of that queue changes the answers a lot.
//
// This is the classic job-shop sequencing question, and the engine gives you
// six rules for it.
//
// *** THE PART PEOPLE GET WRONG. ***
// Priority, SPT and EDD sequence on ENTITY ATTRIBUTES. If nothing gives the
// entities those attributes, every entity reads 0.0, every comparison ties, and
// all three silently behave exactly like FIFO. `assignOnArrival` is what stamps
// them. Forget it and you will get three identical columns and a confusing
// afternoon.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

// Build the same job shop every time, changing only the sequencing rule.
void buildShop(SimulationSystem& sim, QueueDiscipline rule) {
    Model& m = sim.model();
    m.setInterarrival(exponential(10.0));

    // Every arriving JOB carries two numbers of its own:
    m.assignOnArrival("serviceTime", uniform(2.0, 14.0));
    m.assignOnArrival("dueDate",     uniform(10.0, 60.0));
    //   - serviceTime : how long this job will take on the machine
    //   - dueDate     : how long after arrival it is promised
    // Priority uses an attribute literally called "priority"; SPT uses
    // "serviceTime"; EDD uses "dueDate". Those names are fixed by the rules.

    Station* machine = m.addStation("Machine", 1, rule,
                                    uniform(2.0, 14.0));

    // *** Make the machine actually TAKE the time the job says it needs. ***
    // Without this the station would draw a fresh random service time and SPT
    // would be sequencing on a number unrelated to reality -- which is a real
    // modelling error, not just an inefficiency.
    machine->setServiceFromAttribute("serviceTime");

    m.setEntry("Machine");
    sim.setTermination(timeLimit(20000.0));
    sim.setWarmUp(2000.0);
}

void trial(const char* label, QueueDiscipline rule) {
    Experiment e(label, [rule](SimulationSystem& s) { buildShop(s, rule); });
    e.replications(10).baseSeed(4000u).warmUp(2000.0);
    e.run();

    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    const std::vector<double> lq = e.column(&ReplicationResult::Lq);
    std::cout << "  " << std::setw(10) << std::left << label << std::right
              << std::setw(9)  << Summary::mean(wq)
              << " +/- " << std::setw(6) << Summary::halfWidth95(wq)
              << std::setw(10) << Summary::mean(lq)
              << "\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "One machine, jobs every ~10 min, each needing 2-14 min.\n";
    std::cout << "10 replications each, 95% intervals.\n\n";
    std::cout << "  rule           mean wait   +/-        Lq\n";
    std::cout << "  ------------------------------------------\n";

    trial("FIFO",     QueueDiscipline::FIFO);
    trial("LIFO",     QueueDiscipline::LIFO);
    trial("SPT",      QueueDiscipline::SPT);
    trial("EDD",      QueueDiscipline::EDD);
    trial("Random",   QueueDiscipline::Random);

    std::cout << R"(
--- what to notice ------------------------------------------------------
SPT wins on MEAN wait, and it is not close. Serving the shortest job first
gets the most jobs out of the way soonest, so the average comes down. This
is a theorem, not an accident.

But look only at the mean and you will make a bad decision: SPT achieves
it by making long jobs wait a very long time, and in a busy shop a long
job can be starved indefinitely. Check `max waiting time` in the full
report, not just the average, before recommending SPT to anyone.

FIFO, LIFO and Random all give the SAME mean wait (within the intervals).
That surprises people. Little's Law says mean wait depends on the total
work in the system, and reordering a queue does not change how much work
is in it. What reordering changes is the VARIANCE -- LIFO's average equals
FIFO's while its worst case is far worse.

EDD does not minimise waiting and is not trying to: it minimises maximum
LATENESS. Judge a rule by the objective it was designed for.

Priority is left out of the table above because this model has no
"priority" attribute -- see the note at the top of the file. Add
  m.assignOnArrival("priority", uniform(1.0, 5.0));
and QueueDiscipline::Priority will serve the highest value first.
-------------------------------------------------------------------------
)";
    return 0;
}
