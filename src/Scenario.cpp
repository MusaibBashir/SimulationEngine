#include "Scenario.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>

namespace des {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string percent(double fraction) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f%%", fraction * 100.0);
    return buf;
}


class Reader {
public:
    ScenarioRead out;

    void read(const JsonValue& j) {
        Scenario& s = out.scenario;
        if (!j.isObject()) { invalid("", "the answer must be one JSON object"); return; }

        const JsonValue& tu = j["time_unit"];
        if (tu.isString() && !parseTimeUnit(tu.asString(), s.timeUnit))
            invalid("time_unit", "'" + tu.asString() + "' is not seconds, minutes, hours or days");

        s.runLength = duration(j["run_length"], "run_length");
        s.warmUp    = duration(j["warm_up"], "warm_up");
        s.replications = count(j["replications"], "replications", 1);

        const JsonValue& arrivals = j["arrivals"];
        if (arrivals.isArray()) {
            for (std::size_t i = 0; i < arrivals.items().size(); ++i)
                arrival(arrivals.items()[i], "arrivals[" + std::to_string(i) + "]");
        } else if (!arrivals.isNull()) {
            invalid("arrivals", "must be a list");
        }
        if (s.arrivals.empty())
            ask("arrivals", "How often do things arrive? For example \"a customer every two minutes\".");

        const JsonValue& resources = j["resources"];
        if (resources.isArray()) {
            for (std::size_t i = 0; i < resources.items().size(); ++i)
                resource(resources.items()[i], "resources[" + std::to_string(i) + "]");
        }

        const JsonValue& steps = j["steps"];
        if (steps.isArray()) {
            for (std::size_t i = 0; i < steps.items().size(); ++i)
                step(steps.items()[i], "steps[" + std::to_string(i) + "]");
        } else if (!steps.isNull()) {
            invalid("steps", "must be a list");
        }
        if (s.steps.empty())
            ask("steps", "What happens to them once they arrive? For example \"they are served by one of two tellers\".");

        const JsonValue& branches = j["branches"];
        if (branches.isArray()) {
            for (std::size_t i = 0; i < branches.items().size(); ++i)
                branch(branches.items()[i], "branches[" + std::to_string(i) + "]");
        }

        resolveNames();
        checkProbabilities();
        if (out.invalid.empty()) unshadowBranches();
    }

private:
    void invalid(const std::string& path, const std::string& msg) { out.invalid.push_back({path, msg}); }
    void ask(const std::string& path, const std::string& text)    { out.questions.push_back({path, text}); }

    std::string text(const JsonValue& v) { return v.isString() ? trim(v.asString()) : std::string(); }

    std::optional<int> count(const JsonValue& v, const std::string& path, int least) {
        if (v.isNull()) return std::nullopt;
        if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < least ||
            v.asNumber() > 1e9) {
            invalid(path, "must be a whole number of at least " + std::to_string(least));
            return std::nullopt;
        }
        return static_cast<int>(v.asNumber());
    }

    std::optional<double> positive(const JsonValue& v, const std::string& path, bool zeroOk = false) {
        if (v.isNull()) return std::nullopt;
        if (!v.isNumber() || v.asNumber() < 0.0 || (!zeroOk && v.asNumber() == 0.0)) {
            invalid(path, zeroOk ? "must be a number, zero or more" : "must be a number above zero");
            return std::nullopt;
        }
        return v.asNumber();
    }

    bool unitOf(const JsonValue& v, const std::string& path, TimeUnit& unit) {
        if (v.isNull()) { unit = out.scenario.timeUnit; return true; }
        if (v.isString() && parseTimeUnit(v.asString(), unit)) return true;
        invalid(path, "unit must be seconds, minutes, hours or days");
        return false;
    }

    // {"value": 8, "unit": "hours"}, or a bare number in the scenario's unit.
    std::optional<double> duration(const JsonValue& v, const std::string& path) {
        if (v.isNull()) return std::nullopt;
        if (v.isNumber()) return positive(v, path, path == "warm_up");
        if (!v.isObject()) { invalid(path, "must be {\"value\": number, \"unit\": ...}"); return std::nullopt; }
        TimeUnit u{};
        const std::optional<double> value = positive(v["value"], path + ".value", path == "warm_up");
        if (!unitOf(v["unit"], path + ".unit", u) || !value) return std::nullopt;
        return convertTime(*value, u, out.scenario.timeUnit);
    }

    std::optional<TimeSpec> timeSpec(const JsonValue& v, const std::string& path) {
        if (v.isNull()) return std::nullopt;
        if (!v.isObject()) { invalid(path, "must be an object with a pattern"); return std::nullopt; }
        TimeSpec t;
        const std::string p = lower(text(v["pattern"]));
        if (p == "fixed" || p == "exactly" || p == "constant")       t.pattern = Pattern::Fixed;
        else if (p == "random" || p == "average" || p == "exponential" || p.empty())
                                                                     t.pattern = Pattern::Random;
        else if (p == "between" || p == "uniform")                   t.pattern = Pattern::Between;
        else if (p == "around" || p == "about" || p == "triangular") t.pattern = Pattern::Around;
        else if (p == "measured" || p == "lognormal")                t.pattern = Pattern::Measured;
        else { invalid(path + ".pattern", "'" + p + "' is not fixed, random, between, around or measured");
               return std::nullopt; }
        if (!unitOf(v["unit"], path + ".unit", t.unit)) return std::nullopt;
        t.mean = positive(v["mean"], path + ".mean");
        t.min  = positive(v["min"],  path + ".min", true);
        t.max  = positive(v["max"],  path + ".max");
        t.sd   = positive(v["sd"],   path + ".sd");

        switch (t.pattern) {
            case Pattern::Fixed:
            case Pattern::Random:
                if (!t.mean) {
                    // No number at all is a missing fact, not a wrong one --
                    // unless min and max were given, in which case it is
                    // "between" mislabelled, and the model can fix its own label.
                    if (t.min || t.max) { invalid(path, "gives min/max but no mean: use pattern \"between\""); }
                    return std::nullopt;
                }
                break;
            case Pattern::Between:
                if (!t.min || !t.max) { invalid(path, "\"between\" needs both min and max"); return std::nullopt; }
                if (*t.min >= *t.max) { invalid(path, "min must be less than max"); return std::nullopt; }
                break;
            case Pattern::Around:
                if (!t.mean) { invalid(path, "\"around\" needs a mean"); return std::nullopt; }
                if ((t.min && *t.min > *t.mean) || (t.max && *t.max < *t.mean)) {
                    invalid(path, "the mean must lie between min and max");
                    return std::nullopt;
                }
                break;
            case Pattern::Measured:
                if (!t.mean || !t.sd) { invalid(path, "\"measured\" needs a mean and an sd"); return std::nullopt; }
                break;
        }
        return t;
    }

    void arrival(const JsonValue& v, const std::string& path) {
        if (!v.isObject()) { invalid(path, "must be an object"); return; }
        ArrivalStream a;
        a.name = text(v["name"]);
        a.every = timeSpec(v["every"], path + ".every");
        a.limit = count(v["limit"], path + ".limit", 1);
        if (const std::optional<int> g = count(v["group_size"], path + ".group_size", 1)) a.groupSize = *g;
        a.firstStep = text(v["first_step"]);
        if (!a.every && v["every"].isNull())
            ask(path + ".every", "How often do " + (a.name.empty() ? std::string("things") : a.name) +
                                 " arrive? For example \"every two minutes on average\".");
        out.scenario.arrivals.push_back(a);
    }

    void resource(const JsonValue& v, const std::string& path) {
        if (!v.isObject()) { invalid(path, "must be an object"); return; }
        SharedResource r;
        r.name = text(v["name"]);
        if (r.name.empty()) { invalid(path + ".name", "a shared resource needs a name"); return; }
        r.servers = count(v["servers"], path + ".servers", 1);
        out.scenario.resources.push_back(r);
    }

    void step(const JsonValue& v, const std::string& path) {
        if (!v.isObject()) { invalid(path, "must be an object"); return; }
        Step st;
        st.name = text(v["name"]);
        if (st.name.empty()) { invalid(path + ".name", "every step needs a name"); return; }
        const std::string kind = lower(text(v["kind"]));
        if (kind == "delay" || kind == "wait" || kind == "travel") st.kind = StepKind::Delay;
        else if (kind == "process" || kind.empty())                st.kind = StepKind::Process;
        else invalid(path + ".kind", "'" + kind + "' is not process or delay");
        st.servers  = count(v["servers"], path + ".servers", 1);
        st.resource = text(v["resource"]);
        st.time     = timeSpec(v["time"], path + ".time");
        st.balkAt   = count(v["balk_at"], path + ".balk_at", 1);
        st.next     = text(v["next"]);

        const std::string q = lower(text(v["queue"]));
        if (q.empty() || q == "fifo" || q == "first come first served") st.queue = "";
        else if (q == "lifo" || q == "last come first served")        st.queue = "LIFO";
        else if (q == "random")                                        st.queue = "RANDOM";
        else invalid(path + ".queue", "'" + q + "' is not fifo, lifo or random");

        if (!st.time && v["time"].isNull())
            ask(path + ".time", st.kind == StepKind::Delay
                                    ? "How long does " + st.name + " take?"
                                    : "How long does " + st.name + " take for each one it serves?");
        out.scenario.steps.push_back(st);
    }

    void branch(const JsonValue& v, const std::string& path) {
        if (!v.isObject()) { invalid(path, "must be an object"); return; }
        Branch b;
        b.from = text(v["from"]);
        b.to   = text(v["to"]);
        const JsonValue& p = v["probability"];
        if (b.from.empty() || b.to.empty()) { invalid(path, "a branch needs from and to"); return; }
        if (p.isNull()) {
            ask(path + ".probability", "What share of those leaving " + b.from + " go to " + b.to + "?");
            return;
        }
        // "10" for ten percent is the commonest slip there is; anything in
        // (1, 100] is read as a percentage rather than refused.
        double prob = p.isNumber() ? p.asNumber() : -1.0;
        if (prob > 1.0 && prob <= 100.0) prob /= 100.0;
        if (!(prob > 0.0 && prob <= 1.0)) { invalid(path + ".probability", "must be between 0 and 1"); return; }
        b.probability = prob;
        out.scenario.branches.push_back(b);
    }

    // A name as the model wrote it -> the step's own spelling, or empty.
    std::string findStep(const std::string& name) const {
        for (const Step& st : out.scenario.steps) if (lower(st.name) == lower(name)) return st.name;
        return "";
    }
    bool isExit(const std::string& name) const {
        const std::string n = lower(name);
        return n == "exit" || n == "leave" || n == "leaves" || n == "out" || n == "dispose";
    }

    void resolveTarget(std::string& target, const std::string& path) {
        if (target.empty()) return;
        if (isExit(target)) { target = "exit"; return; }
        const std::string found = findStep(target);
        if (found.empty()) invalid(path, "'" + target + "' is not the name of any step");
        else               target = found;
    }

    void resolveNames() {
        Scenario& s = out.scenario;
        for (std::size_t i = 0; i < s.arrivals.size(); ++i)
            resolveTarget(s.arrivals[i].firstStep, "arrivals[" + std::to_string(i) + "].first_step");
        for (std::size_t i = 0; i < s.steps.size(); ++i) {
            Step& st = s.steps[i];
            const std::string path = "steps[" + std::to_string(i) + "]";
            resolveTarget(st.next, path + ".next");
            if (st.resource.empty()) continue;
            bool found = false;
            for (const SharedResource& r : s.resources)
                if (lower(r.name) == lower(st.resource)) { st.resource = r.name; found = true; }
            // Naming a resource nobody declared is the model saying "shared"
            // without saying how many; declare it, and let the builder's
            // assumption (one server) and the load check sort out the count.
            if (!found) s.resources.push_back({st.resource, std::nullopt});
            if (st.kind == StepKind::Delay) invalid(path + ".resource", "a delay uses no resource");
        }
        for (std::size_t i = 0; i < s.branches.size(); ++i) {
            Branch& b = s.branches[i];
            const std::string path = "branches[" + std::to_string(i) + "]";
            const std::string from = findStep(b.from);
            if (from.empty()) invalid(path + ".from", "'" + b.from + "' is not the name of any step");
            else              b.from = from;
            resolveTarget(b.to, path + ".to");
        }
        for (std::size_t i = 0; i < s.steps.size(); ++i)
            for (std::size_t k = i + 1; k < s.steps.size(); ++k)
                if (lower(s.steps[i].name) == lower(s.steps[k].name))
                    invalid("steps[" + std::to_string(k) + "].name",
                            "two steps are called '" + s.steps[k].name + "'");
    }

    // "After the doctor, 20% need an X-ray" with X-ray listed right after
    // Doctor and no `next` on Doctor: read literally, the other 80% fall
    // through to X-ray as well and everybody gets one. A branch to the very
    // place the rest go is never meant, so the rest go past it -- to where the
    // branch target itself goes next -- and the reader says so.
    std::string afterStep(std::size_t i) const {
        const Scenario& s = out.scenario;
        if (!s.steps[i].next.empty()) return s.steps[i].next;
        return i + 1 < s.steps.size() ? s.steps[i + 1].name : std::string("exit");
    }
    void unshadowBranches() {
        Scenario& s = out.scenario;
        for (std::size_t i = 0; i < s.steps.size(); ++i) {
            const std::string fall = afterStep(i);
            if (fall == "exit") continue;
            double branched = 0.0;
            bool shadowed = false;
            for (const Branch& b : s.branches) {
                if (b.from != s.steps[i].name) continue;
                branched += b.probability;
                shadowed = shadowed || b.to == fall;
            }
            if (!shadowed || branched >= 1.0 - 1e-9) continue;
            std::size_t t = 0;
            while (t < s.steps.size() && s.steps[t].name != fall) ++t;
            if (t == s.steps.size()) continue;
            const std::string beyond = afterStep(t);
            if (beyond == fall || beyond == s.steps[i].name) continue;
            s.steps[i].next = beyond;
            out.adjustments.push_back(
                "After " + s.steps[i].name + ", the other " + percent(1.0 - branched) + " go to " +
                (beyond == "exit" ? std::string("the exit") : beyond) + ", not through " + fall +
                " -- otherwise everyone would go to " + fall + ".");
        }
    }

    void checkProbabilities() {
        for (const Step& st : out.scenario.steps) {
            double total = 0.0;
            for (const Branch& b : out.scenario.branches) if (b.from == st.name) total += b.probability;
            if (total > 1.0 + 1e-9)
                invalid("branches", "the branches leaving " + st.name + " add up to more than 1");
        }
    }
};

// Where an entity leaving step i goes, with what probability. Index -1 is the
// exit. Shared by the load calculation and, in spirit, by the builder.
std::vector<std::pair<int, double>> successors(const Scenario& s, std::size_t i) {
    auto indexOf = [&s](const std::string& name) -> int {
        if (name == "exit") return -1;
        for (std::size_t k = 0; k < s.steps.size(); ++k) if (s.steps[k].name == name) return static_cast<int>(k);
        return -1;
    };
    const Step& st = s.steps[i];
    int fallthrough = st.next.empty() ? (i + 1 < s.steps.size() ? static_cast<int>(i + 1) : -1)
                                      : indexOf(st.next);
    std::vector<std::pair<int, double>> out;
    double branched = 0.0;
    for (const Branch& b : s.branches) {
        if (b.from != st.name) continue;
        out.emplace_back(indexOf(b.to), b.probability);
        branched += b.probability;
    }
    if (branched < 1.0) out.emplace_back(fallthrough, 1.0 - branched);
    return out;
}

}  // namespace

bool parseTimeUnit(const std::string& text, TimeUnit& out) {
    const std::string t = lower(trim(text));
    if (t == "s" || t == "sec" || t == "secs" || t == "second" || t == "seconds") { out = TimeUnit::Seconds; return true; }
    if (t == "m" || t == "min" || t == "mins" || t == "minute" || t == "minutes") { out = TimeUnit::Minutes; return true; }
    if (t == "h" || t == "hr" || t == "hrs" || t == "hour" || t == "hours")       { out = TimeUnit::Hours;   return true; }
    if (t == "d" || t == "day" || t == "days")                                    { out = TimeUnit::Days;    return true; }
    return false;
}

std::string timeUnitName(TimeUnit u) {
    switch (u) {
        case TimeUnit::Seconds: return "Seconds";
        case TimeUnit::Minutes: return "Minutes";
        case TimeUnit::Hours:   return "Hours";
        case TimeUnit::Days:    return "Days";
    }
    return "Minutes";
}

double convertTime(double value, TimeUnit from, TimeUnit to) {
    return value * unitsPer(from, to);
}

double meanOf(const TimeSpec& t) {
    switch (t.pattern) {
        case Pattern::Between: return (t.min.value_or(0.0) + t.max.value_or(0.0)) / 2.0;
        case Pattern::Around:
            // Triangular: (min + mode + max) / 3, with the builder's default spread.
            if (t.min && t.max) return (*t.min + *t.mean + *t.max) / 3.0;
            return t.mean.value_or(0.0);
        default: return t.mean.value_or(0.0);
    }
}

ScenarioRead readScenario(const JsonValue& json) {
    Reader r;
    r.read(json);
    return r.out;
}

namespace {

JsonValue mergeObject(const JsonValue& previous, const JsonValue& update) {
    if (!previous.isObject() || !update.isObject()) return update.isNull() ? previous : update;
    JsonValue out = previous;
    for (const auto& m : update.members()) {
        if (m.second.isNull()) continue;
        const JsonValue& before = previous[m.first];
        out.set(m.first, before.isObject() && m.second.isObject() ? mergeObject(before, m.second)
                                                                   : m.second);
    }
    return out;
}

std::string itemKey(const JsonValue& item, bool branch) {
    if (!item.isObject()) return "";
    if (branch) return lower(trim(item["from"].asString())) + "\x1f" + lower(trim(item["to"].asString()));
    return lower(trim(item["name"].asString()));
}

JsonValue mergeList(const JsonValue& previous, const JsonValue& update, bool branch) {
    if (!previous.isArray()) return update;
    std::vector<JsonValue> items = previous.items();
    for (const JsonValue& u : update.items()) {
        const std::string key = itemKey(u, branch);
        // An unnamed item in a list of exactly one is that one -- by position,
        // since an unnamed stream has no name to match with.
        if (key.empty() && !branch && items.size() == 1) {
            items[0] = mergeObject(items[0], u);
            continue;
        }
        bool merged = false;
        for (JsonValue& p : items)
            if (!key.empty() && itemKey(p, branch) == key) { p = mergeObject(p, u); merged = true; break; }
        if (!merged) items.push_back(u);
    }
    JsonValue out = JsonValue::array();
    for (JsonValue& i : items) out.push(std::move(i));
    return out;
}

}  // namespace

JsonValue mergeScenarioJson(const JsonValue& previous, const JsonValue& update) {
    if (!previous.isObject()) return update;
    if (!update.isObject()) return previous;
    JsonValue out = previous;
    for (const auto& m : update.members()) {
        if (m.second.isNull()) continue;
        const bool list = m.first == "arrivals" || m.first == "steps" || m.first == "resources" ||
                          m.first == "branches";
        if (list && m.second.isArray())
            out.set(m.first, mergeList(previous[m.first], m.second, m.first == "branches"));
        else
            out.set(m.first, mergeObject(previous[m.first], m.second));
    }
    return out;
}

std::vector<StepLoad> offeredLoads(const Scenario& s) {
    const std::size_t n = s.steps.size();
    std::vector<double> external(n, 0.0);
    for (const ArrivalStream& a : s.arrivals) {
        if (!a.every) continue;
        const double mean = convertTime(meanOf(*a.every), a.every->unit, s.timeUnit);
        if (!(mean > 0.0)) continue;
        int first = 0;
        if (!a.firstStep.empty())
            for (std::size_t k = 0; k < n; ++k) if (s.steps[k].name == a.firstStep) first = static_cast<int>(k);
        if (a.firstStep == "exit" || n == 0) continue;
        external[static_cast<std::size_t>(first)] += a.groupSize / mean;
    }

    // Traffic equations by fixed-point iteration. A rework loop with return
    // probability p converges geometrically in p; a loop with p = 1 does not
    // converge at all, which is a model nobody meant, and the cap keeps it
    // from spinning -- its rates come out huge and the load check says so.
    std::vector<double> rate = external;
    for (int iter = 0; iter < 500; ++iter) {
        std::vector<double> next = external;
        for (std::size_t i = 0; i < n; ++i)
            for (const auto& succ : successors(s, i))
                if (succ.first >= 0) next[static_cast<std::size_t>(succ.first)] += rate[i] * succ.second;
        double change = 0.0;
        for (std::size_t i = 0; i < n; ++i) change += std::fabs(next[i] - rate[i]);
        rate = next;
        if (change < 1e-12) break;
    }

    std::vector<StepLoad> loads;
    std::vector<double> resourceWork(s.resources.size(), 0.0);
    std::vector<double> resourceRate(s.resources.size(), 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const Step& st = s.steps[i];
        if (st.kind != StepKind::Process || !st.time) continue;
        const double work = rate[i] * convertTime(meanOf(*st.time), st.time->unit, s.timeUnit);
        if (!st.resource.empty()) {
            for (std::size_t r = 0; r < s.resources.size(); ++r)
                if (s.resources[r].name == st.resource) { resourceWork[r] += work; resourceRate[r] += rate[i]; }
            continue;
        }
        loads.push_back({st.name, rate[i], work / st.servers.value_or(1)});
    }
    for (std::size_t r = 0; r < s.resources.size(); ++r)
        loads.push_back({s.resources[r].name, resourceRate[r],
                         resourceWork[r] / s.resources[r].servers.value_or(1)});
    return loads;
}

std::vector<Question> loadQuestions(const Scenario& s) {
    std::vector<Question> out;
    for (const StepLoad& load : offeredLoads(s)) {
        // Exactly 100% is as unstable as 150%; the tolerance stops the
        // fixed-point iteration's last bit of rounding from hiding it.
        if (load.utilisation < 1.0 - 1e-9) continue;
        // Which field the answer fills: the resource's count, or the step's own.
        std::string path;
        int servers = 1;
        for (std::size_t r = 0; r < s.resources.size(); ++r)
            if (s.resources[r].name == load.name) {
                path = "resources[" + std::to_string(r) + "].servers";
                servers = s.resources[r].servers.value_or(1);
            }
        for (std::size_t i = 0; i < s.steps.size() && path.empty(); ++i)
            if (s.steps[i].name == load.name) {
                path = "steps[" + std::to_string(i) + "].servers";
                servers = s.steps[i].servers.value_or(1);
            }
        // Busy-server count is utilisation x servers; one more than that keeps up.
        const int enough = static_cast<int>(std::floor(load.utilisation * servers)) + 1;
        out.push_back({path, load.name + " would be busy " + percent(load.utilisation) +
                                 " of the time with " + std::to_string(servers) +
                                 (servers == 1 ? " server" : " servers") +
                                 ", so its queue would grow without end. How many servers does " +
                                 load.name + " have? (At least " + std::to_string(enough) +
                                 " are needed to keep up, or arrivals must be slower.)"});
    }
    return out;
}

const std::string& scenarioSchemaText() {
    static const std::string text = R"(Answer with ONE JSON object of this shape. Use null for anything the description does not say -- never invent a number.

{
  "time_unit": "minutes",                          // seconds | minutes | hours | days: the unit most times are given in
  "run_length": {"value": 8, "unit": "hours"},     // how long to simulate, or null
  "warm_up": null,                                 // {"value": ..., "unit": ...} to discard at the start, or null
  "replications": null,                            // how many independent runs, or null
  "arrivals": [                                    // one entry per stream of things arriving
    {"name": "Customers",
     "every": TIME,                                // time BETWEEN arrivals
     "limit": null,                                // stop after this many arrivals, or null
     "group_size": null,                           // how many arrive together, or null for one
     "first_step": null}                           // name of the step they go to first; null = the first step
  ],
  "resources": [                                   // only for servers SHARED by several steps
    {"name": "Clerk", "servers": 2}
  ],
  "steps": [                                       // in the order things pass through them
    {"name": "Teller",
     "kind": "process",                            // process = waits for a server; delay = just takes time (travel, drying)
     "servers": 2,                                 // servers used by this step alone, or null
     "resource": null,                             // name of a shared resource instead of own servers, or null
     "time": TIME,                                 // how long the step takes for ONE item
     "queue": null,                                // fifo | lifo | random, or null for fifo
     "balk_at": null,                              // arrivals refuse to join a queue this long, or null
     "next": null}                                 // step name, or "exit", or null = the next step in the list
  ],
  "branches": [                                    // probabilistic routing after a step
    {"from": "Inspect", "to": "Rework", "probability": 0.1}   // the rest follow "next"
  ]
}

TIME is one of:
  {"pattern": "random",   "mean": 2, "unit": "minutes"}            // "on average", "about every", "randomly"
  {"pattern": "fixed",    "mean": 2, "unit": "minutes"}            // "exactly", "always", "every 2 minutes on the dot"
  {"pattern": "between",  "min": 2, "max": 5, "unit": "minutes"}   // "between 2 and 5", "anywhere from 2 to 5"
  {"pattern": "around",   "mean": 4, "min": 3, "max": 6, "unit": "minutes"}   // "usually 4, never under 3 or over 6"
  {"pattern": "measured", "mean": 4, "sd": 1, "unit": "minutes"}   // a mean and a standard deviation
A rate like "30 per hour" means one every 2 minutes: write it as the time between, not the rate.
)";
    return text;
}

}  // namespace des
