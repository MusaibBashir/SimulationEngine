"""
tools/manual_data.py  --  the prose for the reference manual.

Signatures are extracted from the headers by manual_extract.py. Everything in
this file is written by hand: what each type is for, what each member does, and
the call-flow chapters. Prose is passed to reportlab as markup, so use &amp;,
&lt; and &gt; rather than the bare characters.
"""

VERSION_LINE = "v10 — the expression layer · 543 checks · 86 types · 578 public members"

FRONT_MATTER = (
    "A discrete-event simulation engine in the spirit of Arena's Basic Process "
    "template, written from the simulation theory table up. This manual lists "
    "every public type and member, says what each one is for, and traces what "
    "actually calls what when a model runs. "
    "<br/><br/>"
    "The signatures here are <b>extracted from the headers</b> when the manual is "
    "built, so they cannot drift from the code. Only the prose is written by hand."
)

# ---------------------------------------------------------------- types --
TYPE_DOC = {
    # -- foundations
    "EventType": "What kind of thing an event notice represents. The engine dispatches on this in the run loop.",
    "QueueDiscipline": "The service order a queue uses. Selects which IQueueRule a station is built with.",
    "ResourceState": "Whether a resource is idle, partly busy, or fully seized.",
    "Clock": "Simulation time, and the invariant that it never goes backwards. advanceTo() is the only door, which is why the class exists rather than a bare double.",
    "Entity": "The thing that flows through the model: a customer, a part, a patient. Carries an id, a creation time, a type name, and an open-ended bag of numeric attributes.",
    "EventNotice": "One scheduled future occurrence: a time, a kind, and optionally the entity and block it concerns. Ordered by time, with a sequence number breaking ties so equal-time events keep their scheduling order.",
    "FutureEventList": "The pending-event set, kept as a binary heap ordered by time. popImminent() is the beating heart of the run loop. It cannot remove an arbitrary element, which is why reneging uses lazy cancellation instead.",
    "Activity": "A span whose duration is known when it starts — a service time drawn at seize. endTime() is derived, never stored, so the two cannot disagree.",
    "Delay": "A span whose duration is decided later, by the system rather than by a draw — how long an entity waited for a server to free up.",
    "SystemState": "A snapshot of the whole system: number in system, number queued, server status. Summed across stations rather than owned by any one of them.",
    "Statistics": "The accumulators behind every reported average. Holds counts, totals and two time integrals, and knows the difference between a tally average and a time-weighted one.",
    "ModelError": "Thrown when a model is wired wrongly — a programmer's mistake, where stopping is the right response. Contrast Diagnostic, which is how a user's mistake is reported.",

    # -- queues and resources
    "EntityQueue": "A waiting line: a deque of entities plus a pluggable rule that chooses which one leaves next. Tracks its own maximum observed length.",
    "IQueueRule": "The interface a queue discipline implements. One method: given the waiting entities, which index is served next.",
    "FifoRule": "First in, first out. Always index 0.",
    "LifoRule": "Last in, first out. Always the final index.",
    "HighestAttributeRule": "Serves whichever waiting entity has the largest value of a named attribute — the Priority discipline. Ties resolve to the earliest arrival, deliberately.",
    "LowestAttributeRule": "Serves the smallest value of a named attribute — shortest processing time, or earliest due date.",
    "RandomRule": "Picks a waiting entity uniformly at random, drawing from the queue's random stream.",
    "IResourceUser": "The interface a block implements to compete for a shared resource: can it start something, and how long has its longest-waiting entity been waiting.",
    "Resource": "A pool of interchangeable servers with a capacity. Owned by the Model, not by a block, which is what lets several blocks share one operator. When a unit frees, the resource offers it to whichever user has waited longest.",

    # -- randomness
    "EngineKind": "Which underlying generator a RandomStream uses. Includes RANDU, kept precisely so its failure can be demonstrated.",
    "RandomStream": "The single source of randomness. Everything is built on one u01() primitive, so the whole engine is reproducible from one seed. Supports named substreams and antithetic variates.",
    "IDistribution": "The interface every distribution implements: draw a value, report a theoretical mean, describe yourself, clone yourself, and optionally sample from your own stream.",
    "Exponential": "Exponential inter-event times, parameterised by MEAN, not rate. The 1/lambda conversion happens in exactly one line.",
    "Constant": "Always the same value. Consumes nothing from the random stream, which matters when comparing runs.",
    "Uniform": "Uniform on a closed interval.",
    "Triangular": "Triangular with a low, a mode and a high. The usual choice when you have an expert's best, worst and likely case and no data.",
    "Normal": "Normal, optionally truncated at zero so a duration cannot come out negative.",
    "Lognormal": "Lognormal. Its constructor takes the mean and standard deviation of the LOGARITHM; use fromMeanAndSd() if you have the observed values instead.",
    "Weibull": "Weibull, by scale and shape. Common for time-to-failure.",
    "Erlang": "A sum of k exponential phases. fromMean() takes the mean you actually want.",
    "Discrete": "A finite set of values with probabilities. Takes INDIVIDUAL probabilities summing to one; Arena's DISC spelling takes cumulative ones and is converted at the grammar boundary.",
    "Empirical": "Samples straight from observed data with linear interpolation between order statistics — for when you have a hundred real service times and no idea which textbook curve produced them.",
    "Poisson": "Counts rather than durations: arrivals per hour, defects per batch.",
    "Deterministic": "Walks a fixed list of values, optionally repeating. The basis of every hand-checkable test in this project, because it makes a run reproducible on paper.",
    "TestResult": "The outcome of one generator-quality test: a statistic, a threshold, and whether it passed.",
    "StreamTests": "Statistical tests a random stream should survive: chi-square uniformity, Kolmogorov-Smirnov, runs up and down, autocorrelation, and a 3-D serial test that exposes lattice structure.",

    # -- expression layer
    "Value": "What an expression evaluates to: a number or a string. Booleans are numbers, 0 and 1, which is what lets a condition field and a value field be the same kind of field.",
    "ExpressionError": "Thrown when an expression cannot do its job AT RUN TIME — a string in arithmetic, division by zero, an attribute read with no entity.",
    "SourceSpan": "An offset and length within the text that was parsed. Carrying this from the lexer to the diagnostic is what lets a front end put a cursor on the exact bad character.",
    "Severity": "Whether a diagnostic is an error or a warning.",
    "Diagnostic": "A user's mistake, reported as DATA rather than thrown. The parser collects a list of these and keeps going, because a spreadsheet must show every bad cell at once and an exception can only carry one failure.",
    "TokenKind": "The lexical categories of the expression grammar.",
    "Token": "One lexed token: its kind, its span, and its number or text.",
    "LexResult": "Everything one call to tokenise() produced: the tokens, always ending in End, and any diagnostics.",
    "UnaryOp": "The prefix operators: negation and logical not.",
    "BinaryOp": "The infix operators, from arithmetic through comparison to the short-circuiting logicals.",
    "IExpression": "A node in the parsed expression tree. Beyond evaluate(), the interesting members are validate(), which reports problems without throwing, and meanIfKnown(), which admits when a mean cannot be computed in advance.",
    "LiteralExpression": "A number or a quoted string, known at parse time.",
    "NameExpression": "A bare name, resolved LATE — the parser stores only the text, and whether it is an attribute, a variable, Entity.Type or TNOW is decided at validation and evaluation. That is what stops the parser going stale as new kinds of name are added.",
    "UnaryExpression": "A prefix operator applied to one operand.",
    "BinaryExpression": "An infix operator over two operands. The logical operators short-circuit, which is what makes a guarded division safe to write.",
    "LambdaExpression": "Adapter wrapping a C++ predicate so the pre-v10 std::function API is a constructor for the one representation rather than a second code path.",
    "DistributionExpression": "Adapter wrapping an IDistribution so the pre-v10 API keeps working. Also what a parsed EXPO(0.8) with constant arguments becomes.",
    "ParseResult": "What parsing produced: a tree, and the diagnostics found on the way. The tree is never null; on error it holds a placeholder that is never evaluated.",
    "IModelState": "The four questions an expression may ask about a running system, plus the clock. Declared by the expression layer and implemented by SimulationSystem, so the expression layer never has to include Model.hpp.",
    "EvalContext": "The only view of the engine an expression gets while a run is happening: the current entity, the variables, the model state and the random stream. Any of them may be absent, and each accessor says so rather than returning a plausible default.",
    "FieldContext": "Whether the field being validated has an entity to read from. A Create block's interarrival time is evaluated before its entity exists, so an attribute reference there is an error caught before the run.",
    "ValidationContext": "What a name is allowed to resolve to, checked before the run: the declared variables and attributes, and whether an entity is available.",
    "VariableStore": "Arena's Variable data module. Global, numeric, time-persistent values that belong to the system rather than to any entity. A variable may not share a name with an attribute, and writing an undeclared one throws.",

    # -- flowchart
    "NodeContext": "The narrow facade through which a block touches the engine. A node can create, route, schedule and destroy, and cannot reach the future event list, the statistics or the clock.",
    "INode": "A block in the flowchart. Does one job to an entity and decides where it goes next. Everything from Create to Dispose is one of these, which is why the engine has no special case for arrivals.",
    "DelayNode": "Holds an entity for a duration and passes it on. No resource, no queue, no contention — transport, cooling, walking between rooms.",
    "AssignTarget": "What an Assign writes: an entity attribute, a global variable, or the entity's type.",
    "AssignNode": "Sets one or more values on the entity passing through, instantly. Fields run in order, so a later field sees what an earlier one wrote.",
    "DecideNode": "Branches. Either by chance, from a single draw walked against cumulative probabilities, or by condition, first match wins. A block is all-chance or all-condition, never mixed.",
    "BatchNode": "Accumulates entities and sends one representative onward carrying them. Permanent batches consume their members; temporary ones can be split again later. Can group by matching or by distinct attribute values.",
    "SeparateNode": "Splits a temporary batch back into its members, or duplicates an entity. Has two exits — the original and the duplicate — which is a distinction easy to lose when reading an Arena model.",
    "RecordNode": "Tallies something as entities pass: a count, an attribute value, or time in system.",
    "DisposeNode": "The exit. Records the entity's departure and destroys it.",
    "Station": "The Process block: seize a resource, hold the entity for a service time, release. Also the only block that queues, balks and reneges. Implements both INode and IResourceUser.",
    "CreateNode": "An arrival source. Makes entities on its own schedule and reschedules itself. Because it is an ordinary INode, the engine has no separate arrival handler at all.",

    # -- the model
    "Model": "What is simulated, as opposed to the machinery that simulates it. Owns the blocks, the resources and the variables, wires them into a flowchart, and validates the result before a run.",
    "ArrivalAttribute": "An attribute stamped on every arriving entity, drawn from its own distribution. What makes the Priority, SPT and EDD disciplines mean anything.",
    "VisitRatios": "How many times an average entity passes through each block, worked out by walking the flowchart. Carries an exact flag that is false when a condition-based Decide makes the split an output of the run rather than an input.",
    "StabilityReport": "What the offered-load check could and could not determine. Blocks whose service mean is not computable in advance are named rather than silently passed.",

    # -- running
    "StationResults": "One block's numbers after a run: served, utilisation, average wait, queue length.",
    "RunResults": "The whole run's numbers, derived once so that printing is only a view of them.",
    "SimulationSystem": "The engine. Owns the clock, the future event list, the entities, the random streams and the model, and runs the loop that drives them. Also implements IModelState so expressions can read the running system.",
    "TypeStats": "Per-entity-type accounting: number in, number out, work in progress and total time — the figures Arena reports per type.",
    "ITerminationRule": "The interface a stopping condition implements. Asked after every event whether the run should end.",
    "TimeLimit": "Stop when the clock reaches a given time.",
    "EntityLimit": "Stop after a given number of entities have left the system.",
    "DrainedRule": "Stop when nothing is left in the system. Fires at the first moment the system is empty, which may be a transient gap rather than the end of the work.",
    "AnyOf": "Stops when any composed rule says stop.",
    "TraceLevel": "How much detail the trace records, from off to every event.",
    "Trace": "Writes an event-by-event log, optionally as a markdown table. Traces diff, which is what makes a behaviour change show up as a diff rather than as a slightly-off average.",

    # -- experiments
    "ReplicationResult": "One replication's summary figures, so a set of runs can be treated as a sample.",
    "Summary": "The statistics of a sample of replications: mean, standard deviation, standard error and a 95% half-width from Student's t.",
    "Experiment": "Runs a model many times with independent seeds and reports an interval rather than a number. Also implements Welch's method for finding the warm-up period.",
    "Estimate": "A point estimate with its confidence interval.",
    "Comparison": "Two configurations compared, with the interval on their difference — which is what tells you whether a change actually did anything.",
}

# --------------------------------------------------- shared member prose --
COMMON_DOC = {
    "describe": "A human-readable one-line description, used in model dumps and traces.",
    "reset": "Back to the t=0 condition. Configuration survives; run state does not.",
    "resetStatistics": "Discards measurements collected so far and starts again from now, without touching what is held. This is warm-up removal at block level.",
    "clone": "A deep copy. Needed because a distribution may be held in two places at once.",
    "draw": "Samples one value, using this object's own stream if it has been given one.",
    "mean": "The theoretical mean, used by the stability check before the run.",
    "name": "The name this object was constructed with.",
    "enter": "An entity has arrived at this block. Does the block's job and routes onward — possibly not immediately.",
    "onScheduledEvent": "A previously scheduled return has come due, such as the end of a service or a delay.",
    "onRenegeTimeout": "A patience timer has expired. Timers are never cancelled, so this checks whether the entity is still waiting and ignores the event if not.",
    "validate": "Appends every problem found to the list. Does not throw and does not stop at the first.",
    "evaluate": "Computes this node's value in the given context.",
    "meanIfKnown": "The mean if it can be computed in advance, or nothing. Nothing means 'cannot tell', never 'zero'.",
    "useStream": "Gives every sampling site beneath this node its own random stream.",
    "selectIndex": "Which waiting entity is served next, as an index into the queue.",
    "isMet": "Whether this stopping condition now holds.",
    "now": "The current simulation time.",
    "stats": "The statistics accumulator for this block.",
    "count": "How many entities have passed through.",
    "span": "The range of source text this came from.",
    "capacity": "How many units this resource has.",
    "loadPerVisit": "Mean resource-time consumed per entity passing through, divided by capacity. Used by the stability check.",
    "loadIsKnown": "False when the mean cannot be computed in advance, which is not the same as no load.",
}

# ----------------------------------------------- per-type member prose --
MEMBER_DOC = {
    "Clock::advanceTo": "Moves time forward. Refuses to go backwards — the invariant the class exists to protect.",
    "Clock::now": "The current simulation time.",
    "Entity::attribute": "Reads a numeric attribute. Returns 0.0 when absent, which is why callers that care check hasAttribute() first.",
    "Entity::setAttribute": "Inserts or overwrites an attribute.",
    "Entity::hasAttribute": "Whether the attribute has ever been set on this entity.",
    "Entity::copyAttributesFrom": "Copies every attribute from another entity. Used when duplicating.",
    "Entity::type": "The entity's type name, which per-type reporting is keyed on.",
    "Entity::setType": "Changes the type name. Prefer NodeContext::setEntityType, which also moves the live per-type count.",
    "Entity::id": "The unique id assigned at creation.",
    "Entity::creationTime": "When this entity entered the system. Copies inherit the original's, because they are the same work.",
    "EventNotice::operator>": "Orders notices by time, with the sequence number breaking ties so equal-time events keep scheduling order.",
    "EventNotice::sequenceNumber": "A monotonically increasing stamp used only for tie-breaking.",
    "FutureEventList::schedule": "Inserts a notice into the heap.",
    "FutureEventList::popImminent": "Removes and returns the earliest notice. The centre of the run loop.",
    "FutureEventList::nextEventTime": "When the next event is due, without removing it.",
    "Activity::endTime": "Start plus duration, derived rather than stored so the two cannot disagree.",
    "Delay::end": "Closes the delay at the given time.",
    "Delay::hasEnded": "Whether this delay has been closed.",
    "Statistics::updateTimeIntegrals": "Closes the interval that just ENDED, so it must be called before the clock moves and before any state changes, with the OLD values.",
    "Statistics::restartAt": "Throws away everything collected and starts accumulating from now. Warm-up removal in one line.",
    "Statistics::recordArrival": "Counts an arrival.",
    "Statistics::recordDeparture": "Counts a departure and accumulates its waiting time and time in system.",
    "Statistics::timeAverageA": "The first time-weighted average over the measured interval. The caller supplies the elapsed time so that warm-up handling is visible.",
    "Statistics::timeAverageB": "The second time-weighted average over the measured interval.",
    "Statistics::utilisation": "Busy server-time divided by capacity times elapsed time.",
    "EntityQueue::push": "Adds an entity to the back of the line.",
    "EntityQueue::pop": "Removes and returns the entity the discipline selects.",
    "EntityQueue::remove": "Removes a specific entity if it is still waiting, and reports whether it was. How reneging works.",
    "EntityQueue::maxLengthObserved": "The longest this queue has ever been.",
    "EntityQueue::setRandomStream": "Supplies the stream the Random discipline draws from.",
    "Resource::seize": "Takes units. Asserts they are available; the caller must check first.",
    "Resource::release": "Returns units to the pool.",
    "Resource::offerFreedUnit": "Offers a freed unit to whichever registered user has the longest-waiting entity, so a shared resource does not favour the block that released it.",
    "Resource::addUser": "Registers a block as a competitor for this resource.",
    "Resource::unitsAvailable": "Capacity minus units busy, derived rather than stored.",
    "RandomStream::u01": "The single primitive. Every distribution in the engine is built on this one call.",
    "RandomStream::substream": "A named, independent stream derived from the base seed. Naming is what keeps a stream stable as the rest of a model changes.",
    "RandomStream::setAntithetic": "Makes the stream return 1-u instead of u, the basis of antithetic variates.",
    "Lognormal::fromMeanAndSd": "Builds a Lognormal from the mean and standard deviation you observed, rather than those of the logarithm. The conversion is not obvious and getting it wrong is silent.",
    "Erlang::fromMean": "Builds an Erlang from the total mean you want and a phase count.",
    "StreamTests::runAll": "Runs every test and collects the results.",
    "StreamTests::report": "Prints a table of test results.",
    "IExpression::validate": "Appends every problem found. Never throws, so one call reports many.",
    "IExpression::clone": "A deep copy of the whole subtree.",
    "IExpression::reset": "Back to the t=0 condition between replications. Only a Deterministic distribution has anything to do, but an expression that owns one must pass it on.",
    "NameExpression::name": "The text the parser captured, still unresolved.",
    "EvalContext::attribute": "Reads an entity attribute, throwing if there is no entity rather than returning zero.",
    "EvalContext::entityType": "The current entity's type name.",
    "EvalContext::requireRng": "The random stream, or an error explaining that none was supplied.",
    "EvalContext::requireState": "The live model state, or an error explaining that none was supplied.",
    "ValidationContext::isVariable": "Whether this name is a declared variable.",
    "ValidationContext::isAttribute": "Whether this name is a declared attribute.",
    "IModelState::queueLength": "How many entities are waiting at a named block. Throws on an unknown name rather than reading as an empty queue.",
    "IModelState::resourceBusy": "Units of a named resource currently seized.",
    "IModelState::resourceCapacity": "A named resource's capacity.",
    "IModelState::numberInSystem": "How many entities are anywhere in the system.",
    "VariableStore::declare": "Declares a variable with an initial value. Refuses a duplicate, and refuses a name already used by an attribute.",
    "VariableStore::noteAttributeNames": "Supplies the attribute names that declare() checks against.",
    "VariableStore::has": "Whether a variable of this name exists.",
    "VariableStore::get": "The current value.",
    "VariableStore::set": "Closes the time integral at `now`, then changes the value. Throws on an undeclared name, so a typo cannot become a second variable.",
    "VariableStore::updateIntegrals": "Closes every variable's integral up to now.",
    "VariableStore::timeAverage": "The time-weighted average since measurement began.",
    "VariableStore::names": "The variables, in declaration order.",
    "NodeContext::route": "Hands the entity to the next block. A null target means it leaves the system.",
    "NodeContext::scheduleReturn": "Asks to be called back at a given time with this entity — how a Process schedules the end of a service without seeing the event list.",
    "NodeContext::scheduleRenegeCheck": "Schedules a patience timer.",
    "NodeContext::scheduleNextArrival": "A source asking to produce again. A separate call because the event type differs.",
    "NodeContext::createEntity": "Makes a new entity.",
    "NodeContext::registerArrival": "Marks an entity as demand the system received. Only a Create block calls it; batch representatives and duplicates are manufactured, not arrivals.",
    "NodeContext::destroy": "Destroys an entity.",
    "NodeContext::setEntityType": "Retypes an entity through the engine, so the per-type counts move with it.",
    "NodeContext::evaluationContext": "Builds the EvalContext an expression needs. The only route by which a block reaches the variables and the model state.",
    "NodeContext::variables": "The global variable store.",
    "NodeContext::trace": "The trace writer.",
    "NodeContext::rng": "The shared random stream.",
    "INode::setNext": "Sets the block this one routes to by default.",
    "INode::next": "The default onward block.",
    "DecideNode::addBranch": "Adds a branch, in order. Order is meaningful for conditions: first match wins.",
    "DecideNode::setTrueBranch": "Sets the target of the first branch — the two-way spelling.",
    "DecideNode::branches": "The branches, in evaluation order.",
    "DecideNode::tookTrue": "How many entities took the first branch.",
    "DecideNode::tookFalse": "How many took any other branch or fell through.",
    "DecideNode::fellThrough": "How many matched no branch.",
    "DecideNode::isByChance": "Whether this block branches by chance rather than by condition.",
    "AssignNode::set": "Adds a field to this block. Fields run in the order they were added.",
    "AssignNode::rules": "The fields this block writes, in order.",
    "Station::drawService": "Evaluates the service-time expression for this entity.",
    "Station::serviceExpression": "The expression giving the service time.",
    "Station::setBalking": "Refuses to join a queue already this long, sending the entity elsewhere instead.",
    "Station::setReneging": "Gives a waiting entity a random patience, after which it gives up.",
    "Station::setServiceFromAttribute": "Takes the service time from a named attribute the entity already carries.",
    "Station::headOfLineSince": "When the longest-waiting entity here started waiting. A shared resource compares this across blocks.",
    "Station::startFromQueue": "Starts service for the next queued entity, if a unit is available.",
    "Station::hasWaiting": "Whether anything is queued here.",
    "Station::unitsHeld": "Units of the resource this block currently holds — not the same as the resource's total busy count when it is shared.",
    "CreateNode::interarrival": "The expression giving the time until the next arrival. Evaluated with NO entity, because the entity does not exist yet.",
    "CreateNode::exhausted": "Whether this source has produced its maximum number of arrivals.",
    "Model::variable": "Declares a global variable. Checks it does not collide with an attribute name.",
    "Model::variables": "The variable store.",
    "Model::attribute": "Declares an attribute stamped on every arrival, drawn from a distribution.",
    "Model::source": "Adds a named arrival stream. Several may coexist.",
    "Model::arrivals": "The single-source shorthand: creates a source called Arrivals.",
    "Model::resource": "Declares a shared resource that several blocks may seize.",
    "Model::station": "Adds a Process block with its own private resource.",
    "Model::stationUsing": "Adds a Process block that seizes an already-declared shared resource.",
    "Model::process": "Alias for station(), in Arena's vocabulary.",
    "Model::delay": "Adds a Delay block.",
    "Model::assign": "Adds or extends an Assign block. Calling it again with the same name adds another field.",
    "Model::assignTo": "Assigns an attribute from an expression written as text.",
    "Model::assignVariable": "Assigns a global variable from an expression.",
    "Model::assignEntityType": "Restamps the entity's type from an expression.",
    "Model::decideByChance": "Adds a two-way Decide taking the true branch with a fixed probability.",
    "Model::decideByCondition": "Adds a two-way Decide on a predicate or expression.",
    "Model::decideWhen": "Adds a two-way Decide whose condition is written as text.",
    "Model::decideNWayByChance": "Starts an N-way Decide branching by chance.",
    "Model::decideNWayByCondition": "Starts an N-way Decide branching by condition.",
    "Model::branch": "Adds a branch to an N-way Decide, in order.",
    "Model::branchWhen": "Adds a branch whose condition is written as text.",
    "Model::batch": "Accumulates entities into groups of a given size.",
    "Model::batchBySameAttribute": "Groups entities that agree on an attribute — same lot, same order.",
    "Model::batchOneOfEach": "Groups entities that differ — one of each. The rule plain batching cannot fake.",
    "Model::separate": "Splits a temporary batch back into its members.",
    "Model::duplicate": "Copies an entity a given number of times.",
    "Model::record": "Counts entities passing a point.",
    "Model::recordAttribute": "Tallies an attribute's value as entities pass.",
    "Model::recordTimeInSystem": "Tallies time in system as entities pass.",
    "Model::dispose": "Adds a named exit.",
    "Model::route": "Connects one block's default exit to another block.",
    "Model::routeTrue": "Connects a Decide's true branch.",
    "Model::routeDuplicate": "Connects a Separate's duplicate exit. Its original exit is plain route().",
    "Model::entryAt": "Names the block arrivals enter at.",
    "Model::connect": "Alias for route().",
    "Model::setEntry": "Alias for entryAt().",
    "Model::balkAt": "Makes a Process refuse a queue already this long.",
    "Model::renegeAfter": "Gives entities waiting at a Process a random patience.",
    "Model::allowOverload": "Permits a model whose queues grow without bound. Correct for a terminating run, meaningless for a steady-state study, and the report says so.",
    "Model::overloadAllowed": "Whether overload has been permitted.",
    "Model::node": "Looks a block up by name.",
    "Model::nodeAs": "Looks a block up by name and type, throwing if either is wrong rather than returning a null the caller will dereference later.",
    "Model::station": "Looks a Process block up by name.",
    "Model::resourceNamed": "Looks a resource up by name.",
    "Model::visitRatios": "How many times an average entity passes through each block.",
    "Model::offeredLoad": "The offered load at one Process, summed over every arrival stream and weighted by visits.",
    "Model::stability": "What the offered-load check could and could not verify.",
    "Model::arrivalRateIsKnown": "False when any source's interarrival mean cannot be computed in advance, which makes every station's load unknowable too.",
    "Model::checkExpressions": "Validates every expression in the model against the declared names, returning diagnostics rather than throwing.",
    "Model::validate": "Checks the whole model and throws on the first problem. Catches routing loops before they become a hang.",
    "Model::wireSources": "Connects the shorthand source to the entry block, so the order a model is described in does not matter.",
    "Model::describe": "The whole model as text, block by block.",
    "Model::entry": "The block arrivals enter at.",
    "SimulationSystem::initialise": "Validates the model, resets everything holding run state, and schedules the first arrival. Must be called before run().",
    "SimulationSystem::run": "The event loop: pop the earliest event, close the integrals for the interval that just ended, move the clock, then dispatch. That order is not negotiable.",
    "SimulationSystem::report": "Prints the run's results.",
    "SimulationSystem::reportArenaStyle": "Prints the same numbers in Arena's report layout.",
    "SimulationSystem::reportStability": "Prints the blocks the offered-load check could not verify. Prints nothing when everything was checked.",
    "SimulationSystem::results": "The run's numbers, derived once.",
    "SimulationSystem::model": "The model being simulated.",
    "SimulationSystem::stopAt": "Stops the run at a given time.",
    "SimulationSystem::stopAfter": "Stops after a given number of entities have left.",
    "SimulationSystem::setTermination": "Sets an arbitrary stopping rule.",
    "SimulationSystem::setWarmUp": "Discards statistics collected before this time.",
    "SimulationSystem::enableTrace": "Starts writing an event log to a file.",
    "SimulationSystem::useSeparateStreams": "Gives each named role its own random stream, which is what makes two model variants comparable.",
    "SimulationSystem::useAntithetic": "Turns on antithetic variates.",
    "SimulationSystem::variableAverage": "The time-weighted average of a global variable.",
    "SimulationSystem::byType": "Per-entity-type counts and times.",
    "SimulationSystem::clock": "The simulation clock.",
    "SimulationSystem::setObservationInterval": "Samples number-in-system on a fixed grid, for Welch plots.",
    "Experiment::replications": "How many independent runs to make.",
    "Experiment::baseSeed": "The seed the per-replication seeds are derived from.",
    "Experiment::warmUp": "The warm-up period applied to every replication.",
    "Experiment::observeEvery": "The observation grid used for Welch's method.",
    "Experiment::run": "Runs every replication and collects the results.",
    "Experiment::report": "Prints each quantity as a mean with a 95% confidence interval.",
    "Experiment::estimate": "A point estimate with its interval, from a sample.",
    "Experiment::welchAverages": "The Welch moving average across replications, for finding the transient.",
    "Experiment::suggestWarmUp": "A warm-up length suggested by MSER. A starting point, not an answer.",
    "Experiment::writeWelchSeries": "Writes the Welch series to a CSV so it can be plotted.",
    "Experiment::separateStreams": "Turns on per-role random streams for every replication.",
    "Experiment::antitheticPairs": "Pairs each replication with its antithetic twin.",
    "Summary::halfWidth95": "Half the width of a 95% confidence interval, from Student's t.",
    "Summary::tCritical95": "The two-sided 95% critical value of Student's t.",
    "Trace::open": "Starts writing a trace to a file.",
    "Trace::isOn": "Whether tracing is enabled. Callers check this before building a message.",
    "AnyOf::add": "Adds a rule; the composite stops when any of them does.",
}


# ------------------------------------------------------------- chapters --
# ("text", markup) | ("h3", title) | ("code", block) | ("table", rows, widths)
# ("diagram", title, steps, caption) | ("layers", title, rows, caption)
# ("types", [names])   <- signatures come from the headers

CHAPTERS = [

 (1, "Reading This Manual",
  "What is in here, how it was produced, and the shape of the engine as a whole.",
  [
   ("text", "This manual documents every public type and member of the engine. It is "
            "generated: <b>the signatures are read out of include/*.hpp when the PDF is "
            "built</b>, so they cannot drift from the code. Only the prose is written by "
            "hand. If a member appears here it exists; if one is deleted from the code it "
            "disappears from the manual on the next build."),
   ("h3", "Conventions"),
   ("table", [["Shown", "Means"],
              ["class", "Has invariants to protect; members default to private."],
              ["struct", "A bag of values, all public."],
              ["enum class", "A closed set of named alternatives."],
              ["inherits", "Base classes, so the polymorphism is visible."],
              ["I-prefix", "An interface: pure virtual, implemented elsewhere."]],
    [0.30, 0.70]),
   ("text", "Members are listed in header order rather than alphabetically, because header "
            "order is how the author grouped them and that grouping carries meaning."),
   ("layers", "The engine in layers",
    [("Experiments", "Experiment - Summary - replications, intervals", False),
     ("Engine", "SimulationSystem - Clock - FutureEventList - Trace", False),
     ("Model", "Model - INode - Station - Resource", False),
     ("Expressions", "Parser - IExpression - EvalContext - VariableStore", False),
     ("Foundations", "Entity - Statistics - RandomStream - Diagnostic", False)],
    "Each layer uses the ones below it. The expression layer sits at the bottom rather "
    "than beside the model, because it must not depend on Model: it declares "
    "IModelState and the engine implements it."),
   ("h3", "The one rule worth knowing first"),
   ("text", "Inside the run loop the integrals for the interval that just ended are closed "
            "<b>before</b> the clock moves, and the clock moves <b>before</b> any state "
            "changes. Swap any two of those and every time-weighted average goes quietly "
            "wrong, with no error anywhere. Chapter 10 shows the order explicitly."),
  ]),

 (2, "Foundations",
  "Time, entities, events, and the accumulators every reported number comes from.",
  [
   ("types", ["EventType", "QueueDiscipline", "ResourceState", "Clock", "Entity",
              "EventNotice", "FutureEventList", "Activity", "Delay", "SystemState",
              "Statistics", "ModelError"]),
  ]),

 (3, "Queues and Resources",
  "Waiting lines, the disciplines that order them, and the servers they wait for.",
  [
   ("text", "A queue holds entities; a rule decides which one leaves next; a resource is "
            "the pool of servers they are waiting for. Resources are owned by the "
            "<b>Model</b> rather than by a block, which is what makes one operator shared "
            "across three machines expressible at all."),
   ("types", ["EntityQueue", "IQueueRule", "FifoRule", "LifoRule",
              "HighestAttributeRule", "LowestAttributeRule", "RandomRule",
              "IResourceUser", "Resource"]),
  ]),

 (4, "Randomness",
  "One primitive, twelve distributions, and the tests that show a generator is sound.",
  [
   ("text", "Everything random is built on a single u01() call, so a run is reproducible "
            "from one seed. Distributions may be given their own named streams, which is "
            "what lets you change a service time without the arrival pattern shifting "
            "underneath you."),
   ("types", ["EngineKind", "RandomStream", "IDistribution", "Exponential", "Constant",
              "Uniform", "Triangular", "Normal", "Lognormal", "Weibull", "Erlang",
              "Discrete", "Empirical", "Poisson", "Deterministic",
              "TestResult", "StreamTests"]),
  ]),

 (5, "The Expression Layer",
  "How a model's conditions, values and durations became text rather than compiled code.",
  [
   ("text", "Through v9 a service time was a constructed distribution and a condition was a "
            "lambda, so changing a model needed a compiler. v10 makes every one of those "
            "fields a string. There is no separate distribution field any more: EXPO(0.8) "
            "is a function in the grammar whose node owns one of the twelve IDistribution "
            "objects that already existed."),
   ("h3", "Value"),
   ("text", "A variant of double and string: what an expression evaluates to. Strings exist "
            "for one purpose, comparing an entity type. Booleans are numbers, 0 and 1, "
            "which is what lets a condition field and a duration field be the same kind of "
            "field."),
   ("h3", "Errors are data, not exceptions"),
   ("text", "A malformed expression is a person typing in a cell, not a programmer bug, so "
            "the lexer and parser never throw: they record a Diagnostic carrying a "
            "SourceSpan, recover, and keep going. One call reports many problems. The "
            "exception is expr(), which throws, because on the C++ API a bad expression "
            "really is a programmer error and there is no cell to point at."),
   ("types", ["ExpressionError", "SourceSpan", "Severity", "Diagnostic",
              "TokenKind", "Token", "LexResult",
              "UnaryOp", "BinaryOp", "IExpression", "LiteralExpression",
              "NameExpression", "UnaryExpression", "BinaryExpression",
              "LambdaExpression", "DistributionExpression", "ParseResult",
              "IModelState", "EvalContext", "FieldContext", "ValidationContext",
              "VariableStore"]),
  ]),

 (6, "The Flowchart",
  "The blocks entities move through, and the narrow window each one gets on the engine.",
  [
   ("text", "A model is a graph of blocks. Each does one job to an entity and decides where "
            "it goes next. A block reaches the engine only through NodeContext, which "
            "exposes what a block may do and nothing else: it cannot touch the future "
            "event list, the statistics or the clock."),
   ("types", ["NodeContext", "INode", "DelayNode", "AssignTarget", "AssignNode",
              "DecideNode", "BatchNode", "SeparateNode", "RecordNode",
              "DisposeNode", "Station", "CreateNode"]),
  ]),

 (7, "The Model",
  "What is simulated, kept separate from the machinery that simulates it.",
  [
   ("text", "The engine takes a Model and runs it, and knows nothing about tellers or "
            "restaurants. Model owns the blocks, the resources and the variables, and "
            "validates the flowchart before a run, catching a routing loop as an error "
            "rather than as a program that never stops."),
   ("h3", "Nested types"),
   ("text", "<b>ArrivalAttribute</b> is a name and a distribution, stamped on every arrival. "
            "<b>VisitRatios</b> is how often an average entity reaches each block, with an "
            "exact flag that goes false when a condition-based Decide makes the split an "
            "output of the run rather than an input. <b>StabilityReport</b> is what the "
            "offered-load check could and could not judge."),
   ("types", ["Model"]),
  ]),

 (8, "Running a Simulation",
  "The engine, the stopping rules, and the trace that makes a run checkable by hand.",
  [
   ("text", "SimulationSystem owns the clock, the event list, the entities, the random "
            "streams and the model. It also implements IModelState, which is how an "
            "expression reads the running system without the expression layer depending "
            "on the model."),
   ("types", ["SimulationSystem", "StationResults", "RunResults",
              "ITerminationRule", "TimeLimit", "EntityLimit", "DrainedRule", "AnyOf",
              "TraceLevel", "Trace"]),
  ]),

 (9, "Experiments",
  "Why one run is one sample, and what to do about it.",
  [
   ("text", "A single run is one observation of a random variable. Quoting it to four "
            "decimals implies a precision that does not exist. Experiment runs the model "
            "many times with independent seeds and reports an interval."),
   ("h3", "Nested types"),
   ("text", "<b>Estimate</b> is a point estimate with its confidence interval. "
            "<b>Comparison</b> is two configurations and the interval on their difference, "
            "which is what actually answers whether a change did anything."),
   ("types", ["ReplicationResult", "Summary", "Experiment"]),
  ]),

 (10, "Call Flows",
  "What calls what, in order, when a model is built and run.",
  [
   ("text", "These traces follow examples/16_expressions.cpp, which exercises every part of "
            "the engine: text expressions, a condition reading a queue, a global variable, "
            "and a Decide by chance."),

   ("diagram", "1 - building and starting",
    [("main()", "constructs SimulationSystem(seed)", "then describes the model"),
     ("Model::variable / attribute / arrivals", "declares globals and the arrival stream", "each returns *this"),
     ("Model::station(name, cap, rule, text)", "parses the service expression via expr()", "throws on a bad one"),
     ("Model::route / routeTrue / entryAt", "wires the flowchart", ""),
     ("SimulationSystem::initialise()", "the gate every run passes through", "")],
    "Model building is ordinary method calls and nothing is simulated yet. A text field "
    "is parsed here, so a typo is caught while the model is being described rather than "
    "part way through a run."),

   ("diagram", "2 - initialise()",
    [("Model::wireSources()", "connects the shorthand source to the entry", ""),
     ("Model::validate()", "checks the model before anything runs", "throws ModelError"),
     ("Model::checkExpressions()", "every expression against declared names", "collects diagnostics"),
     ("Model::stability()", "offered load per block where knowable", "names what it cannot judge"),
     ("reset() on everything", "clock, stats, state, rng, FEL, entities, model", "configuration survives"),
     ("assignStreams()", "a named substream per role", "if separate streams are on"),
     ("FutureEventList::schedule", "the first arrival, at t=0", "")],
    "Order matters here: the model is validated BEFORE run state is reset, so a broken "
    "model fails without having touched anything."),

   ("diagram", "3 - the run loop",
    [("FutureEventList::popImminent()", "the earliest notice", ""),
     ("updateAllIntegrals(notice.time())", "closes the interval that just ENDED", "uses the OLD state"),
     ("Clock::advanceTo(notice.time())", "time JUMPS; nothing happens between events", ""),
     ("dispatch on notice.type()", "Arrival, Departure, Renege, WarmUpEnd, Observe", ""),
     ("ITerminationRule::isMet()", "asked after every event", "loop, or stop")],
    "This order is not negotiable. Closing the integrals after the clock moved would "
    "credit the new state with time it never held, and every time-weighted average would "
    "be wrong with nothing to show for it."),

   ("diagram", "4 - an arrival",
    [("CreateNode::onScheduledEvent()", "the source's turn to produce", ""),
     ("ctx.scheduleNextArrival(...)", "reschedules ITSELF first", "before routing"),
     ("ctx.createEntity()", "id and creation time assigned here", ""),
     ("ctx.registerArrival(e)", "counts it as demand, stamps attributes", ""),
     ("ctx.route(e, next)", "hands it to the first block", "synchronous"),
     ("Station::enter(ctx, e)", "seize, queue, or balk", "")],
    "The source reschedules before it routes. Routing is synchronous and may travel a long "
    "way through the flowchart; scheduling afterwards would make the arrival stream depend "
    "on what happens downstream."),

   ("diagram", "5 - service, and a freed unit",
    [("Station::enter()", "is a unit available?", "no: queue or balk"),
     ("Station::drawService(ctx, e)", "evaluates the service expression", ""),
     ("Resource::seize(units)", "takes the units", ""),
     ("ctx.scheduleReturn(now + d, e, this)", "books the end of service", ""),
     ("Station::onScheduledEvent()", "service complete", ""),
     ("Resource::release(units)", "returns the units", ""),
     ("Resource::offerFreedUnit(ctx)", "offers to the LONGEST waiter anywhere", "not to this block first"),
     ("ctx.route(e, next)", "onward", "")],
    "The freed unit is offered through the resource rather than handed back to this block's "
    "own queue, because with a shared resource somebody at another block may have been "
    "waiting longer."),

   ("diagram", "6 - evaluating an expression",
    [("Station::drawService(ctx, e)", "needs a duration", ""),
     ("NodeContext::evaluationContext(e)", "the only way a block builds one", ""),
     ("IExpression::evaluate(ctx)", "walks the tree", ""),
     ("NameExpression::evaluate", "TNOW, then variables, then attributes", "resolved LATE"),
     ("StateCall::evaluate", "NQ, NR, MR, WIP", "asks IModelState"),
     ("SimulationSystem::queueLength(name)", "the engine answers", "throws on a bad name"),
     ("DistributionCall::evaluate", "draws from an IDistribution", "")],
    "The expression layer declares IModelState and SimulationSystem implements it, so the "
    "dependency points downward and the expression layer never includes Model.hpp."),

   ("diagram", "7 - a Decide by condition",
    [("DecideNode::enter(ctx, e)", "by chance, or by condition?", ""),
     ("ctx.evaluationContext(e)", "conditions read entity and model state", ""),
     ("IExpression::evaluate per branch", "in order, FIRST MATCH WINS", "so order is meaningful"),
     ("ctx.route(e, branch.target)", "or fall through to next()", "")],
    "A by-chance Decide takes ONE draw and walks it against the cumulative probabilities. "
    "Drawing once per branch would burn several random numbers and would not give the "
    "branch probabilities you asked for."),
  ]),

 (11, "Free Functions",
  "The factories and helpers that are not members of anything.",
  [
   ("h3", "Distribution factories, from Build.hpp"),
   ("text", "Thin wrappers so a model reads as a description rather than as memory "
            "management."),
   ("table", [["Function", "Builds"],
              ["exponential(mean)", "Exponential. Takes a MEAN, not a rate."],
              ["constant(v)", "Constant. Consumes nothing from the stream."],
              ["uniform(a, b)", "Uniform on a closed interval."],
              ["triangular(a, mode, b)", "Triangular."],
              ["normal(mean, sd, truncate)", "Normal, truncated at zero by default."],
              ["lognormal(logMean, logSd)", "Lognormal from log-space parameters."],
              ["lognormalFrom(mean, sd)", "Lognormal from observed mean and sd."],
              ["weibull(scale, shape)", "Weibull."],
              ["erlang(totalMean, phases)", "Erlang with the mean you want."],
              ["discrete(values, probs)", "Discrete, individual probabilities."],
              ["empirical(observations)", "Empirical, sampled from data."],
              ["poisson(mean)", "Poisson counts."],
              ["fixedTimes(values, repeat)", "Deterministic. The basis of hand checks."]],
    [0.42, 0.58]),
   ("h3", "Termination rules, from Build.hpp"),
   ("table", [["Function", "Stops when"],
              ["timeLimit(t)", "the clock reaches t."],
              ["entityLimit(n)", "n entities have left."],
              ["whenDrained()", "the system is empty, which may be a transient gap."],
              ["anyOf(a, b, ...)", "any composed rule fires."]],
    [0.36, 0.64]),
   ("h3", "The expression layer"),
   ("table", [["Function", "Does"],
              ["tokenise(text)", "Text to tokens with spans. Never throws."],
              ["parseExpression(text)", "Tokens to a tree plus diagnostics. Never throws."],
              ["expr(text)", "The same, but throws. For C++ callers."],
              ["isBuiltinFunction(name)", "Whether a name is in the call table."],
              ["isNumber / isText", "Which half of a Value is present."],
              ["asNumber / asText", "Reads a Value, throwing rather than coercing."],
              ["truthy(v)", "Non-zero is true; text throws."],
              ["formatValue(v)", "A Value as text, whole numbers without a point."],
              ["hasErrors(diags)", "Whether any diagnostic is an error."],
              ["formatDiagnostics(diags)", "Diagnostics as text, one per line."],
              ["describe(TokenKind)", "Names a token kind, for messages."],
              ["spelling(op)", "An operator's symbol."]],
    [0.40, 0.60]),
   ("h3", "Grammar functions"),
   ("text", "Usable inside any expression field. Distributions: EXPO, CONS, UNIF, TRIA, "
            "NORM, LOGN, WEIB, ERLA, POIS, DISC. Model state: NQ(block), NR(name), "
            "MR(name), WIP(), and the constant TNOW. Maths: MIN, MAX, ABS, ROUND, TRUNC, "
            "SQRT, LN, EXP, MOD."),
   ("text", "<b>Three traps.</b> EXPO is the exponential distribution while EXP is e to the "
            "x, which is Arena's collision kept deliberately. DISC takes CUMULATIVE "
            "probabilities as Arena writes them, while the engine's own discrete() takes "
            "individual ones. LOGN takes the mean and standard deviation of the variable, "
            "not of its logarithm."),
  ]),
]


# The members the derived-accessor fallback cannot honestly describe.
MEMBER_DOC.update({
    "Entity::addMember": "Adds an entity to this one's batch. A batch representative carries its members until a Separate splits them out.",
    "Entity::members": "The entities this batch representative is carrying.",
    "Entity::isBatch": "Whether this entity is carrying members.",
    "Entity::clearMembers": "Drops the carried members, after a Separate has released them.",
    "Entity::setCreationTime": "Overrides the creation time. Used when a copy must inherit the original's, because it is the same work.",
    "EventNotice::operator>": "Orders notices for the heap: earlier times first, with the sequence number breaking ties so equal-time events keep scheduling order.",
    "Summary::stdDev": "Sample standard deviation, with the n-1 denominator.",
    "Summary::standardError": "Standard deviation divided by the square root of the sample size.",
    "Experiment::column": "Pulls one field out of every replication result as a plain vector, so any of them can be summarised the same way.",
    "Model::assignOnArrival": "Alias for attribute(): stamps a drawn value on every arriving entity.",
    "Model::stationAt": "The i-th Process block. Process blocks are the only ones a utilisation table means anything for.",
    "Model::sourceAt": "The i-th arrival source.",
    "Model::nodeAt": "The i-th block, of any kind.",
    "Model::stationCount": "How many Process blocks the model has.",
    "Model::sourceCount": "How many arrival sources the model has.",
    "Model::nodeCount": "How many blocks the model has in total.",
    "Model::arrivalAttributes": "The attributes stamped on every arrival.",
    "Model::interarrival": "The interarrival distribution kept for the stability check. Not the live one: the sources own those.",
    "DelayNode::durationExpression": "The expression giving the hold time.",
    "RecordNode::timeInSystem": "Builds a Record block that tallies each entity's total time in the system.",
    "RandomStream::exponential": "One exponential sample with the given MEAN. The inversion is the only place the 1/lambda conversion happens.",
    "RandomStream::uniform": "One uniform sample on a closed interval.",
    "RandomStream::normal": "One normal sample, by the Box-Muller transform.",
    "RandomStream::lognormal": "One lognormal sample, from the mean and sd of the LOGARITHM.",
    "RandomStream::weibull": "One Weibull sample, by inversion.",
    "RandomStream::erlang": "One Erlang sample: the sum of `phases` exponential draws.",
    "RandomStream::triangular": "One triangular sample, by inversion either side of the mode.",
    "RandomStream::poisson": "One Poisson count, by Knuth's multiplication method.",
    "RandomStream::bernoulli": "True with probability p. One draw.",
    "RandomStream::uniformIndex": "A uniformly chosen index in [0, n). What the Random queue discipline uses.",
    "RandomStream::normalQuantile": "The inverse normal CDF, used to build normal samples and confidence intervals.",
    "IResourceUser::headOfLineSince": "When this user's longest-waiting entity started waiting. The resource compares this across every block sharing it, so the longest waiter anywhere is served next.",
    "IResourceUser::startFromQueue": "Asks this user to start serving its next queued entity, if it can. Returning without seizing tells the resource to stop offering.",
    "RunResults::station": "One block's results, by name.",
    "SimulationSystem::queueLength": "How many entities wait at a named block. Part of IModelState, so expressions can read it. Throws on an unknown name rather than reading as an empty queue.",
    "SimulationSystem::resourceBusy": "Units of a named resource currently seized. A Process with a private resource is addressable by the block's name.",
    "SimulationSystem::resourceCapacity": "A named resource's capacity.",
    "SimulationSystem::numberInSystem": "How many entities are anywhere in the system.",
    "SimulationSystem::createEntity": "Makes an entity and registers it in the entity table. Called through NodeContext, never directly by a block.",
    "SimulationSystem::execute": "initialise() and run() in one call, for the common case.",
    "SimulationSystem::stopWhen": "Sets an arbitrary stopping rule, returning *this so it chains.",
    "SimulationSystem::warmUpFor": "Discards statistics collected before this time, returning *this so it chains.",
    "Station::resource": "The resource this block seizes. May be shared with other blocks.",
    "Station::queue": "The waiting line at this block.",
    "StreamTests::chiSquareUniformity": "Bins many draws and compares the counts against a flat expectation. Catches a generator whose output is not evenly spread.",
    "StreamTests::kolmogorovSmirnov": "Compares the empirical distribution against the uniform one by their largest gap. Sensitive where chi-square is not.",
    "StreamTests::runsUpAndDown": "Counts ascending and descending runs against their expected number. Catches sequences that are uniform but not independent.",
    "StreamTests::autocorrelation": "Correlation between draws a given lag apart. A generator can pass every one-dimensional test and still fail this.",
    "StreamTests::serial3D": "Bins consecutive triples in the unit cube. This is the test that exposes RANDU, whose triples fall on fifteen planes.",
    "StreamTests::chiSquareCritical95": "The 95% critical value of chi-square, so a test can report pass or fail rather than a bare statistic.",
    "Trace::note": "Writes a free-text line into the trace, outside the event table.",
})

# A handful the derived-accessor line describes awkwardly.
MEMBER_DOC.update({
    "AnyOf::empty": "Whether any rules have been added. An AnyOf with none never stops the run.",
    "Trace::close": "Closes the trace file. Called by the destructor, so this is only for closing early.",
    "Trace::lineCount": "How many lines have been written, so a test can assert a trace was produced.",
    "FutureEventList::clear": "Discards every pending event. Part of resetting between replications.",
    "FutureEventList::isEmpty": "Whether anything is still scheduled. The run loop stops when this is true.",
    "FutureEventList::size": "How many events are pending.",
    "EntityQueue::isEmpty": "Whether anything is waiting here.",
    "EntityQueue::contents": "The waiting entities, in arrival order, whichever discipline is in use.",
    "EntityQueue::ruleName": "The discipline's name, for reports.",
    "Statistics::areaA": "The raw first integral. Divide by measured time for the average.",
    "Statistics::areaB": "The raw second integral. Divide by measured time for the average.",
    "Statistics::lastUpdateTime": "When the integrals were last closed.",
    "Statistics::numberArrived": "How many arrivals were counted.",
    "Statistics::numberServed": "How many departures were counted.",
    "Resource::userCount": "How many blocks compete for this resource.",
    "Resource::isAvailable": "Whether at least one unit is free.",
    "SystemState::serverStatus": "Whether servers are idle, busy or fully seized.",
})
