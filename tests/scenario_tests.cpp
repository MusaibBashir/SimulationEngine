// ============================================================================
// scenario_tests.cpp  --  v17: models from a description. Everything but the
// network: JSON, the pre-parser, the scenario reader, the load questions, the
// builder, and the generator against recorded replies. The language model is
// never called; what it might answer is written down here instead.
// ============================================================================

#include <cmath>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des.hpp"
#include "CurlTransport.hpp"
#include "Json.hpp"
#include "ModelGenerator.hpp"
#include "PreParse.hpp"
#include "Scenario.hpp"
#include "ScenarioBuilder.hpp"

using namespace des;
using des_test::check;
using des_test::section;

namespace {

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

JsonValue json(const std::string& text) {
    const JsonParse p = parseJson(text);
    if (!p.ok) check(false, "test JSON parses: " + p.error);
    return p.value;
}

const char* kTeller = R"({
  "time_unit": "minutes",
  "run_length": {"value": 8, "unit": "hours"},
  "arrivals": [{"name": "Customers", "every": {"pattern": "random", "mean": 2, "unit": "minutes"}}],
  "steps": [{"name": "Teller", "kind": "process", "servers": 2,
             "time": {"pattern": "random", "mean": 3, "unit": "minutes"}}]
})";

// A reply in the shape an OpenAI-compatible server sends.
std::string reply(const std::string& content, int status = 200) {
    (void)status;
    JsonValue msg = JsonValue::object();
    msg.set("role", JsonValue::string("assistant"));
    msg.set("content", JsonValue::string(content));
    JsonValue choice = JsonValue::object();
    choice.set("index", JsonValue::number(0));
    choice.set("message", msg);
    choice.set("finish_reason", JsonValue::string("stop"));
    JsonValue choices = JsonValue::array();
    choices.push(choice);
    JsonValue body = JsonValue::object();
    body.set("choices", choices);
    return writeJson(body);
}

class RecordedTransport : public ITransport {
public:
    std::vector<HttpResponse> replies;
    std::vector<std::string>  bodies;
    std::vector<std::vector<std::string>> headers;
    std::vector<std::string>  urls;

    HttpResponse post(const std::string& url, const std::vector<std::string>& h,
                      const std::string& body) override {
        urls.push_back(url);
        headers.push_back(h);
        bodies.push_back(body);
        if (replies.empty()) return HttpResponse{};
        HttpResponse r = replies.front();
        replies.erase(replies.begin());
        return r;
    }
    void add(int status, const std::string& body) {
        HttpResponse r;
        r.ok = true;
        r.status = status;
        r.body = body;
        replies.push_back(r);
    }
};

LlmConfig testConfig() {
    LlmConfig c;
    c.apiKey = "gsk_testkey0123456789abcd";
    return c;
}

void testJson() {
    section("v17: JSON");
    const JsonParse p = parseJson(R"( {"a": [1, 2.5, -3e2, true, false, null], "b": "x\"y\\z\né"} )");
    check(p.ok, "a mixed document parses");
    check(p.value["a"].items().size() == 6, "an array keeps every item");
    check(p.value["a"].items()[2].asNumber() == -300.0, "exponents are read");
    check(p.value["b"].asString() == "x\"y\\z\n\xc3\xa9", "escapes, including \\u, are decoded");
    check(p.value["missing"]["deeper"].isNull(), "a lookup into nothing is null, not a crash");

    const std::string round = writeJson(p.value);
    const JsonParse again = parseJson(round);
    check(again.ok && writeJson(again.value) == round, "written JSON reads back to the same text");
    check(writeJson(JsonValue::number(0.1)) == "0.1", "0.1 is written as 0.1, not 0.1000000000000000055");
    check(writeJson(JsonValue::number(480)) == "480", "a whole number has no decimal point");

    check(!parseJson("{\"a\": 1,}").ok, "a trailing comma is refused");
    check(!parseJson("{\"a\": 1} x").ok, "text after the value is refused");
    check(contains(parseJson("{\n\"a\" 1}").error, "line 2"), "an error says which line");
    check(!parseJson(std::string(10000, '[')).ok, "absurd nesting is refused, not recursed into");

    const JsonParse fenced = parseJsonObjectIn("Sure! Here it is:\n```json\n{\"a\": \"}\"}\n```\nHope that helps.");
    check(fenced.ok && fenced.value["a"].asString() == "}",
          "an object wrapped in prose and a fence is found, a brace inside a string notwithstanding");
    check(!parseJsonObjectIn("no braces at all").ok, "a reply with no object is an error");

    JsonValue o = JsonValue::object();
    o.set("k", JsonValue::number(1));
    check(writeJson(o, 2) == "{\n  \"k\": 1\n}", "indented output is laid out a level per line");
}

void testPreParse() {
    section("v17: pre-parse");
    check(preParse("two tellers").text == "2 tellers", "a number word becomes a digit");
    check(preParse("twenty five customers").text == "25 customers", "tens and units combine");
    check(preParse("one hundred and ten jobs").text == "110 jobs", "hundreds, with 'and'");
    check(preParse("two and a half minutes").text == "2.5 minutes", "'and a half' is a half");
    check(preParse("2 and a half minutes").text == "2.5 minutes", "the same with a digit");
    check(preParse("half an hour").text == "30 minutes", "half an hour is 30 minutes");
    check(preParse("a quarter of an hour").text == "a 15 minutes", "a quarter of an hour is 15 minutes");
    check(preParse("a dozen trucks").text == "a 12 trucks", "a dozen is 12");
    check(preParse("two and three").text == "2 and 3", "'and' between two numbers joins nothing");

    const PreParsed rate = preParse("thirty customers per hour, served in 3 minutes");
    check(rate.notes.size() == 1 && contains(rate.notes[0], "one every 2 minutes"),
          "a rate per hour is turned into the time between arrivals");
    check(contains(rate.text, "per hour (on average one every 2 minutes)"),
          "...and written beside the rate, where the model will read it");

    check(contains(preParse("4 trucks a day").text, "(on average one every 6 hours)"),
          "a slow rate is given in hours");
    check(contains(preParse("120 calls an hour").text, "(on average one every 30 seconds)"),
          "a fast rate is given in seconds");
    check(preParse("takes 5 minutes per customer").notes.empty(),
          "'5 minutes per customer' is a time, not a rate");
    check(preParse("2 tellers each serving").notes.empty(), "'each' with no unit after it is not a rate");

    const PreParsed about = preParse("a doctor for about 15 minutes, arriving thirty an hour");
    check(contains(about.text, "about 15 minutes (pattern \"around\", mean 15)"),
          "'about 15 minutes' carries the pattern it means, beside the words");
    check(contains(about.text, "(on average one every 2 minutes)") && about.notes.size() == 2,
          "...alongside a rate hint in the same sentence");
    check(contains(preParse("walk, roughly one minute").text, "(pattern \"around\", mean 1)"),
          "'roughly one minute': number words first, then the hint");
    check(preParse("about every 6 minutes").notes.empty(),
          "'about every 6 minutes' is a rough arrival rate, not a duration, and gets no hint");
}

void testScenarioReader() {
    section("v17: the scenario reader");
    const ScenarioRead ok = readScenario(json(kTeller));
    check(ok.invalid.empty() && ok.questions.empty(), "a complete description reads cleanly");
    check(ok.scenario.runLength && *ok.scenario.runLength == 480.0, "8 hours is 480 in a minutes model");
    check(ok.scenario.steps.size() == 1 && ok.scenario.steps[0].servers == 2, "the step and its servers");

    const ScenarioRead missing = readScenario(json(R"({
        "arrivals": [{"name": "Cars", "every": {"pattern": "random", "mean": 4, "unit": "minutes"}}],
        "steps": [{"name": "Wash", "time": null}]})"));
    check(missing.invalid.empty(), "a missing time is not a mistake");
    check(missing.questions.size() == 1 && missing.questions[0].path == "steps[0].time" &&
              contains(missing.questions[0].text, "Wash"),
          "...it is a question for the person, naming the step");

    const ScenarioRead noArrivals = readScenario(json(R"({"steps": [{"name": "Wash",
        "time": {"pattern": "fixed", "mean": 5}}]})"));
    check(!noArrivals.questions.empty() && noArrivals.questions[0].path == "arrivals",
          "no arrivals at all is a question too");

    const ScenarioRead bad = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "sometimes", "mean": 4}}],
        "steps": [{"name": "A", "time": {"pattern": "between", "min": 5, "max": 2}, "next": "Nowhere"}]})"));
    check(bad.invalid.size() == 3, "an unknown pattern, min above max and a dangling next are all reported");
    check(contains(bad.invalid[0].path, "arrivals[0].every.pattern"), "each with the path to fix");

    const ScenarioRead branches = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 5}}],
        "steps": [{"name": "inspect", "time": {"pattern": "fixed", "mean": 1}},
                  {"name": "Rework", "time": {"pattern": "fixed", "mean": 2}, "next": "Inspect"}],
        "branches": [{"from": "Inspect", "to": "rework", "probability": 10}]})"));
    check(branches.invalid.empty(), "names match regardless of case");
    check(branches.scenario.branches.size() == 1 &&
              branches.scenario.branches[0].probability == 0.1 &&
              branches.scenario.branches[0].to == "Rework",
          "10 is read as 10% and the target takes the step's own spelling");

    const ScenarioRead shared = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 1}}],
        "steps": [{"name": "Intake", "resource": "clerk", "time": {"pattern": "random", "mean": 0.5}},
                  {"name": "Review", "resource": "Clerk", "time": {"pattern": "random", "mean": 0.7}}]})"));
    check(shared.invalid.empty() && shared.scenario.resources.size() == 1,
          "a resource named but never declared is declared once, for both steps");

    const ScenarioRead overBranched = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 5}}],
        "steps": [{"name": "A", "time": {"pattern": "fixed", "mean": 1}}, {"name": "B", "time": {"pattern": "fixed", "mean": 1}}],
        "branches": [{"from": "A", "to": "B", "probability": 0.7}, {"from": "A", "to": "exit", "probability": 0.6}]})"));
    check(!overBranched.invalid.empty(), "branches adding up to more than 1 are refused");

    check(!readScenario(json("[1, 2]")).invalid.empty(), "an answer that is not an object is refused");

    // The clinic, as Qwen 3.8 27B first sent it: X-ray listed right after
    // Doctor, a 20% branch to it, and no `next` on Doctor -- so read literally
    // everybody went to X-ray (140% busy).
    const ScenarioRead clinic = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 5}}],
        "steps": [{"name": "Doctor", "servers": 3, "time": {"pattern": "random", "mean": 15}},
                  {"name": "X-ray", "time": {"pattern": "fixed", "mean": 10}},
                  {"name": "Discharge", "time": {"pattern": "fixed", "mean": 2}}],
        "branches": [{"from": "Doctor", "to": "X-ray", "probability": 0.2}]})"));
    check(clinic.invalid.empty() && clinic.scenario.steps[0].next == "Discharge",
          "a branch to the step the rest fall through to sends the rest past it");
    check(clinic.adjustments.size() == 1 && contains(clinic.adjustments[0], "other 80% go to Discharge"),
          "...and says so");
    std::vector<StepLoad> cl = offeredLoads(clinic.scenario);
    check(cl.size() == 3 && std::abs(cl[1].utilisation - 0.4) < 1e-9,
          "so X-ray gets 20% of the traffic: 0.2 x 0.2/min x 10 = 40%, not 200%");
    const ScenarioRead meant = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 5}}],
        "steps": [{"name": "Inspect", "time": {"pattern": "fixed", "mean": 1}, "next": "Ship"},
                  {"name": "Rework", "time": {"pattern": "fixed", "mean": 2}, "next": "Inspect"},
                  {"name": "Ship", "time": {"pattern": "fixed", "mean": 1}}],
        "branches": [{"from": "Inspect", "to": "Rework", "probability": 0.1}]})"));
    check(meant.adjustments.empty() && meant.scenario.steps[0].next == "Ship",
          "a form that already says where the rest go is left alone");

    // Merging a partial answer onto the form.
    const JsonValue merged = mergeScenarioJson(
        json(R"({"run_length": {"value": 8, "unit": "hours"}, "steps": [{"name": "Teller", "servers": 1,
                 "time": {"pattern": "random", "mean": 3}}], "branches": [{"from": "A", "to": "B", "probability": 0.1}]})"),
        json(R"({"run_length": null, "steps": [{"name": "teller", "servers": 4}, {"name": "Exit desk", "servers": 1}],
                 "branches": [{"from": "a", "to": "b", "probability": 0.2}]})"));
    check(merged["run_length"]["value"].asNumber() == 8, "null in the answer keeps the earlier value");
    check(merged["steps"].items().size() == 2 && merged["steps"].items()[0]["servers"].asNumber() == 4 &&
              merged["steps"].items()[0]["time"]["mean"].asNumber() == 3,
          "a step matched by name is updated field by field; a new one is added");
    check(merged["branches"].items().size() == 1 && merged["branches"].items()[0]["probability"].asNumber() == 0.2,
          "a branch matched by from/to is updated, not duplicated");
}

void testLoad() {
    section("v17: offered load becomes a question");
    ScenarioRead r = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 2, "unit": "minutes"}}],
        "steps": [{"name": "Teller", "time": {"pattern": "random", "mean": 3, "unit": "minutes"}}]})"));
    const std::vector<Question> q = loadQuestions(r.scenario);
    check(q.size() == 1, "one server for a 150% load is a question");
    check(q.size() == 1 && contains(q[0].text, "150%") && contains(q[0].text, "At least 2") &&
              q[0].path == "steps[0].servers",
          "...saying how busy, how many would do, and which field the answer fills");

    r.scenario.steps[0].servers = 2;
    check(loadQuestions(r.scenario).empty(), "two servers at 75% is fine");

    const ScenarioRead loop = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 10}}],
        "steps": [{"name": "Make", "time": {"pattern": "fixed", "mean": 5}},
                  {"name": "Inspect", "time": {"pattern": "fixed", "mean": 1}}],
        "branches": [{"from": "Inspect", "to": "Make", "probability": 0.5}]})"));
    const std::vector<StepLoad> loads = offeredLoads(loop.scenario);
    check(loads.size() == 2 && std::abs(loads[0].arrivalRate - 0.2) < 1e-9,
          "a rework loop doubles the traffic through it (1/10 in, half return: 1/5)");
    check(std::abs(loads[0].utilisation - 1.0) < 1e-9 && loadQuestions(loop.scenario).size() == 1,
          "...which puts Make at exactly 100%, and that is a question");

    const ScenarioRead shared = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 1}}],
        "resources": [{"name": "Clerk", "servers": 1}],
        "steps": [{"name": "Intake", "resource": "Clerk", "time": {"pattern": "random", "mean": 0.5}},
                  {"name": "Review", "resource": "Clerk", "time": {"pattern": "random", "mean": 0.7}}]})"));
    const std::vector<Question> sq = loadQuestions(shared.scenario);
    check(sq.size() == 1 && sq[0].path == "resources[0].servers" && contains(sq[0].text, "120%"),
          "a shared resource is loaded by every step that uses it, summed");
}

void testBuilder() {
    section("v17: the scenario builder");
    check(blockNameFor("check in desk") == "CheckInDesk", "words become one name");
    check(blockNameFor("X-ray") == "XRay", "punctuation is dropped");
    check(blockNameFor("2nd inspection") == "B2ndInspection", "a name never starts with a digit");

    TimeSpec t;
    t.pattern = Pattern::Random;
    t.unit = TimeUnit::Seconds;
    t.mean = 30;
    check(expressionFor(t, TimeUnit::Minutes) == "EXPO(0.5)", "30 seconds in a minutes model is EXPO(0.5)");
    t.pattern = Pattern::Between; t.min = 60; t.max = 120;
    check(expressionFor(t, TimeUnit::Minutes) == "UNIF(1, 2)", "between is UNIF");
    t.pattern = Pattern::Around; t.unit = TimeUnit::Minutes; t.mean = 4; t.min.reset(); t.max.reset();
    std::vector<std::string> said;
    check(expressionFor(t, TimeUnit::Minutes, &said, "Wash") == "TRIA(3, 4, 5)",
          "'about 4' with no bounds is a quarter either side");
    check(said.size() == 1 && contains(said[0], "between 3 and 5"), "...and that invention is said out loud");
    t.pattern = Pattern::Fixed;
    said.clear();
    check(expressionFor(t, TimeUnit::Minutes, &said, "Teller") == "CONS(4)" && said.size() == 1 &&
              contains(said[0], "no variation"),
          "a fixed time is said out loud too -- no variation is a strong claim nobody may have made");

    const BuiltScenario b = buildScenario(readScenario(json(kTeller)).scenario);
    const std::string text = writeDocument(b.document);
    check(contains(text, "Interarrival = EXPO(2)") || contains(text, "Interarrival = EXPO(2)\n") ||
              b.document.cell("Create", 0, "Interarrival") == "EXPO(2)",
          "the arrivals become a Create");
    check(b.document.cell("Process", 0, "Capacity") == "2" &&
              b.document.cell("Process", 0, "Service") == "EXPO(3)" &&
              b.document.cell("Process", 0, "Next") == "Exit",
          "the step becomes a Process wired to the exit");
    check(b.document.cell("Run", 0, "Length") == "480", "the run length is in model units");
    const CompileResult c = compile(b.document);
    check(!hasErrors(c.diagnostics), "the built model compiles: " + formatDiagnostics(c.diagnostics));

    std::vector<Diagnostic> problems;
    check(RunController::fromDocument(b.document, problems) != nullptr,
          "...and is accepted for a run, stability check included");

    const ScenarioRead hospital = readScenario(json(R"({
        "arrivals": [{"name": "Patients", "every": {"pattern": "random", "mean": 6}}],
        "steps": [{"name": "Registration", "servers": 1, "time": {"pattern": "between", "min": 2, "max": 4}},
                  {"name": "Doctor", "servers": 3, "time": {"pattern": "around", "mean": 15}, "next": "exit"},
                  {"name": "X-ray", "time": {"pattern": "fixed", "mean": 20}, "next": "exit"}],
        "branches": [{"from": "Doctor", "to": "X-ray", "probability": 0.1}]})"));
    check(hospital.invalid.empty() && hospital.questions.empty(), "the hospital reads cleanly");
    const BuiltScenario h = buildScenario(hospital.scenario);
    check(h.document.cell("Process", 1, "Next") == "DoctorSplit" &&
              h.document.cell("Decide", 0, "Type") == "Chance" &&
              h.document.cell("Decide", 0, "Next") == "Exit" &&
              h.document.cell("DecideBranch", 0, "To") == "XRay" &&
              h.document.cell("DecideBranch", 0, "Probability") == "0.1",
          "a branch becomes a Decide whose else-exit is where the step went before");
    check(!hasErrors(compile(h.document).diagnostics), "the hospital compiles");
    bool saidDay = false, saidServer = false;
    for (const std::string& a : h.assumptions) {
        saidDay = saidDay || contains(a, "8-hour day");
        saidServer = saidServer || contains(a, "X-ray, so it has 1");
    }
    check(saidDay && saidServer, "the default run length and server count are both stated");

    const ScenarioRead shared = readScenario(json(R"({
        "arrivals": [{"every": {"pattern": "random", "mean": 1}, "limit": 200}],
        "resources": [{"name": "Clerk", "servers": 2}],
        "steps": [{"name": "Intake", "resource": "Clerk", "time": {"pattern": "random", "mean": 0.5}},
                  {"name": "Review", "resource": "Clerk", "time": {"pattern": "random", "mean": 0.7}}]})"));
    const BuiltScenario sb = buildScenario(shared.scenario);
    check(sb.document.cell("Resource", 0, "Capacity") == "2" &&
              sb.document.cell("Process", 0, "Resource") == "Clerk" &&
              sb.document.cell("Process", 1, "Resource") == "Clerk",
          "a shared resource is declared once and named by both steps");
    check(sb.document.cell("Run", 0, "Length").empty(),
          "with every stream limited and no length, it runs until they are all served");
    check(!hasErrors(compile(sb.document).diagnostics), "the shared model compiles");
}

void testGenerator() {
    section("v17: the generator, against recorded replies");
    {
        RecordedTransport t;
        t.add(200, reply(kTeller));
        const GenerateResult r = generateModel("customers every 2 minutes, two tellers, 3 minutes each",
                                               JsonValue(), testConfig(), t);
        check(r.ok && r.calls == 1 && r.error.empty(), "a good answer is one call and a model");
        check(t.urls.size() == 1 && t.urls[0] == "https://api.groq.com/openai/v1/chat/completions",
              "the request goes to Groq's OpenAI-compatible endpoint");
        const JsonValue body = json(t.bodies[0]);
        check(body["model"].asString() == "qwen/qwen3.8-27b" &&
                  body["temperature"].asNumber() == 0.0 &&
                  body["response_format"]["type"].asString() == "json_object",
              "Qwen 3.8 27B, temperature 0, JSON mode");
        check(body["reasoning_format"].asString() == "hidden" && !body.has("reasoning_effort"),
              "a Qwen 3 model is sent reasoning_format hidden, which Groq requires with JSON mode");
        check(contains(body["messages"].items()[1]["content"].asString(), "2 tellers"),
              "the model reads the pre-parsed text");
        bool auth = false;
        for (const std::string& h : t.headers[0]) auth = auth || h == "Authorization: Bearer gsk_testkey0123456789abcd";
        check(auth && !contains(t.bodies[0], "gsk_testkey"), "the key is a header and never in the body");
    }
    {
        RecordedTransport t;
        t.add(200, reply(std::string("Here you go:\n```json\n") + kTeller + "\n```"));
        check(generateModel("x", JsonValue(), testConfig(), t).ok, "an answer wrapped in a fence is still used");
    }
    {
        RecordedTransport t;
        t.add(200, reply(R"({"arrivals": [{"every": {"pattern": "often", "mean": 2}}], "steps": []})"));
        t.add(200, reply(kTeller));
        const GenerateResult r = generateModel("x", JsonValue(), testConfig(), t);
        check(r.ok && r.calls == 2, "a malformed answer gets one correction");
        const JsonValue second = json(t.bodies[1]);
        check(second["messages"].items().size() == 4 &&
                  contains(second["messages"].items()[3]["content"].asString(), "arrivals[0].every.pattern"),
              "...which names the field that was wrong");
    }
    {
        RecordedTransport t;
        t.add(200, reply("not json"));
        t.add(200, reply("still not json"));
        const GenerateResult r = generateModel("x", JsonValue(), testConfig(), t);
        check(!r.ok && r.calls == 2 && contains(r.error, "after a correction"),
              "two bad answers stop there rather than spending more");
    }
    {
        RecordedTransport t;
        t.add(200, reply(R"({"arrivals": [{"every": {"pattern": "random", "mean": 2}}],
                              "steps": [{"name": "Teller", "time": {"pattern": "random", "mean": 3}}]})"));
        const GenerateResult r = generateModel("x", JsonValue(), testConfig(), t);
        check(!r.ok && r.error.empty() && r.questions.size() == 1 && contains(r.questions[0].text, "150%"),
              "an overloaded model is a question for the person, not another call");
        check(r.scenarioJson.isObject(), "...and the form is kept so the answer can be merged in");

        RecordedTransport t2;
        const GenerateResult r2 = generateModel("there are two tellers", r.scenarioJson, testConfig(), t2);
        check(r2.ok && r2.calls == 0 && t2.bodies.empty(),
              "a short count answering a server question is filled in by code -- no call at all");
        check(r2.document.cell("Process", 0, "Capacity") == "2", "...into the step that was asked about");
        RecordedTransport t2b;
        check(generateModel("take 4 serverse", r.scenarioJson, testConfig(), t2b).calls == 0,
              "typos around the number do not matter");

        // "every 3 minutes" names a time: that is a change, not a count, and
        // goes to the model with the question it might be answering.
        RecordedTransport t3;
        t3.add(200, reply(R"({"arrivals": [{"every": {"pattern": "random", "mean": 6}}]})"));
        const GenerateResult r3 = generateModel("make it every 6 minutes", r.scenarioJson, testConfig(), t3);
        check(t3.bodies.size() == 1 &&
                  contains(json(t3.bodies[0])["messages"].items()[1]["content"].asString(), "The person was asked"),
              "a longer answer goes to the model with the question it answers");
        check(r3.ok && r3.scenario.steps.size() == 1 && r3.scenario.arrivals.size() == 1 &&
                  r3.scenario.arrivals[0].every && *r3.scenario.arrivals[0].every->mean == 6.0,
              "a reply holding only the changed arrivals is merged -- the steps survive it");
    }
    {
        RecordedTransport t;
        t.add(401, R"({"error": {"message": "Invalid API Key"}})");
        const GenerateResult r = generateModel("x", JsonValue(), testConfig(), t);
        check(!r.ok && contains(r.error, "refused the API key"), "a 401 says the key was refused");
        RecordedTransport t404;
        t404.add(404, R"({"error": {"message": "The model `x` does not exist or you do not have access to it."}})");
        check(contains(generateModel("x", JsonValue(), testConfig(), t404).error, "llm.conf"),
              "a 404 says where the model name is set");
        RecordedTransport t429;
        t429.add(429, "{}");
        check(contains(generateModel("x", JsonValue(), testConfig(), t429).error, "rate-limiting"),
              "a 429 says to wait");
        RecordedTransport t500;
        t500.add(500, R"({"error": {"message": "model overloaded"}})");
        check(contains(generateModel("x", JsonValue(), testConfig(), t500).error, "model overloaded"),
              "another status passes on the provider's own message");
        RecordedTransport none;
        check(!generateModel("x", JsonValue(), testConfig(), none).ok, "no response at all is an error");
    }
    {
        LlmConfig local;
        local.baseUrl = "http://localhost:11434/v1";
        local.model = "llama3.1";
        RecordedTransport t;
        t.add(200, reply(kTeller));
        check(generateModel("x", JsonValue(), local, t).ok && t.headers[0].size() == 1,
              "a local server with no key gets no Authorization header");
        check(!json(t.bodies[0]).has("reasoning_format"),
              "a model that does not reason is not sent reasoning_format, which Groq would refuse");

        LlmConfig set = testConfig();
        set.reasoningFormat = "parsed";
        set.reasoningEffort = "none";
        RecordedTransport t2;
        t2.add(200, reply(kTeller));
        generateModel("x", JsonValue(), set, t2);
        check(json(t2.bodies[0])["reasoning_format"].asString() == "parsed" &&
                  json(t2.bodies[0])["reasoning_effort"].asString() == "none",
              "the settings choose the reasoning format and effort when given");

        RecordedTransport t3;
        t3.add(200, reply(std::string("<think>maybe {\"time_unit\": \"hours\"}? no.</think>\n") + kTeller));
        const GenerateResult thought = generateModel("x", JsonValue(), testConfig(), t3);
        check(thought.ok && thought.scenario.timeUnit == TimeUnit::Minutes,
              "thinking passed through in the content is skipped, braces and all");
        RecordedTransport t4;
        t4.add(200, reply("<think>only thoughts</think>"));
        check(contains(generateModel("x", JsonValue(), testConfig(), t4).error, "only the model's thinking"),
              "a reply that is all thinking says so");
    }

    check(maskKey("gsk_abcdefghijklmnopwxyz") == "gsk_...wxyz", "a key is shown as its prefix and last four");
    check(systemPrompt() == systemPrompt(), "the system prompt is the same on every call, so it can be cached");

    // The worked example inside the prompt is itself a valid scenario. A prompt
    // that teaches a shape the reader rejects would cost a correction per call.
    const std::string& prompt = systemPrompt();
    const std::size_t at = prompt.find("Answer: ");
    const JsonParse example = parseJsonObjectIn(prompt.substr(at + 8));
    const ScenarioRead er = readScenario(example.value);
    check(example.ok && er.invalid.empty() && er.questions.empty(),
          "the prompt's own example reads cleanly");
    check(!hasErrors(compile(buildScenario(er.scenario).document).diagnostics),
          "...and builds a model that compiles");

    const std::string cfg = curlConfig("https://x/y", {"Authorization: Bearer k\"ey"}, "/tmp/b.json", 30);
    check(contains(cfg, "header = \"Authorization: Bearer k\\\"ey\"") &&
              contains(cfg, "data-binary = \"@/tmp/b.json\"") && contains(cfg, "max-time = 30"),
          "curl's config quotes what it is given");
}

}  // namespace

void runScenarioTests() {
    testJson();
    testPreParse();
    testScenarioReader();
    testLoad();
    testBuilder();
    testGenerator();
}
