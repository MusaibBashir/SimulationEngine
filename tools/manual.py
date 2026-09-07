#!/usr/bin/env python3
"""
tools/manual.py  --  build the two-column PDF reference manual.

    python tools/manual.py [output.pdf]

The API surface is EXTRACTED FROM THE HEADERS, not retyped here: signatures come
from include/*.hpp at build time, so a signature in the manual cannot silently
drift from the one in the code. Only the prose lives in manual_data.py, and any
member without authored prose is reported at the end so the gap is visible
rather than quietly filled with filler.
"""
import io, json, os, re, sys

from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (BaseDocTemplate, Flowable, Frame, KeepTogether,
                                NextPageTemplate, PageBreak, PageTemplate,
                                Paragraph, Spacer, Table, TableStyle)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import manual_data as D                                    # noqa: E402
from manual_extract import parse_headers                   # noqa: E402

# ---------------------------------------------------------------- palette --
INK      = colors.HexColor("#1a1a1a")
MUTED    = colors.HexColor("#5f6b71")
RULE     = colors.HexColor("#c9d1d3")
ACCENT   = colors.HexColor("#1f4e5f")
CODE_BG  = colors.HexColor("#f2f4f5")
BOX_FILL = colors.HexColor("#eef2f4")
BOX_LINE = colors.HexColor("#7d939c")

PAGE_W, PAGE_H = A4
MARGIN, GUTTER = 40, 18
COL_W = (PAGE_W - 2 * MARGIN - GUTTER) / 2.0

# ----------------------------------------------------------------- styles --
def styles():
    s = {}
    s["body"] = ParagraphStyle("body", fontName="Helvetica", fontSize=8.3,
                               leading=11.2, textColor=INK, alignment=TA_JUSTIFY,
                               spaceAfter=4)
    s["lede"] = ParagraphStyle("lede", parent=s["body"], fontSize=8.8, leading=12.4,
                               textColor=colors.HexColor("#33424a"), spaceAfter=7)
    s["h1"] = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=23,
                             leading=26, textColor=ACCENT, spaceAfter=2)
    s["h1n"] = ParagraphStyle("h1n", fontName="Helvetica-Bold", fontSize=8.5,
                              leading=11, textColor=MUTED, spaceAfter=5)
    s["h2"] = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=11,
                             leading=13, textColor=ACCENT, spaceBefore=9,
                             spaceAfter=3, keepWithNext=1)
    s["h3"] = ParagraphStyle("h3", fontName="Helvetica-Bold", fontSize=8.6,
                             leading=11, textColor=INK, spaceBefore=6,
                             spaceAfter=2, keepWithNext=1)
    s["kind"] = ParagraphStyle("kind", fontName="Helvetica-Oblique", fontSize=7.4,
                               leading=9, textColor=MUTED, spaceAfter=3)
    s["sig"] = ParagraphStyle("sig", fontName="Courier-Bold", fontSize=6.9,
                              leading=8.6, textColor=colors.HexColor("#14303a"),
                              spaceBefore=2.5, spaceAfter=0.5)
    s["mdesc"] = ParagraphStyle("mdesc", parent=s["body"], fontSize=7.7,
                                leading=9.9, leftIndent=7, spaceAfter=2.5)
    s["code"] = ParagraphStyle("code", fontName="Courier", fontSize=7.0,
                               leading=9.0, textColor=INK, backColor=CODE_BG,
                               borderPadding=4, spaceBefore=3, spaceAfter=5)
    s["cap"] = ParagraphStyle("cap", fontName="Helvetica-Oblique", fontSize=7.3,
                              leading=9.4, textColor=MUTED, spaceBefore=2,
                              spaceAfter=7)
    s["toc"] = ParagraphStyle("toc", parent=s["body"], fontSize=8.6, leading=12.6,
                              alignment=0, spaceAfter=0)
    return s


# The built-in Helvetica/Courier fonts are WinAnsi, and typographic punctuation
# is the classic way to get a row of black boxes or question marks in a PDF that
# looked fine on screen. Normalise it once, centrally, so the prose can be
# written naturally -- and so this cannot be forgotten in one entry out of six
# hundred.
PUNCT = {
    "—": "--", "–": "-", "‘": "'", "’": "'",
    "“": '"', "”": '"', "·": "-", "…": "...",
    " ": " ", "−": "-",
}


def norm(t):
    for bad, good in PUNCT.items():
        t = t.replace(bad, good)
    return t


def esc(t):
    t = norm(t)
    return (t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


# ------------------------------------------------------------- rule lines --
class Rule(Flowable):
    """A hairline. Used under chapter titles and between type entries."""
    def __init__(self, width, thickness=0.5, colour=RULE, space=3):
        Flowable.__init__(self)
        self.width, self.thickness, self.colour, self.space = width, thickness, colour, space
        self.height = thickness + space

    def draw(self):
        self.canv.setStrokeColor(self.colour)
        self.canv.setLineWidth(self.thickness)
        self.canv.line(0, self.space, self.width, self.space)


# -------------------------------------------------------------- diagrams --
class FlowDiagram(Flowable):
    """A vertical call-flow: boxes joined by labelled arrows.

    Deliberately hand-drawn rather than generated by Graphviz -- the project has
    no dependencies, and a diagram that renders wherever reportlab does is worth
    more than a prettier one that needs a toolchain installed.
    """
    BOX_H, GAP, PAD = 25, 15, 4

    def __init__(self, steps, width):
        Flowable.__init__(self)
        self.steps = steps                 # [(title, subtitle|None, edge|None)]
        self.width = width
        self.height = len(steps) * self.BOX_H + (len(steps) - 1) * self.GAP

    def wrap(self, aw, ah):
        return self.width, self.height

    def draw(self):
        c = self.canv
        y = self.height
        for i, (title, sub, edge) in enumerate(self.steps):
            y -= self.BOX_H
            c.setFillColor(BOX_FILL); c.setStrokeColor(BOX_LINE); c.setLineWidth(0.6)
            c.roundRect(0, y, self.width, self.BOX_H, 2.5, stroke=1, fill=1)
            c.setFillColor(INK); c.setFont("Courier-Bold", 6.9)
            c.drawString(self.PAD + 1, y + (14 if sub else 9), title[:58])
            if sub:
                c.setFillColor(MUTED); c.setFont("Helvetica", 6.2)
                c.drawString(self.PAD + 1, y + 5, sub[:70])
            if i < len(self.steps) - 1:
                ax = self.width / 2.0
                c.setStrokeColor(BOX_LINE); c.setLineWidth(0.8)
                c.line(ax, y, ax, y - self.GAP + 4)
                c.setFillColor(BOX_LINE)
                head = c.beginPath()
                head.moveTo(ax - 2.6, y - self.GAP + 4.5)
                head.lineTo(ax + 2.6, y - self.GAP + 4.5)
                head.lineTo(ax, y - self.GAP)
                head.close()
                c.drawPath(head, stroke=0, fill=1)
                if edge:
                    c.setFillColor(MUTED); c.setFont("Helvetica-Oblique", 5.9)
                    c.drawString(ax + 5, y - self.GAP + 6.5, edge[:52])
            y -= self.GAP


class LayerDiagram(Flowable):
    """Stacked layers, top to bottom, with an optional boundary marker."""
    ROW_H, GAP = 26, 4

    def __init__(self, rows, width):
        Flowable.__init__(self)
        self.rows = rows                   # [(label, detail, is_boundary)]
        self.width = width
        self.height = len(rows) * (self.ROW_H + self.GAP)

    def wrap(self, aw, ah):
        return self.width, self.height

    def draw(self):
        c = self.canv
        y = self.height
        for label, detail, boundary in self.rows:
            y -= self.ROW_H
            if boundary:
                c.setStrokeColor(ACCENT); c.setLineWidth(0.9); c.setDash(3, 2)
                c.line(0, y + self.ROW_H / 2, self.width, y + self.ROW_H / 2)
                c.setDash()
                c.setFillColor(colors.white)
                w = c.stringWidth(label, "Helvetica-Bold", 6.4) + 8
                c.rect((self.width - w) / 2, y + self.ROW_H / 2 - 4, w, 8, stroke=0, fill=1)
                c.setFillColor(ACCENT); c.setFont("Helvetica-Bold", 6.4)
                c.drawCentredString(self.width / 2, y + self.ROW_H / 2 - 2, label)
            else:
                c.setFillColor(BOX_FILL); c.setStrokeColor(BOX_LINE); c.setLineWidth(0.6)
                c.rect(0, y, self.width, self.ROW_H, stroke=1, fill=1)
                c.setFillColor(INK); c.setFont("Helvetica-Bold", 7.4)
                c.drawString(5, y + self.ROW_H - 10, label)
                c.setFillColor(MUTED); c.setFont("Courier", 6.1)
                c.drawString(5, y + 5, detail[:64])
            y -= self.GAP


# ------------------------------------------------------------- page furniture --
def decorate(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(RULE); canvas.setLineWidth(0.4)
    canvas.line(MARGIN, PAGE_H - MARGIN + 12, PAGE_W - MARGIN, PAGE_H - MARGIN + 12)
    canvas.setFont("Helvetica", 7); canvas.setFillColor(MUTED)
    canvas.drawString(MARGIN, PAGE_H - MARGIN + 16, "DES Engine - Reference Manual")
    canvas.drawRightString(PAGE_W - MARGIN, PAGE_H - MARGIN + 16,
                           getattr(doc, "chapter_title", ""))
    canvas.line(MARGIN, MARGIN - 10, PAGE_W - MARGIN, MARGIN - 10)
    canvas.setFont("Helvetica", 7.6); canvas.setFillColor(INK)
    canvas.drawCentredString(PAGE_W / 2.0, MARGIN - 20, str(canvas.getPageNumber()))
    canvas.setFont("Helvetica", 6.4); canvas.setFillColor(MUTED)
    canvas.drawRightString(PAGE_W - MARGIN, MARGIN - 20, norm(D.VERSION_LINE))
    canvas.restoreState()


class Manual(BaseDocTemplate):
    def __init__(self, path):
        BaseDocTemplate.__init__(self, path, pagesize=A4,
                                 leftMargin=MARGIN, rightMargin=MARGIN,
                                 topMargin=MARGIN, bottomMargin=MARGIN,
                                 title="DES Engine — Reference Manual",
                                 author="Musaib", subject="Discrete-event simulation engine")
        top = PAGE_H - 2 * MARGIN
        left = Frame(MARGIN, MARGIN, COL_W, top, id="left",
                     leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        right = Frame(MARGIN + COL_W + GUTTER, MARGIN, COL_W, top, id="right",
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        head_h = 86
        band = Frame(MARGIN, MARGIN + top - head_h, PAGE_W - 2 * MARGIN, head_h,
                     id="band", leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        cl = Frame(MARGIN, MARGIN, COL_W, top - head_h, id="cleft",
                   leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        cr = Frame(MARGIN + COL_W + GUTTER, MARGIN, COL_W, top - head_h, id="cright",
                   leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([
            PageTemplate(id="body", frames=[left, right], onPageEnd=decorate),
            PageTemplate(id="chapter", frames=[band, cl, cr], onPageEnd=decorate),
        ])
        self.chapter_title = ""

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name == "h1":
            self.chapter_title = re.sub(r"<[^>]+>", "", flowable.getPlainText())


# -------------------------------------------------------------- rendering --
def words(ident):
    """maxLengthObserved -> 'max length observed'."""
    s = re.sub(r'(?<!^)(?=[A-Z])', ' ', ident).lower()
    return s.replace("_", " ")


def humanise(sig, type_name):
    """Derive a description from the signature for the plain accessors.

    An accessor's name and return type genuinely do say what it is, so a
    derived line here is honest rather than filler. Anything this cannot
    describe returns None, and manual.py reports it as a real gap instead of
    printing something vague.
    """
    m = re.search(r'([\w~]+)\s*\(([^)]*)\)', sig)
    if not m:
        return None
    name, args = m.group(1), m.group(2).strip()

    if name.startswith("~"):
        return "Destructor."
    if name == type_name:
        return "Constructor."
    if name.startswith("operator"):
        return None

    ret = sig[:m.start(1)].replace("static", "").replace("virtual", "").strip()
    is_const = sig.rstrip().endswith("const")

    if name.startswith("set") and len(name) > 3:
        return f"Sets the {words(name[3:])}."
    if (name.startswith("is") or name.startswith("has")) and ret.startswith("bool"):
        return f"Whether {words(name[2:] if name.startswith('is') else name[3:])}."
    if not args and is_const:
        return f"The {words(name)}."
    if not args and ret.startswith("void"):
        return f"Performs {words(name)}."
    return None


def member_doc(type_name, sig):
    """Authored prose first, then shared prose, then a derived accessor line.
    Only a genuine gap reports False."""
    # `operator>` has a non-word character in its name, so match it explicitly
    # before falling back to an ordinary identifier.
    m = re.search(r'(operator\s*[^\s(]+|[\w~]+)\s*\(', sig)
    name = re.sub(r'\s+', '', m.group(1)) if m else sig
    key = f"{type_name}::{name}"
    if key in D.MEMBER_DOC:
        return D.MEMBER_DOC[key], True
    if name in D.COMMON_DOC:
        return D.COMMON_DOC[name], True
    derived = humanise(sig, type_name)
    if derived:
        return derived, True
    return "See the header for details.", False


def render_type(entry, st, missing):
    """One class/struct/enum: heading, kind line, prose, then every public member."""
    name = entry["name"]
    flow = [Paragraph(esc(name), st["h2"])]
    kind = {"class": "class", "struct": "struct", "enum": "enum class"}[entry["kind"]]
    bases = entry.get("bases", "").strip()
    tag = f"{kind} &middot; {entry['header']}"
    if bases:
        tag += f" &middot; inherits {esc(bases)}"
    flow.append(Paragraph(tag, st["kind"]))
    prose = D.TYPE_DOC.get(name)
    if prose is None:
        missing.append(f"TYPE {name}")
        prose = "Undocumented."
    flow.append(Paragraph(norm(prose), st["body"]))

    if entry["kind"] == "enum" and entry.get("enumerators"):
        vals = ", ".join(entry["enumerators"])
        flow.append(Paragraph("<b>Values</b> &nbsp;" + esc(vals), st["mdesc"]))

    for sig in entry["members"]:
        doc, found = member_doc(name, sig)
        if not found:
            missing.append(f"{name}::{sig}")
        flow.append(Paragraph(esc(sig), st["sig"]))
        flow.append(Paragraph(norm(doc), st["mdesc"]))
    flow.append(Rule(COL_W, 0.4, RULE, 2))
    flow.append(Spacer(1, 4))
    # Keep the heading with at least its prose; long member lists may break.
    return [KeepTogether(flow[:3])] + flow[3:]


def build(out_path):
    api = parse_headers(os.path.join(ROOT, "include"))
    st = styles()
    story, missing = [], []

    # ---- title page -------------------------------------------------------
    story.append(NextPageTemplate("body"))
    story.append(Spacer(1, 120))
    story.append(Paragraph("DES Engine", ParagraphStyle(
        "t", parent=st["h1"], fontSize=34, leading=37, alignment=0)))
    story.append(Paragraph("Reference Manual", ParagraphStyle(
        "t2", parent=st["h1"], fontSize=17, leading=21,
        textColor=MUTED, alignment=0)))
    story.append(Spacer(1, 10))
    story.append(Rule(COL_W, 1.1, ACCENT, 4))
    story.append(Spacer(1, 10))
    story.append(Paragraph(norm(D.FRONT_MATTER), st["lede"]))
    story.append(Spacer(1, 12))
    story.append(Paragraph(norm(D.VERSION_LINE), st["kind"]))

    # ---- contents ---------------------------------------------------------
    story.append(PageBreak())
    story.append(Paragraph("Contents", st["h2"]))
    story.append(Rule(COL_W, 0.6, ACCENT, 3))
    for num, title, blurb, _ in D.CHAPTERS:
        story.append(Paragraph(
            f'<b>{num}. {esc(title)}</b><br/><font size="7.4" color="#5f6b71">{esc(blurb)}</font>',
            st["toc"]))
        story.append(Spacer(1, 4))

    # ---- chapters ---------------------------------------------------------
    for num, title, blurb, items in D.CHAPTERS:
        story.append(NextPageTemplate("chapter"))
        story.append(PageBreak())
        story.append(Paragraph(f"Chapter {num}", st["h1n"]))
        story.append(Paragraph(esc(title), st["h1"]))
        story.append(Rule(PAGE_W - 2 * MARGIN, 1.0, ACCENT, 4))
        story.append(Paragraph(esc(blurb), st["lede"]))
        story.append(NextPageTemplate("body"))

        for item in items:
            kind = item[0]
            if kind == "text":
                story.append(Paragraph(norm(item[1]), st["body"]))
            elif kind == "h3":
                story.append(Paragraph(esc(item[1]), st["h3"]))
            elif kind == "code":
                for line in item[1].strip("\n").split("\n"):
                    story.append(Paragraph(
                        esc(line).replace(" ", "&nbsp;") or "&nbsp;", st["code"]))
            elif kind == "table":
                data = [[Paragraph(f"<b>{esc(c)}</b>", st["mdesc"]) for c in item[1][0]]] + \
                       [[Paragraph(esc(c), st["mdesc"]) for c in row] for row in item[1][1:]]
                t = Table(data, colWidths=[COL_W * w for w in item[2]])
                t.setStyle(TableStyle([
                    ("LINEBELOW", (0, 0), (-1, 0), 0.5, ACCENT),
                    ("LINEBELOW", (0, 1), (-1, -2), 0.25, RULE),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 1),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                    ("TOPPADDING", (0, 0), (-1, -1), 2),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]))
                story.append(t); story.append(Spacer(1, 5))
            elif kind == "diagram":
                story.append(KeepTogether([
                    Paragraph(esc(item[1]), st["h3"]),
                    FlowDiagram(item[2], COL_W),
                    Paragraph(norm(item[3]), st["cap"])]))
            elif kind == "layers":
                story.append(KeepTogether([
                    Paragraph(esc(item[1]), st["h3"]),
                    LayerDiagram(item[2], COL_W),
                    Paragraph(norm(item[3]), st["cap"])]))
            elif kind == "types":
                for tname in item[1]:
                    found = None
                    for hdr, ts in api.items():
                        for t in ts:
                            if t["name"] == tname:
                                found = dict(t, header=hdr)
                    if found is None:
                        missing.append(f"MISSING TYPE IN HEADERS: {tname}")
                        continue
                    story.extend(render_type(found, st, missing))

    Manual(out_path).build(story)

    # Report gaps rather than hiding them.
    declared = {t["name"] for ts in api.values() for t in ts}
    covered = {n for _, _, _, items in D.CHAPTERS for it in items
               if it[0] == "types" for n in it[1]}
    uncovered = sorted(declared - covered)
    print(f"wrote {out_path}")
    print(f"types documented {len(covered)} / {len(declared)}")
    if uncovered:
        print("NOT IN ANY CHAPTER: " + ", ".join(uncovered))
    if missing:
        print(f"members without authored prose: {len(missing)}")
        for m in missing[:15]:
            print("   " + m)


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "DES_Engine_Reference.pdf"))
