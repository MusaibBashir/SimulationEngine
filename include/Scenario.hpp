// ============================================================================
// Scenario.hpp  --  v17: what somebody SAID, before it is a model
// ============================================================================
// The intermediate form between a sentence and a .des file. A small language
// model fills this in; it never writes the file. Every fact here is something
// a person can be asked about in their own words -- "how many tellers?" --
// which is the test for whether a field belongs: Arena's vocabulary goes in
// the BUILDER, not here.
//
// Deliberately narrow: arrival streams, processing steps, delays, chance
// branches, shared resources and run setup. That covers what people describe
// out loud. Assign, Batch, Separate, Record and the rest stay in the editor,
// and the reader says so rather than half-supporting them.
//
// MISSING IS NOT WRONG. A sentence that never mentions service time has not
// made a mistake; it has left a question. Issues are split along that line:
// a missing fact becomes a Question for the person, a malformed one goes back
// to the model that wrote it.

#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Json.hpp"
#include "Schedule.hpp"   // TimeUnit, shared with the engine

namespace des {

// "seconds", "min", "hrs" ... -> the unit: looser than timeUnitFrom(), which
// reads the .des spelling, because this reads what a language model wrote.
bool        parseTimeUnit(const std::string& text, TimeUnit& out);
std::string timeUnitName(TimeUnit u);                 // "Minutes", as [Run] spells it
double      convertTime(double value, TimeUnit from, TimeUnit to);

// How a duration varies, in words a person uses rather than distribution names.
enum class Pattern {
    Fixed,      // "exactly 3 minutes"                      CONS
    Random,     // "on average every 2 minutes"             EXPO
    Between,    // "between 2 and 5 minutes"                UNIF
    Around,     // "about 4, never less than 3 or over 6"   TRIA
    Measured    // "mean 4, standard deviation 1"           LOGN
};

struct TimeSpec {
    Pattern  pattern{Pattern::Random};
    TimeUnit unit{TimeUnit::Minutes};
    std::optional<double> mean, min, max, sd;
};

double meanOf(const TimeSpec& t);   // in t.unit

struct ArrivalStream {
    std::string             name;            // "Customers"; may be empty
    std::optional<TimeSpec> every;           // time between arrivals
    std::optional<int>      limit;           // stop after this many arrivals
    int                     groupSize{1};    // how many arrive together
    std::string             firstStep;       // empty: the first step listed
};

struct SharedResource {
    std::string        name;
    std::optional<int> servers;
};

enum class StepKind { Process, Delay };

struct Step {
    std::string             name;
    StepKind                kind{StepKind::Process};
    std::optional<int>      servers;     // its own servers, when no resource
    std::string             resource;    // a SharedResource's name, or empty
    std::optional<TimeSpec> time;
    std::string             queue;       // "FIFO", "LIFO", ... empty: FIFO
    std::optional<int>      balkAt;      // refuse to join a queue this long
    std::string             next;        // a step, "exit", or empty: the next listed
};

struct Branch {
    std::string from;
    std::string to;           // a step or "exit"
    double      probability{0.0};
};

struct Scenario {
    TimeUnit                    timeUnit{TimeUnit::Minutes};
    std::optional<double>       runLength;     // in timeUnit
    std::optional<double>       warmUp;        // in timeUnit
    std::optional<int>          replications;
    std::vector<ArrivalStream>  arrivals;
    std::vector<SharedResource> resources;
    std::vector<Step>           steps;
    std::vector<Branch>         branches;
};

struct ScenarioIssue {
    std::string path;      // "steps[1].time", for the model to find it
    std::string message;   // for whoever has to fix it
};

struct Question {
    std::string path;      // the field the answer fills
    std::string text;      // "How long does Teller take to serve one customer?"
};

struct ScenarioRead {
    Scenario                   scenario;
    std::vector<ScenarioIssue> invalid;     // malformed: ask the model again
    std::vector<Question>      questions;   // missing: ask the person
    // What the reader changed because the form, read literally, could not be
    // what was meant -- each said out loud with the other assumptions.
    std::vector<std::string>   adjustments;
};

// Never throws. Reads what it can and lists the rest. Run length and warm-up
// arrive with their own units and are converted into the scenario's.
ScenarioRead readScenario(const JsonValue& json);

// The arrival rate reaching each step and the share of capacity it would use.
// A step or resource at or above 100% never reaches steady state; the engine
// refuses such a model, and a person -- not the language model -- knows the
// real server count, so it becomes a Question.
struct StepLoad {
    std::string name;          // the step, or the shared resource
    double      arrivalRate{0.0};
    double      utilisation{0.0};
};
std::vector<StepLoad> offeredLoads(const Scenario& s);

// `update` laid over `previous`: what the model sent wins, what it left out or
// sent as null keeps its earlier value. Steps, arrivals and resources match by
// name and branches by from/to, so an answer that sends back only the step it
// changed -- which a small model does, however it is told -- adds to the form
// rather than replacing it. (The cost: removing a step needs the editor.)
JsonValue mergeScenarioJson(const JsonValue& previous, const JsonValue& update);
std::vector<Question> loadQuestions(const Scenario& s);

// The JSON shape the language model is asked for, in its own words: the one
// place the schema is written down, so the prompt and the reader cannot
// disagree about it.
const std::string& scenarioSchemaText();

}  // namespace des
