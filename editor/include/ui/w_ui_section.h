//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

// Section-style groupings: bordered drawers with header rows, shared by the
// About-popup sections, the inspector's component drawers, and the console
// log trace drawer. The splitter + border + fill pattern lives in exactly
// one place here.

namespace wiesel::editor::ui::section {

// --- Header flags -----------------------------------------------------------

enum HeaderFlags_ {
  HeaderFlags_None = 0,
  // Non-collapsible: no chevron, always "open", no storage toggle, no TreePush
  // (so callers must NOT call TreePop). Used for section-style headers that
  // shouldn't hide their contents.
  HeaderFlags_NoCollapse = 1 << 0,
};

// --- Header state -----------------------------------------------------------

// State emitted by the last ClosableTreeNode() call on this thread. Outer
// framing code (drawer loop in inspector, or BeginSection internally) reads
// this to know where the header ended so it can position the drawer fill.
struct HeaderState {
  bool header_rendered;
  bool open;
  float header_bottom_y;  // screen-space Y where drawer content starts
};
void ResetHeaderState();
const HeaderState& GetHeaderState();

// Queues an icon glyph (ICON_LC_*) to be prepended to the label of the next
// ClosableTreeNode() call on this thread. Pass nullptr/"" to clear. The
// setting auto-clears after being consumed.
void SetNextHeaderIcon(const char* icon);

// Renders a header row with optional chevron + icon + label + close button,
// used by component drawers and section headers. Returns true when the
// drawer is "open" (always true for HeaderFlags_NoCollapse). `p_visible` may
// be null to hide the close button; when non-null and clicked the pointed-to
// bool is set to false.
bool ClosableTreeNode(const char* label, bool* p_visible, int flags = 0);

// --- Drawer frame (splitter + border + optional fill) -----------------------

// Begin a bordered frame at the current cursor position. The caller renders
// header + body content normally between Begin and End; EndDrawerFrame draws
// the 4-line border (and optional dark fill) around everything that was
// rendered. Uses ImDrawListSplitter under the hood with content on channel
// 1 and borders/fill on channel 0 (so the bg painted at End stays behind the
// already-rendered content).
void BeginDrawerFrame();

// Close the matching BeginDrawerFrame. Draws borders spanning the host
// window's full width from the Begin's top_y down to the current cursor Y.
// When `fill` is true, also fills the rect below `header_bottom_y` with
// style::kDrawerBg (makes the "drawer" look recessed).
//
// After the call the cursor sits on the bottom border line so the next
// BeginDrawerFrame starts flush - adjacent drawers then share a single 1px
// divider instead of a gap.
void EndDrawerFrame(float header_bottom_y, bool fill);

// --- Sections --------------------------------------------------------------

// Begin a bordered, non-collapsible section with an icon + label header.
// Thin wrapper around BeginDrawerFrame + ClosableTreeNode(NoCollapse).
// Adjacent sections share a single 1px divider between their borders.
//
// `icon`: optional ICON_LC_* glyph.
// `fill`: paint style::kDrawerBg under the body. On for static info panels
//         (About popup); off for places where a darker fill would clash with
//         input widget FrameBg.
void BeginSection(const char* label, const char* icon = nullptr,
                  bool fill = false);
void EndSection();

}  // namespace wiesel::editor::ui::section
