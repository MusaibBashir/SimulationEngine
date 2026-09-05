#include "RunController.hpp"

#include "Model.hpp"
#include "Resource.hpp"
#include "SimulationSystem.hpp"
#include "Station.hpp"
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

}  // namespace des
