// ============================================================================
// flowgraph_tests.cpp  --  v16.2: the Flow tab as a graph. DOT generation and
// the ASCII drawing are tested from text alone; running Graphviz is tested
// only where it is installed, and the fallback to the list everywhere.
// ============================================================================

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des.hpp"
#include "des_ui.hpp"

using namespace des;
using des_test::check;
using des_test::modelPath;
using des_test::section;

namespace {

std::string fileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void setEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value == nullptr ? "" : value);
#else
    if (value == nullptr) unsetenv(name); else setenv(name, value, 1);
#endif
}

// What `dot -Tplain` said for A -> B (labelled), B -> C, and a dashed A -> C.
const char* const PLAIN =
    "graph 1 3.7118 0.97222\n"
    "node A 0.375 0.30556 0.75 0.5 A solid box black lightgrey\n"
    "node B 2.0729 0.72222 0.75 0.5 B solid diamond black lightgrey\n"
    "node C 3.3368 0.25 0.75 0.5 C solid box black lightgrey\n"
    "edge A B 4 0.75541 0.39699 0.98601 0.45453 1.2871 0.52964 1.5418 0.59319 \"p=0.6\" "
    "1.224 0.67805 solid black\n"
    "edge A C 4 0.75455 0.29863 1.2688 0.28889 2.2085 0.2711 2.7975 0.25995 dashed black\n"
    "edge B C 4 2.451 0.58309 2.5632 0.54024 2.6888 0.49224 2.8091 0.4463 solid black\n"
    "stop\n";

bool anyLineHas(const std::vector<std::string>& lines, const std::string& s) {
    for (const std::string& l : lines) if (l.find(s) != std::string::npos) return true;
    return false;
}

}  // namespace

void runFlowGraphTests() {
    section("The model as DOT");
    {
        const ModelDocument doc = readDocument(fileText(modelPath("ride_hailing.des"))).document;
        const std::string dot = dotFor(doc);
        check(dot.rfind("digraph flow {", 0) == 0 && dot.find("rankdir=LR") != std::string::npos,
              "a left-to-right digraph");
        check(dot.find("\"Create_Pax_Suburb\" [shape=house]") != std::string::npos,
              "a Create is a house");
        check(dot.find("\"Choose_Destination\" [shape=diamond]") != std::string::npos,
              "a Decide is a diamond");
        check(dot.find("\"Choose_Destination\" -> \"Trip_to_Downtown\" [label=\"p=0.60\"]") !=
                  std::string::npos,
              "a branch carries its probability");
        check(dot.find("\"Split_By_Zone\" -> \"Wait_Downtown\" [label=\"myZone == 1\"]") !=
                  std::string::npos,
              "and a condition branch its condition");
        check(dot.find("\"Trip_to_Downtown\" -> \"Zone1_Station\" [style=dashed]") !=
                  std::string::npos,
              "a Route's trip to its Station is dashed");
        check(dot.find("subgraph \"cluster_Match_Rider_Driver\"") != std::string::npos &&
                  dot.find("\"Riders_Waiting\";") != std::string::npos,
              "a Match's lanes share its frame");
        check(dot.find("\"Match_Rider_Driver\" [") == std::string::npos,
              "and the Match itself is the frame, not a node");
        check(dot.find("rank=source; \"Create_Pax_Downtown\";") != std::string::npos,
              "every Create starts on the left");

        const std::string odd = dotFor(readDocument(
            "version = 1\n[Create]\nName = A\"B\nInterarrival = 1\nNext = C\n"
            "[Dispose]\nName = C\n").document);
        check(odd.find("\"A\\\"B\"") != std::string::npos, "a quote in a name is escaped");
    }

    section("Drawing Graphviz's layout in characters");
    {
        const PlainLayout layout = parsePlain(PLAIN);
        check(layout.ok() && layout.nodes.size() == 3 && layout.edges.size() == 3,
              "three nodes and three edges read back");
        check(layout.edges[0].hasLabel && layout.edges[0].label == "p=0.6" &&
                  layout.edges[0].points.size() == 4,
              "with the label and the control points");
        check(layout.edges[1].style == "dashed", "and each edge's style");

        const AsciiGraph g = drawAscii(layout);
        check(g.boxes.size() == 3, "a box per node");
        const CellRect& a = g.boxes.at("A");
        const CellRect& c = g.boxes.at("C");
        check(a.left < g.boxes.at("B").left && g.boxes.at("B").left < c.left,
              "left to right as Graphviz placed them");
        const std::string& mid = g.lines[static_cast<std::size_t>((a.top + a.bottom) / 2)];
        check(mid.find('A') != std::string::npos, "the name sits inside its box");
        check(g.lines[static_cast<std::size_t>(a.top)][static_cast<std::size_t>(a.left)] == '+',
              "boxes have corners");
        check(anyLineHas(g.lines, "p=0.6"), "edge labels are drawn");
        check(anyLineHas(g.lines, ">"), "edges end in an arrowhead");
        check(anyLineHas(g.lines, "..."), "a dashed edge is dotted");
        const std::string& bMid =
            g.lines[static_cast<std::size_t>((g.boxes.at("B").top + g.boxes.at("B").bottom) / 2)];
        check(bMid[static_cast<std::size_t>(g.boxes.at("B").left)] == '<',
              "a diamond shows as < >");
        check(flowImagePath("models/ride.des") ==
                  (std::filesystem::path("models") / "images" / "ride-flow.png").string(),
              "the picture goes in an images folder beside the model");
    }

    section("Graphviz, where it is installed");
    {
        const std::string dot = findDot();
        if (dot.empty()) {
            check(true, "Graphviz is not installed here; the round trip is skipped");
        } else {
            const ModelDocument doc = readDocument(fileText(modelPath("decide.des"))).document;
            const std::string out =
                (std::filesystem::temp_directory_path() / "des_flow_test.plain").string();
            std::string why;
            check(runDot(dot, dotFor(doc), "plain", out, why), "dot lays out decide.des");
            const AsciiGraph g = drawAscii(parsePlain(fileText(out)));
            check(g.boxes.count("Sort") == 1 && g.boxes.count("Out") == 1,
                  "and every block is in the drawing");
            std::filesystem::remove(out);
            check(!runDot(dot, "digraph {", "plain", out, why) && !why.empty(),
                  "bad DOT fails with dot's own message");

            const std::string dir = "flowgraph_test_dir";
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir);
            TuiState s = TuiState::fromText(fileText(modelPath("decide.des")), dir + "/decide.des");
            std::string opened;
            s.setOpener([&opened](const std::string& p) { opened = p; return true; });
            handleKey(s, Key::control('f'));
            check(s.flowGraph() != nullptr, "the Flow tab has a graph");
            Screen screen(120, 30);
            render(s, screen);
            check(screen.asText().find("Sort") != std::string::npos &&
                      screen.asText().find("L list") != std::string::npos,
                  "and draws it");
            handleKey(s, Key::character('p'));
            const std::string png = dir + "/images/decide-flow.png";
            check(std::filesystem::exists(png), "P writes images/decide-flow.png");
            check(fileText(png).rfind("\x89PNG", 0) == 0, "a real PNG");
            check(opened == flowImagePath(dir + "/decide.des"), "and opens it");
            handleKey(s, Key::character('l'));
            render(s, screen);
            check(screen.asText().find("Next -> Big") != std::string::npos ||
                      screen.asText().find("-> Big") != std::string::npos,
                  "L shows the list instead");
            std::filesystem::remove_all(dir);
        }
    }

    section("Without Graphviz the Flow tab is the list, and says why");
    {
        setEnv("DES_DOT", "no/such/dot.exe");
        TuiState s = TuiState::fromText(fileText(modelPath("decide.des")), "decide.des");
        handleKey(s, Key::control('f'));
        check(s.flowGraph() == nullptr, "no graph");
        check(s.flowGraphError().find("Graphviz") != std::string::npos, "and a reason naming Graphviz");
        Screen screen(120, 30);
        render(s, screen);
        check(screen.asText().find("-> Big") != std::string::npos, "the list is shown");
        check(screen.asText().find("Graphviz") != std::string::npos, "with the reason under it");
        handleKey(s, Key::character('p'));
        check(s.status().find("Graphviz") != std::string::npos, "and P says why there is no picture");
        setEnv("DES_DOT", nullptr);
    }
}
