#include "ScenarioBuilder.hpp"

#include <cctype>
#include <cstdio>
#include <map>
#include <set>

namespace des {

namespace {

std::string num(double d) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", d);
    return buf;
}

std::string lowerUnit(TimeUnit u) {
    std::string s = timeUnitName(u);
    s[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}

class Builder {
public:
    explicit Builder(const Scenario& s) : m_s(s) {}

    BuiltScenario run() {
        nameBlocks();
        runSetup();
        resources();
        arrivals();
        steps();
        m_out.document.addRow("Dispose");
        set("Dispose", 0, "Name", m_exit);
        return std::move(m_out);
    }

private:
    const Scenario& m_s;
    BuiltScenario   m_out;
    std::map<std::string, std::string> m_block;   // step name -> block name
    std::set<std::string>              m_taken;
    std::string                        m_exit;

    void assume(const std::string& text) { m_out.assumptions.push_back(text); }

    void set(const std::string& type, std::size_t row, const std::string& col, const std::string& v) {
        m_out.document.setCell(type, row, col, v);
    }
    std::size_t add(const std::string& type) {
        m_out.document.addRow(type);
        return m_out.document.rowCount(type) - 1;
    }

    std::string unique(const std::string& base) {
        std::string name = base.empty() ? "Block" : base;
        for (int n = 2; m_taken.count(name) != 0; ++n) name = base + std::to_string(n);
        m_taken.insert(name);
        return name;
    }

    void nameBlocks() {
        for (const SharedResource& r : m_s.resources) m_taken.insert(blockNameFor(r.name));
        for (const Step& st : m_s.steps) m_block[st.name] = unique(blockNameFor(st.name));
        m_exit = unique("Exit");
    }

    // Where "next step" leads from step i when nothing branches.
    std::string fallthrough(std::size_t i) const {
        const Step& st = m_s.steps[i];
        if (st.next == "exit") return m_exit;
        if (!st.next.empty()) return m_block.at(st.next);
        return i + 1 < m_s.steps.size() ? m_block.at(m_s.steps[i + 1].name) : m_exit;
    }
    std::string target(const std::string& stepOrExit) const {
        if (stepOrExit == "exit") return m_exit;
        return m_block.at(stepOrExit);
    }

    void runSetup() {
        const std::size_t r = add("Run");
        set("Run", r, "Name", "Setup");
        const std::string unit = lowerUnit(m_s.timeUnit);
        if (m_s.runLength) {
            set("Run", r, "Length", num(*m_s.runLength));
        } else {
            bool allLimited = !m_s.arrivals.empty();
            for (const ArrivalStream& a : m_s.arrivals) allLimited = allLimited && a.limit.has_value();
            if (allLimited) {
                assume("No run length was given, so it runs until every arrival has been served.");
            } else {
                const double day = convertTime(8.0, TimeUnit::Hours, m_s.timeUnit);
                set("Run", r, "Length", num(day));
                assume("No run length was given, so it runs for one 8-hour day (" + num(day) +
                       " " + unit + ").");
            }
        }
        if (m_s.warmUp && *m_s.warmUp > 0.0) set("Run", r, "Warm-up", num(*m_s.warmUp));
        if (m_s.replications) set("Run", r, "Replications", std::to_string(*m_s.replications));
        set("Run", r, "Time Units", timeUnitName(m_s.timeUnit));
    }

    void resources() {
        for (const SharedResource& res : m_s.resources) {
            const std::size_t r = add("Resource");
            set("Resource", r, "Name", blockNameFor(res.name));
            set("Resource", r, "Capacity", std::to_string(res.servers.value_or(1)));
            if (!res.servers) assume("No count was given for " + res.name + ", so it has 1.");
        }
    }

    void arrivals() {
        for (const ArrivalStream& a : m_s.arrivals) {
            if (!a.every) continue;
            const std::size_t r = add("Create");
            const std::string label = a.name.empty() ? std::string("arrivals") : a.name;
            set("Create", r, "Name", unique(blockNameFor(a.name.empty() ? "Arrivals" : a.name)));
            set("Create", r, "Interarrival",
                expressionFor(*a.every, m_s.timeUnit, &m_out.assumptions, "Time between " + label));
            if (a.limit) set("Create", r, "Max Arrivals", std::to_string(*a.limit));
            if (a.groupSize > 1) set("Create", r, "Per Arrival", std::to_string(a.groupSize));
            if (m_s.steps.empty()) continue;
            set("Create", r, "Next", a.firstStep.empty() ? m_block.at(m_s.steps.front().name)
                                                         : target(a.firstStep));
        }
    }

    void steps() {
        for (std::size_t i = 0; i < m_s.steps.size(); ++i) {
            const Step& st = m_s.steps[i];
            const std::string name = m_block.at(st.name);

            // A step with branches hands over to a Decide, whose ELSE exit is
            // where the step would otherwise have gone.
            std::vector<const Branch*> mine;
            for (const Branch& b : m_s.branches) if (b.from == st.name) mine.push_back(&b);
            std::string next = fallthrough(i);
            if (!mine.empty()) {
                const std::string decide = unique(name + "Split");
                const std::size_t d = add("Decide");
                set("Decide", d, "Name", decide);
                set("Decide", d, "Type", "Chance");
                set("Decide", d, "Next", next);
                for (const Branch* b : mine) {
                    const std::size_t k = add("DecideBranch");
                    set("DecideBranch", k, "Decide", decide);
                    set("DecideBranch", k, "Probability", num(b->probability));
                    set("DecideBranch", k, "To", target(b->to));
                }
                next = decide;
            }

            if (st.kind == StepKind::Delay) {
                const std::size_t r = add("Delay");
                set("Delay", r, "Name", name);
                if (st.time) set("Delay", r, "Duration",
                                 expressionFor(*st.time, m_s.timeUnit, &m_out.assumptions, st.name));
                set("Delay", r, "Next", next);
                continue;
            }

            const std::size_t r = add("Process");
            set("Process", r, "Name", name);
            if (!st.resource.empty()) {
                set("Process", r, "Resource", blockNameFor(st.resource));
            } else {
                set("Process", r, "Capacity", std::to_string(st.servers.value_or(1)));
                if (!st.servers) assume("No server count was given for " + st.name + ", so it has 1.");
            }
            if (!st.queue.empty()) set("Process", r, "Discipline", st.queue);
            if (st.time) set("Process", r, "Service",
                             expressionFor(*st.time, m_s.timeUnit, &m_out.assumptions, st.name));
            if (st.balkAt) set("Process", r, "Balk At", std::to_string(*st.balkAt));
            set("Process", r, "Next", next);
        }
    }
};

}  // namespace

std::string blockNameFor(const std::string& words) {
    std::string out;
    bool upper = true;
    for (const char ch : words) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            out += upper ? static_cast<char>(std::toupper(c)) : ch;
            upper = false;
        } else {
            upper = true;
        }
    }
    if (out.empty()) return "Block";
    if (std::isdigit(static_cast<unsigned char>(out[0]))) out = "B" + out;
    return out;
}

std::string expressionFor(const TimeSpec& t, TimeUnit unit, std::vector<std::string>* assumptions,
                          const std::string& what) {
    auto in = [&](double v) { return num(convertTime(v, t.unit, unit)); };
    switch (t.pattern) {
        case Pattern::Fixed:
            return "CONS(" + in(*t.mean) + ")";
        case Pattern::Random:
            if (assumptions != nullptr && !what.empty())
                assumptions->push_back(what + ": random, averaging " + num(*t.mean) + " " +
                                       lowerUnit(t.unit) + " (exponential).");
            return "EXPO(" + in(*t.mean) + ")";
        case Pattern::Between:
            return "UNIF(" + in(*t.min) + ", " + in(*t.max) + ")";
        case Pattern::Around: {
            // "About four minutes" with no bounds: a quarter either side. Said
            // aloud, because it is the most invented number in the whole model.
            const double lo = t.min ? *t.min : *t.mean * 0.75;
            const double hi = t.max ? *t.max : *t.mean * 1.25;
            if ((!t.min || !t.max) && assumptions != nullptr && !what.empty())
                assumptions->push_back(what + ": \"about " + num(*t.mean) + "\" taken as between " +
                                       num(lo) + " and " + num(hi) + " " + lowerUnit(t.unit) +
                                       ", most often " + num(*t.mean) + ".");
            return "TRIA(" + in(lo) + ", " + in(*t.mean) + ", " + in(hi) + ")";
        }
        case Pattern::Measured:
            return "LOGN(" + in(*t.mean) + ", " + in(*t.sd) + ")";
    }
    return "EXPO(1)";
}

BuiltScenario buildScenario(const Scenario& s) { return Builder(s).run(); }

}  // namespace des
