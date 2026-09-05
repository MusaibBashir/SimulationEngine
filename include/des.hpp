// ============================================================================
// des.hpp  --  the only header you need to include
// ============================================================================
// Everything in this engine lives in namespace `des`. Include this one header
// and either qualify names (`des::Model`) or open the namespace:
//
//     #include "des.hpp"
//     using namespace des;
//
// WHY A NAMESPACE AT ALL: the engine defines names like Model, Entity, Clock,
// Uniform, Constant, Summary and Trace. Every one of those is a name your own
// code, or another library, might reasonably want. Before v5 they were global
// and the first collision would have been a wall of template errors far from
// the actual problem.
//
// WHY AN UMBRELLA HEADER: so that "which header declares Triangular?" stops
// being a question you have to answer. The cost is a slightly slower compile;
// at this size that is nothing, and any file that cares can still include the
// individual headers directly.

#pragma once

#include "Common.hpp"
#include "ModelError.hpp"
#include "Diagnostic.hpp"
#include "Value.hpp"
#include "Lexer.hpp"
#include "Entity.hpp"
#include "Resource.hpp"
#include "EntityQueue.hpp"
#include "QueueRule.hpp"
#include "EventNotice.hpp"
#include "FutureEventList.hpp"
#include "Clock.hpp"
#include "SystemState.hpp"
#include "Activity.hpp"
#include "Delay.hpp"
#include "Statistics.hpp"
#include "RandomStream.hpp"
#include "StreamTests.hpp"
#include "Distribution.hpp"
#include "EvalContext.hpp"
#include "VariableStore.hpp"
#include "Expression.hpp"
#include "Functions.hpp"
#include "Parser.hpp"
#include "TerminationRule.hpp"
#include "Trace.hpp"
#include "Node.hpp"
#include "Nodes.hpp"
#include "Create.hpp"
#include "Station.hpp"
#include "Model.hpp"
#include "SimulationSystem.hpp"
#include "Experiment.hpp"
#include "Build.hpp"
