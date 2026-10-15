// ============================================================================
// FlowGraph.cpp
// ============================================================================

#include "FlowGraph.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include "TuiState.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace des {

namespace {

constexpr double COLS_PER_INCH = 12.0;
constexpr double ROWS_PER_INCH = 6.0;

std::string dotQuoted(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

const char* shapeFor(const std::string& type) {
    if (type == "Create")    return "house";
    if (type == "Decide")    return "diamond";
    if (type == "Dispose")   return "oval";
    if (type == "Route")     return "cds";
    if (type == "Station")   return "box, style=rounded";
    if (type == "MatchLane") return "box";
    return "box";
}

// "p=0.60 -> Trip" -> {"p=0.60", "Trip"}. Next carries no label: it is the
// plain wire, and labelling every one would bury the ones that say something.
bool splitExit(const std::string& exit, std::string& label, std::string& target) {
    const std::size_t at = exit.rfind(" -> ");
    if (at == std::string::npos) return false;
    label = exit.substr(0, at);
    target = exit.substr(at + 4);
    if (label == "Next") label.clear();
    return true;
}

std::vector<std::string> tokens(const std::string& line) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        if (i >= line.size()) break;
        std::string t;
        if (line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) ++i;
                t += line[i++];
            }
            ++i;
        } else {
            while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
                t += line[i++];
        }
        out.push_back(t);
    }
    return out;
}

double num(const std::string& s) { return std::strtod(s.c_str(), nullptr); }

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool isFile(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}

}  // namespace

std::string dotFor(const ModelDocument& doc) {
    const std::vector<FlowBlock> blocks = flowOf(doc);
    std::ostringstream os;
    os << "digraph flow {\n"
          "  rankdir=LR; newrank=true; nodesep=0.3; ranksep=0.45;\n"
          "  node [shape=box, fontname=\"Courier\", fontsize=10, height=0.5, "
          "margin=\"0.08,0.04\"];\n"
          "  edge [fontname=\"Courier\", fontsize=9];\n";

    // Lanes are drawn inside their Match's frame; the Match itself is the
    // frame, not a node, because nothing is routed to it.
    std::map<std::string, std::vector<std::string>> lanesOf;
    for (std::size_t r = 0; r < doc.rowCount("MatchLane"); ++r)
        lanesOf[doc.cell("MatchLane", r, "Match")].push_back(doc.cell("MatchLane", r, "Name"));

    for (const FlowBlock& b : blocks) {
        if (b.type == "Match" || b.type == "MatchLane" || b.name.empty()) continue;
        os << "  " << dotQuoted(b.name) << " [shape=" << shapeFor(b.type) << "];\n";
    }
    // Every source on the left, however the flow loops back on itself.
    os << "  { rank=source;";
    for (const FlowBlock& b : blocks)
        if (b.type == "Create" && !b.name.empty()) os << " " << dotQuoted(b.name) << ";";
    os << " }\n";
    for (const auto& kv : lanesOf) {
        os << "  subgraph " << dotQuoted("cluster_" + kv.first) << " {\n"
           << "    label=" << dotQuoted(kv.first) << "; style=rounded;\n";
        for (const std::string& lane : kv.second) os << "    " << dotQuoted(lane) << ";\n";
        os << "  }\n";
    }
    for (const FlowBlock& b : blocks) {
        if (b.type == "Match" || b.name.empty()) continue;
        for (const std::string& e : b.exits) {
            std::string label, target;
            if (!splitExit(e, label, target) || target.empty()) continue;
            os << "  " << dotQuoted(b.name) << " -> " << dotQuoted(target);
            std::vector<std::string> attrs;
            if (b.type == "Route" && label == "to") attrs.push_back("style=dashed");
            else if (!label.empty()) attrs.push_back("label=" + dotQuoted(label));
            if (!attrs.empty()) {
                os << " [";
                for (std::size_t i = 0; i < attrs.size(); ++i) os << (i ? ", " : "") << attrs[i];
                os << "]";
            }
            os << ";\n";
        }
    }
    os << "}\n";
    return os.str();
}

PlainLayout parsePlain(const std::string& text) {
    PlainLayout out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const std::vector<std::string> t = tokens(line);
        if (t.empty()) continue;
        if (t[0] == "graph" && t.size() >= 4) {
            out.width = num(t[2]);
            out.height = num(t[3]);
        } else if (t[0] == "node" && t.size() >= 9) {
            PlainNode n;
            n.name = t[1];
            n.x = num(t[2]); n.y = num(t[3]);
            n.width = num(t[4]); n.height = num(t[5]);
            n.label = t[6];
            n.shape = t[8];
            out.nodes.push_back(n);
        } else if (t[0] == "edge" && t.size() >= 4) {
            PlainEdge e;
            e.tail = t[1];
            e.head = t[2];
            const std::size_t n = static_cast<std::size_t>(std::atoi(t[3].c_str()));
            std::size_t i = 4;
            for (std::size_t k = 0; k < n && i + 1 < t.size(); ++k, i += 2)
                e.points.emplace_back(num(t[i]), num(t[i + 1]));
            const std::size_t rest = t.size() - std::min(i, t.size());
            if (rest >= 5) {
                e.label = t[i];
                e.labelX = num(t[i + 1]);
                e.labelY = num(t[i + 2]);
                e.hasLabel = true;
                e.style = t[i + 3];
            } else if (rest >= 1) {
                e.style = t[i];
            }
            out.edges.push_back(e);
        }
    }
    return out;
}

AsciiGraph drawAscii(const PlainLayout& layout) {
    AsciiGraph g;
    if (!layout.ok()) return g;
    const int width = static_cast<int>(std::ceil(layout.width * COLS_PER_INCH)) + 3;
    const int height = static_cast<int>(std::ceil(layout.height * ROWS_PER_INCH)) + 3;
    std::vector<std::string> canvas(static_cast<std::size_t>(height),
                                    std::string(static_cast<std::size_t>(width), ' '));
    const auto col = [&](double x) { return static_cast<int>(std::lround(x * COLS_PER_INCH)) + 1; };
    const auto row = [&](double y) {
        return static_cast<int>(std::lround((layout.height - y) * ROWS_PER_INCH)) + 1;
    };
    const auto put = [&](int c, int r, char ch) {
        if (r >= 0 && r < height && c >= 0 && c < width)
            canvas[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = ch;
    };
    const auto at = [&](int c, int r) {
        if (r >= 0 && r < height && c >= 0 && c < width)
            return canvas[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
        return ' ';
    };

    // 1. Edges, sampled along each cubic Bezier piece of the spline.
    struct Arrow { int c, r; char ch; };
    std::vector<Arrow> arrows;
    for (const PlainEdge& e : layout.edges) {
        if (e.points.size() < 2) continue;
        std::vector<std::pair<int, int>> cells;
        const auto add = [&](double x, double y) {
            const std::pair<int, int> c{col(x), row(y)};
            if (cells.empty() || cells.back() != c) cells.push_back(c);
        };
        for (std::size_t s = 0; s + 3 < e.points.size(); s += 3) {
            const auto& p0 = e.points[s];
            const auto& p1 = e.points[s + 1];
            const auto& p2 = e.points[s + 2];
            const auto& p3 = e.points[s + 3];
            // Enough samples that no cell is skipped however long the piece.
            const double span = (std::abs(p3.first - p0.first) + std::abs(p1.first - p0.first) +
                                 std::abs(p2.first - p1.first)) * COLS_PER_INCH +
                                (std::abs(p3.second - p0.second) +
                                 std::abs(p1.second - p0.second) +
                                 std::abs(p2.second - p1.second)) * ROWS_PER_INCH;
            const int steps = std::max(16, static_cast<int>(span * 3));
            for (int k = 0; k <= steps; ++k) {
                const double u = static_cast<double>(k) / steps, v = 1.0 - u;
                add(v * v * v * p0.first + 3 * v * v * u * p1.first + 3 * v * u * u * p2.first +
                        u * u * u * p3.first,
                    v * v * v * p0.second + 3 * v * v * u * p1.second +
                        3 * v * u * u * p2.second + u * u * u * p3.second);
            }
        }
        if (e.points.size() < 4) for (const auto& p : e.points) add(p.first, p.second);
        const bool dashed = e.style == "dashed" || e.style == "dotted";
        for (std::size_t i = 1; i < cells.size(); ++i) {
            // The direction over a few cells, not one step: a shallow slope
            // stepped cell by cell reads as noise ("|--/--"), measured over
            // five it reads as the line it is. A row is two columns tall.
            const std::size_t a = i >= 2 ? i - 2 : 0;
            const std::size_t b = std::min(cells.size() - 1, i + 2);
            const int dx = cells[b].first - cells[a].first;
            const int dy = 2 * (cells[b].second - cells[a].second);
            char ch;
            if (std::abs(dx) > 2 * std::abs(dy)) ch = '-';
            else if (std::abs(dy) > 2 * std::abs(dx)) ch = '|';
            else ch = (dx > 0) == (dy < 0) ? '/' : '\\';
            // A slope crosses two columns per row; one slash per row reads as
            // a line, two read as a thick smear ("\\\\").
            if ((ch == '/' || ch == '\\') && cells[i].second == cells[i - 1].second) ch = '-';
            if (dashed) ch = dx == 0 ? ':' : '.';
            // Where two edges cross, say so rather than letting one win.
            const char was = at(cells[i].first, cells[i].second);
            if ((was == '-' && ch == '|') || (was == '|' && ch == '-')) ch = '+';
            put(cells[i].first, cells[i].second, ch);
        }
        if (cells.size() >= 2) {
            const auto& last = cells.back();
            const auto& prev = cells[cells.size() - 2];
            const int dx = last.first - prev.first, dy = last.second - prev.second;
            const char ch = std::abs(dx) >= std::abs(dy) ? (dx >= 0 ? '>' : '<')
                                                         : (dy > 0 ? 'v' : '^');
            arrows.push_back({last.first, last.second, ch});
        }
    }

    // 2. Edge labels.
    for (const PlainEdge& e : layout.edges) {
        if (!e.hasLabel) continue;
        const int c = col(e.labelX) - static_cast<int>(e.label.size()) / 2;
        const int r = row(e.labelY);
        for (std::size_t i = 0; i < e.label.size(); ++i) put(c + static_cast<int>(i), r, e.label[i]);
    }

    // 3. Boxes last, so no edge or label runs through a name.
    for (const PlainNode& n : layout.nodes) {
        CellRect b;
        b.left = col(n.x - n.width / 2);
        b.right = std::max(b.left + 2, col(n.x + n.width / 2));
        b.top = row(n.y + n.height / 2);
        b.bottom = std::max(b.top + 2, row(n.y - n.height / 2));
        for (int r = b.top; r <= b.bottom; ++r)
            for (int c = b.left; c <= b.right; ++c) {
                const bool edgeRow = r == b.top || r == b.bottom;
                const bool edgeCol = c == b.left || c == b.right;
                put(c, r, edgeRow && edgeCol ? '+' : edgeRow ? '-' : edgeCol ? '|' : ' ');
            }
        const int mid = (b.top + b.bottom) / 2;
        if (n.shape == "diamond") { put(b.left, mid, '<'); put(b.right, mid, '>'); }
        if (n.shape == "house" || n.shape == "oval" || n.shape == "circle" ||
            n.shape == "ellipse") {
            put(b.left, mid, '(');
            put(b.right, mid, ')');
        }
        const int room = b.right - b.left - 1;
        std::string label = n.name;
        if (static_cast<int>(label.size()) > room && room > 1)
            label = label.substr(0, static_cast<std::size_t>(room - 1)) + "~";
        const int start = b.left + 1 + std::max(0, (room - static_cast<int>(label.size())) / 2);
        for (std::size_t i = 0; i < label.size() && static_cast<int>(i) < room; ++i)
            put(start + static_cast<int>(i), mid, label[i]);
        g.boxes[n.name] = b;
    }

    // 4. Arrowheads, wherever a box did not claim the cell.
    for (const Arrow& a : arrows) {
        const char was = at(a.c, a.r);
        if (was == '+' || was == '|' || was == '(' || was == ')' || was == '<' ||
            (was == '-' && (a.ch == 'v' || a.ch == '^')))
            continue;
        put(a.c, a.r, a.ch);
    }

    for (std::string& l : canvas) {
        while (!l.empty() && l.back() == ' ') l.pop_back();
    }
    g.lines = std::move(canvas);
    return g;
}

std::string findDot() {
    if (const char* forced = std::getenv("DES_DOT"))
        return isFile(forced) ? std::string(forced) : std::string();
#ifdef _WIN32
    const char* exe = "dot.exe";
    const char sep = ';';
#else
    const char* exe = "dot";
    const char sep = ':';
#endif
    if (const char* path = std::getenv("PATH")) {
        std::string dirs = path;
        std::size_t start = 0;
        while (start <= dirs.size()) {
            const std::size_t end = dirs.find(sep, start);
            const std::string dir = dirs.substr(start, end == std::string::npos ? std::string::npos
                                                                                 : end - start);
            if (!dir.empty() && isFile(std::filesystem::path(dir) / exe))
                return (std::filesystem::path(dir) / exe).string();
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
#ifdef _WIN32
    for (const char* base : {"C:\\Program Files\\Graphviz\\bin\\dot.exe",
                             "C:\\Program Files (x86)\\Graphviz\\bin\\dot.exe"})
        if (isFile(base)) return base;
#else
    for (const char* base : {"/usr/bin/dot", "/usr/local/bin/dot", "/opt/homebrew/bin/dot"})
        if (isFile(base)) return base;
#endif
    return std::string();
}

bool runDot(const std::string& dotPath, const std::string& dotText, const std::string& format,
            const std::string& outPath, std::string& error) {
    if (dotPath.empty()) { error = "Graphviz (dot) is not installed"; return false; }
    std::error_code ec;
    const std::filesystem::path tmp = std::filesystem::temp_directory_path(ec);
    static int serial = 0;
    const std::string stem = "des_flow_" + std::to_string(++serial);
    const std::string in = (tmp / (stem + ".dot")).string();
    const std::string err = (tmp / (stem + ".err")).string();
    {
        std::ofstream o(in, std::ios::binary);
        o << dotText;
        if (!o.good()) { error = "could not write " + in; return false; }
    }
    std::filesystem::remove(outPath, ec);
#ifdef _WIN32
    // dot runs on a hidden console of its OWN. Going through cmd.exe on this
    // console -- which std::system does -- can reset the raw keyboard mode the
    // terminal UI runs in, and every key after it would arrive as a line.
    std::string line = "\"" + dotPath + "\" -T" + format + " -o \"" + outPath + "\" \"" + in + "\"";
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE errFile = CreateFileA(err.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = errFile;
    si.hStdError = errFile;
    PROCESS_INFORMATION pi{};
    int status = -1;
    if (CreateProcessA(nullptr, &line[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                       nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        status = static_cast<int>(code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (errFile != INVALID_HANDLE_VALUE) CloseHandle(errFile);
#else
    const auto q = [](const std::string& s) {
        std::string out = "'";
        for (const char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
        return out + "'";
    };
    const std::string cmd = q(dotPath) + " -T" + format + " -o " + q(outPath) + " " + q(in) +
                            " 2>" + q(err);
    const int status = std::system(cmd.c_str());
#endif
    const std::string why = readAll(err);
    std::filesystem::remove(in, ec);
    std::filesystem::remove(err, ec);
    if (status != 0 || !isFile(outPath)) {
        error = why.empty() ? "dot failed (exit " + std::to_string(status) + ")" : why;
        while (!error.empty() && (error.back() == '\n' || error.back() == '\r')) error.pop_back();
        return false;
    }
    return true;
}

std::string flowImagePath(const std::string& modelPath) {
    const std::filesystem::path p(modelPath);
    std::string stem = p.stem().string();
    if (stem.empty()) stem = "model";
    std::filesystem::path out = p.parent_path() / "images" / (stem + "-flow.png");
    return out.make_preferred().string();
}

}  // namespace des
