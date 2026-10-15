// ============================================================================
// gui/Theme.hpp  --  one look on every platform
// ============================================================================
// Qt's Fusion style draws every widget itself rather than asking the operating
// system to, so with one palette and one stylesheet the app looks the same on
// Windows and on a Mac -- which is the requirement: one design, two platforms.
// Every colour the app uses is named here, so the flow graph, the editor and
// the charts cannot drift apart.

#pragma once
#include <QColor>
#include <QString>

class QApplication;

namespace theme {

const QColor background(0xF4, 0xF6, 0xF9);
const QColor surface(0xFF, 0xFF, 0xFF);
const QColor border(0xD5, 0xDB, 0xE3);
const QColor text(0x1F, 0x29, 0x37);
const QColor muted(0x6B, 0x72, 0x80);
const QColor accent(0x25, 0x63, 0xEB);
const QColor accentSoft(0xDB, 0xE7, 0xFF);
const QColor error(0xDC, 0x26, 0x26);
const QColor warning(0xD9, 0x77, 0x06);
const QColor success(0x16, 0xA3, 0x4A);

// The flowchart blocks, one colour per kind, used by the graph and the palette.
QColor blockColor(const QString& type);

void apply(QApplication& app);

}  // namespace theme
