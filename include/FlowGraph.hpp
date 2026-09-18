// ============================================================================
// FlowGraph.hpp  --  v16.2: the flowchart as a picture
// ============================================================================
// Graphviz lays the model out; this draws what it laid out, in ASCII for the
// Flow tab and as a PNG for a file. The split is deliberate: dotFor(),
// parsePlain() and drawAscii() are pure text-to-text and are tested without
// Graphviz installed, and only findDot()/runDot() touch the machine.
// ============================================================================

#pragma once
#include <map>
#include <string>
#include <vector>
#include "ModelDocument.hpp"

namespace des {

// The model as Graphviz DOT, left to right. Node names are the block names;
// shapes say the block type; Decide branches carry their labels; a Route's
// trip to its Station is dashed; a Match's lanes share a frame.
std::string dotFor(const ModelDocument& doc);

// What `dot -Tplain` says: sizes and positions in inches, y upwards.
struct PlainNode {
    std::string name;
    std::string label;
    std::string shape;
    double x{0}, y{0}, width{0}, height{0};
};
struct PlainEdge {
    std::string tail, head, label, style;
    std::vector<std::pair<double, double>> points;   // B-spline control points
    double labelX{0}, labelY{0};
    bool hasLabel{false};
};
struct PlainLayout {
    double width{0}, height{0};
    std::vector<PlainNode> nodes;
    std::vector<PlainEdge> edges;
    bool ok() const { return width > 0 && !nodes.empty(); }
};
PlainLayout parsePlain(const std::string& text);

// A box on the drawing, in character cells.
struct CellRect { int left{0}, top{0}, right{0}, bottom{0}; };

struct AsciiGraph {
    std::vector<std::string>        lines;
    std::map<std::string, CellRect> boxes;
};
// 12 columns and 6 rows to the inch, which is what the DOT asks Courier 10pt
// for -- so a block name fits in the box Graphviz sized for it.
AsciiGraph drawAscii(const PlainLayout& layout);

// Where dot is: $DES_DOT, then `dot` on PATH, then the usual install folder.
// Empty when there is none.
std::string findDot();

// Runs dot on `dotText`, writing `format` (plain, png, ...) to `outPath`.
// False, with the reason, when it could not.
bool runDot(const std::string& dotPath, const std::string& dotText,
            const std::string& format, const std::string& outPath, std::string& error);

// Where `P` puts the picture: <model's folder>/images/<model>-flow.png.
std::string flowImagePath(const std::string& modelPath);

}  // namespace des
