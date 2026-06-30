//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_input_ui.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>

#include "input/w_input.h"
#include "ui/w_ui_button.h"
#include "ui/w_ui_chip.h"
#include "ui/w_ui_draw.h"
#include "ui/w_ui_field.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_popup.h"
#include "ui/w_ui_row.h"
#include "ui/w_ui_section.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_lucide.h"
#include "util/w_gamepadcodes.h"
#include "util/w_keycodes.h"
#include "w_engine.h"

namespace wiesel::editor {
namespace {

namespace button = ui::button;
namespace chip_ns = ui::chip;
namespace draw = ui::draw;
namespace field = ui::field;
namespace layout = ui::layout;
namespace row = ui::row;
namespace section = ui::section;
namespace style = ui::style;

// Right-align the cursor inside the current content region, leaving `w`
// pixels of width for the widget about to render. Works correctly inside
// bordered children (unlike layout::RightAlignCursorX which is
// window-width based). Call after SameLine() to stay on the current row.
void RightAlignInChild(float w) {
  const float avail = ImGui::GetContentRegionAvail().x;
  if (avail > w) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - w);
  }
}

// --- Persistent UI state (across frames) -----------------------------------

enum class BindSlot {
  None,
  ActionKey,
  ActionButton,
  AxisPosKey,
  AxisNegKey,
};

struct BindRequest {
  BindSlot slot = BindSlot::None;
  int index = -1;              // action idx or axis idx in the target context
  std::string context_name;    // which context owns the target list
};

struct InputUIState {
  // Inline rename of a context. Empty = not renaming. The value is the
  // original name being edited; the new name lives in `rename_buf`.
  std::string rename_target;
  char rename_buf[64]{};

  // Inline "add context" row in the sidebar. On Enter we commit + reset.
  bool add_context_mode = false;
  char add_context_buf[64]{};

  // Active binding-capture popup state.
  BindRequest bind;

  // Previous-frame raw key/button state used for edge detection while the
  // bind popup is open. Primed on popup open with the keys currently held
  // so "keys already pressed" don't fire a spurious capture.
  std::array<bool, 350> prev_key_pressed{};
  std::array<std::array<bool, GamepadButtonCount>, 8> prev_btn_pressed{};
};

InputUIState& State() {
  static InputUIState s;
  return s;
}

// --- Capture helpers -------------------------------------------------------

void PrimeCaptureSnapshot() {
  for (auto& v : State().prev_key_pressed) {
    v = false;
  }
  for (auto& gp : State().prev_btn_pressed) {
    gp.fill(false);
  }
  for (KeyCode code : GetAllKeyCodes()) {
    if (code >= 0 &&
        code < static_cast<KeyCode>(State().prev_key_pressed.size())) {
      State().prev_key_pressed[code] = Engine::input().IsKeyPressed(code);
    }
  }
  const int gp_count = Engine::input().GetConnectedGamepadCount();
  for (int gp = 0; gp < gp_count && gp < 8; gp++) {
    for (int b = 0; b < GamepadButtonCount; b++) {
      State().prev_btn_pressed[gp][b] =
          Engine::input().IsGamepadButtonPressed(gp, b);
    }
  }
}

// Edge-triggered key capture. Returns KeyUnknown when no press edge this
// frame. Escape is reserved for popup-cancel and never returned.
KeyCode CaptureKey() {
  KeyCode captured = KeyUnknown;
  for (KeyCode code : GetAllKeyCodes()) {
    if (code < 0 ||
        code >= static_cast<KeyCode>(State().prev_key_pressed.size())) {
      continue;
    }
    const bool now = Engine::input().IsKeyPressed(code);
    const bool prev = State().prev_key_pressed[code];
    if (now && !prev && captured == KeyUnknown && code != KeyEscape) {
      captured = code;
    }
    State().prev_key_pressed[code] = now;
  }
  return captured;
}

GamepadButton CaptureGamepadButton() {
  GamepadButton captured = -1;
  const int gp_count = Engine::input().GetConnectedGamepadCount();
  for (int gp = 0; gp < gp_count && gp < 8; gp++) {
    for (int b = 0; b < GamepadButtonCount; b++) {
      const bool now = Engine::input().IsGamepadButtonPressed(gp, b);
      const bool prev = State().prev_btn_pressed[gp][b];
      if (now && !prev && captured < 0) {
        captured = b;
      }
      State().prev_btn_pressed[gp][b] = now;
    }
  }
  return captured;
}

bool IsKeySlot(BindSlot s) {
  return s == BindSlot::ActionKey || s == BindSlot::AxisPosKey ||
         s == BindSlot::AxisNegKey;
}

void OpenBindPopup(BindSlot slot, int index, const std::string& ctx_name) {
  State().bind.slot = slot;
  State().bind.index = index;
  State().bind.context_name = ctx_name;
  PrimeCaptureSnapshot();
  ImGui::OpenPopup("##bind_capture_popup");
}

// Resolve the target binding list based on BindSlot + index, or nullptr if
// the context or index is stale.
std::vector<int32_t>* ResolveKeyTarget(InputSettings& input) {
  auto it = input.contexts.find(State().bind.context_name);
  if (it == input.contexts.end()) {
    return nullptr;
  }
  InputContext& ctx = it->second;
  const int idx = State().bind.index;
  switch (State().bind.slot) {
    case BindSlot::ActionKey:
      if (idx >= 0 && idx < (int)ctx.actions.size()) {
        return &ctx.actions[idx].keys;
      }
      break;
    case BindSlot::AxisPosKey:
      if (idx >= 0 && idx < (int)ctx.axes.size()) {
        return &ctx.axes[idx].positive_keys;
      }
      break;
    case BindSlot::AxisNegKey:
      if (idx >= 0 && idx < (int)ctx.axes.size()) {
        return &ctx.axes[idx].negative_keys;
      }
      break;
    default:
      break;
  }
  return nullptr;
}

std::vector<int32_t>* ResolveButtonTarget(InputSettings& input) {
  if (State().bind.slot != BindSlot::ActionButton) {
    return nullptr;
  }
  auto it = input.contexts.find(State().bind.context_name);
  if (it == input.contexts.end()) {
    return nullptr;
  }
  InputContext& ctx = it->second;
  const int idx = State().bind.index;
  if (idx >= 0 && idx < (int)ctx.actions.size()) {
    return &ctx.actions[idx].buttons;
  }
  return nullptr;
}

// Append `v` to `list` if not already present. Returns true if appended.
bool PushUnique(std::vector<int32_t>& list, int32_t v) {
  if (std::find(list.begin(), list.end(), v) != list.end()) {
    return false;
  }
  list.push_back(v);
  return true;
}

// --- Bind capture popup ----------------------------------------------------

bool RenderBindPopup(InputSettings& input) {
  if (State().bind.slot == BindSlot::None &&
      !ImGui::IsPopupOpen("##bind_capture_popup")) {
    return false;
  }

  bool changed = false;

  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ui::popup::kModalFlags | ImGuiWindowFlags_AlwaysAutoResize;

  const bool visible = ImGui::BeginPopupModal("##bind_capture_popup", nullptr,
                                              flags);
  if (!visible) {
    State().bind.slot = BindSlot::None;
    return false;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    State().bind.slot = BindSlot::None;
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return false;
  }

  const bool is_key = IsKeySlot(State().bind.slot);

  const ImGuiStyle& s = ImGui::GetStyle();
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  // Title strip
  ImGui::Indent(s.WindowPadding.x);
  ImGui::SetWindowFontScale(style::kHeaderFontScale);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", is_key ? ICON_LC_KEYBOARD : ICON_LC_GAMEPAD_2);
  ImGui::SameLine();
  ImGui::TextUnformatted(is_key ? "Press any key..." : "Press any button...");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Unindent(s.WindowPadding.x);

  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));
  layout::Separator();
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::Indent(s.WindowPadding.x);
  ImGui::TextDisabled(
      "Or pick from the list below. Esc to cancel.");
  ImGui::Spacing();

  // Live capture poll
  if (is_key) {
    const KeyCode code = CaptureKey();
    if (code != KeyUnknown) {
      if (auto* t = ResolveKeyTarget(input)) {
        if (PushUnique(*t, code)) {
          changed = true;
        }
      }
      State().bind.slot = BindSlot::None;
      ImGui::CloseCurrentPopup();
      ImGui::Unindent(s.WindowPadding.x);
      ImGui::EndPopup();
      return changed;
    }
  } else {
    const GamepadButton btn = CaptureGamepadButton();
    if (btn >= 0) {
      if (auto* t = ResolveButtonTarget(input)) {
        if (PushUnique(*t, btn)) {
          changed = true;
        }
      }
      State().bind.slot = BindSlot::None;
      ImGui::CloseCurrentPopup();
      ImGui::Unindent(s.WindowPadding.x);
      ImGui::EndPopup();
      return changed;
    }
  }

  // Searchable fallback list
  static char search_buf[64]{};
  if (ImGui::IsWindowAppearing()) {
    search_buf[0] = '\0';
  }
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##bind_search", "Search...", search_buf,
                           sizeof(search_buf));

  std::string filter = search_buf;
  std::transform(filter.begin(), filter.end(), filter.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  ImGui::BeginChild("##bind_list", ImVec2(0, 240));
  if (is_key) {
    for (KeyCode code : GetAllKeyCodes()) {
      if (code == KeyEscape) {
        continue;
      }
      const char* label = KeyCodeToString(code);
      std::string lower = label;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (!filter.empty() && lower.find(filter) == std::string::npos) {
        continue;
      }
      if (ImGui::Selectable(label)) {
        if (auto* t = ResolveKeyTarget(input)) {
          if (PushUnique(*t, code)) {
            changed = true;
          }
        }
        State().bind.slot = BindSlot::None;
        ImGui::CloseCurrentPopup();
      }
    }
  } else {
    for (GamepadButton btn : GetAllGamepadButtons()) {
      const char* label = GamepadButtonToString(btn);
      std::string lower = label;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (!filter.empty() && lower.find(filter) == std::string::npos) {
        continue;
      }
      if (ImGui::Selectable(label)) {
        if (auto* t = ResolveButtonTarget(input)) {
          if (PushUnique(*t, btn)) {
            changed = true;
          }
        }
        State().bind.slot = BindSlot::None;
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::EndChild();

  ImGui::Unindent(s.WindowPadding.x);
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::EndPopup();
  return changed;
}

// --- Chip row ---------------------------------------------------------------

// Render a row of binding chips + an "Add" ghost button. Returns true if
// the list was mutated (chip removed). OpenBindPopup is called directly
// when Add is clicked.
bool RenderChipRow(const char* label, std::vector<int32_t>& list,
                   const char* (*to_string)(int32_t),
                   const char* add_label, BindSlot slot, int item_index,
                   const std::string& ctx_name) {
  ImGui::PushID(label);
  ImGui::AlignTextToFramePadding();
  const float label_w = 70.0f;
  ImGui::TextUnformatted(label);
  ImGui::SameLine(label_w);

  bool changed = false;
  int to_remove = -1;
  for (int i = 0; i < (int)list.size(); i++) {
    ImGui::PushID(i);
    if (i > 0) {
      ImGui::SameLine();
    }
    if (chip_ns::Chip(to_string(list[i]))) {
      to_remove = i;
    }
    ImGui::PopID();
  }
  if (to_remove >= 0) {
    list.erase(list.begin() + to_remove);
    changed = true;
  }

  if (!list.empty()) {
    ImGui::SameLine();
  }
  if (chip_ns::AddChipButton(add_label)) {
    OpenBindPopup(slot, item_index, ctx_name);
  }

  ImGui::PopID();
  return changed;
}

// --- Action / Axis cards ---------------------------------------------------

bool RenderActionCard(InputAction& action, int idx,
                      const std::string& ctx_name, bool& want_delete) {
  bool changed = false;
  ImGui::PushID(idx);

  section::BeginDrawerFrame();

  const ImGuiStyle& s = ImGui::GetStyle();
  ImGui::Dummy(ImVec2(0.0f, s.FramePadding.y));

  // Header row: name input + delete button (right-aligned inside the card).
  ImGui::Indent(s.WindowPadding.x);
  const float delete_w = ImGui::GetFrameHeight();
  const float inner_right_pad = s.WindowPadding.x;
  const float input_w = ImGui::GetContentRegionAvail().x - delete_w -
                        s.ItemSpacing.x - inner_right_pad;
  ImGui::SetNextItemWidth(input_w);
  char buf[128];
  std::strncpy(buf, action.name.c_str(), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  if (ImGui::InputText("##name", buf, sizeof(buf))) {
    action.name = buf;
    changed = true;
  }
  ImGui::SameLine();
  if (button::ToolbarButton(ICON_LC_TRASH_2 "##del")) {
    want_delete = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Delete action");
  }
  ImGui::Unindent(s.WindowPadding.x);

  const float header_bottom_y = ImGui::GetCursorScreenPos().y;

  ImGui::Dummy(ImVec2(0.0f, s.FramePadding.y));

  // Body: Keyboard + Gamepad chip rows
  ImGui::Indent(s.WindowPadding.x);
  changed |= RenderChipRow(
      "Keyboard", action.keys,
      [](int32_t v) { return KeyCodeToString(v); },
      ICON_LC_PLUS "  Add Key", BindSlot::ActionKey, idx, ctx_name);
  ImGui::Spacing();
  changed |= RenderChipRow(
      "Gamepad", action.buttons,
      [](int32_t v) { return GamepadButtonToString(v); },
      ICON_LC_PLUS "  Add Button", BindSlot::ActionButton, idx, ctx_name);
  ImGui::Unindent(s.WindowPadding.x);

  section::EndDrawerFrame(header_bottom_y, /*fill=*/true);

  ImGui::PopID();
  return changed;
}

bool RenderAxisCard(InputAxisMapping& axis, int idx,
                    const std::string& ctx_name, bool& want_delete) {
  bool changed = false;
  ImGui::PushID(idx);

  section::BeginDrawerFrame();

  const ImGuiStyle& s = ImGui::GetStyle();
  ImGui::Dummy(ImVec2(0.0f, s.FramePadding.y));

  // Header: name input + delete (right-aligned inside the card).
  ImGui::Indent(s.WindowPadding.x);
  const float delete_w = ImGui::GetFrameHeight();
  const float inner_right_pad = s.WindowPadding.x;
  const float input_w = ImGui::GetContentRegionAvail().x - delete_w -
                        s.ItemSpacing.x - inner_right_pad;
  ImGui::SetNextItemWidth(input_w);
  char buf[128];
  std::strncpy(buf, axis.name.c_str(), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  if (ImGui::InputText("##name", buf, sizeof(buf))) {
    axis.name = buf;
    changed = true;
  }
  ImGui::SameLine();
  if (button::ToolbarButton(ICON_LC_TRASH_2 "##del")) {
    want_delete = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Delete axis");
  }
  ImGui::Unindent(s.WindowPadding.x);
  const float header_bottom_y = ImGui::GetCursorScreenPos().y;

  ImGui::Dummy(ImVec2(0.0f, s.FramePadding.y));

  ImGui::Indent(s.WindowPadding.x);
  // Chip rows: positive, negative
  changed |= RenderChipRow(
      "Positive", axis.positive_keys,
      [](int32_t v) { return KeyCodeToString(v); },
      ICON_LC_PLUS "  Add Key", BindSlot::AxisPosKey, idx, ctx_name);
  ImGui::Spacing();
  changed |= RenderChipRow(
      "Negative", axis.negative_keys,
      [](int32_t v) { return KeyCodeToString(v); },
      ICON_LC_PLUS "  Add Key", BindSlot::AxisNegKey, idx, ctx_name);
  ImGui::Spacing();

  // Stick combo + Invert toggle on a single row, laid out responsively.
  // Combo takes all remaining space minus the invert checkbox width.
  constexpr float kLabelW = 70.0f;
  {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Stick");
    ImGui::SameLine(kLabelW);

    const ImVec2 invert_sz = ImGui::CalcTextSize("Invert");
    const float invert_w =
        ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
        invert_sz.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float combo_w =
        ImMax(80.0f, avail - invert_w - ImGui::GetStyle().ItemSpacing.x);

    const char* stick_label =
        axis.gamepad_axis >= 0 ? GamepadAxisToString(axis.gamepad_axis)
                               : "None";
    ImGui::SetNextItemWidth(combo_w);
    if (ImGui::BeginCombo("##stick", stick_label)) {
      if (ImGui::Selectable("None", axis.gamepad_axis < 0)) {
        axis.gamepad_axis = -1;
        changed = true;
      }
      for (auto ga : GetAllGamepadAxes()) {
        if (ImGui::Selectable(GamepadAxisToString(ga),
                              axis.gamepad_axis == ga)) {
          axis.gamepad_axis = ga;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Invert", &axis.invert_axis)) {
      changed = true;
    }
  }

  ImGui::Spacing();

  // Smooth row: compact checkbox. When enabled, Gravity + Sensitivity
  // appear on a sub-row sharing the remaining width (half each).
  {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Smooth");
    ImGui::SameLine(kLabelW);
    if (ImGui::Checkbox("##smooth", &axis.smooth)) {
      changed = true;
    }
    if (axis.smooth) {
      ImGui::Spacing();
      ImGui::Indent(kLabelW);
      const float avail = ImGui::GetContentRegionAvail().x;
      const float gap = ImGui::GetStyle().ItemSpacing.x;
      const float field_w = ImMax(80.0f, (avail - gap) * 0.5f - 48.0f);
      // Reserved 48 on each side for the field's inline label ("Gravity",
      // "Sensitivity") which ImGui renders to the right of the drag field.
      ImGui::SetNextItemWidth(field_w);
      if (ImGui::DragFloat("Gravity", &axis.gravity, 0.1f, 0.1f, 50.0f,
                           "%.2f")) {
        changed = true;
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(field_w);
      if (ImGui::DragFloat("Sensitivity", &axis.sensitivity, 0.1f, 0.1f,
                           50.0f, "%.2f")) {
        changed = true;
      }
      ImGui::Unindent(kLabelW);
    }
  }

  ImGui::Unindent(s.WindowPadding.x);

  section::EndDrawerFrame(header_bottom_y, /*fill=*/true);

  ImGui::PopID();
  return changed;
}

// --- Tabs -----------------------------------------------------------------

bool RenderActionsTab(InputContext& ctx, const std::string& ctx_name) {
  bool changed = false;

  int to_remove = -1;
  for (int i = 0; i < (int)ctx.actions.size(); i++) {
    bool want_delete = false;
    changed |= RenderActionCard(ctx.actions[i], i, ctx_name, want_delete);
    if (want_delete) {
      to_remove = i;
    }
  }
  if (to_remove >= 0) {
    ctx.actions.erase(ctx.actions.begin() + to_remove);
    changed = true;
  }

  ImGui::Spacing();
  if (chip_ns::AddChipButton(ICON_LC_PLUS "  Add Action")) {
    ctx.actions.push_back({"New Action", {}, {}});
    changed = true;
  }

  return changed;
}

bool RenderAxesTab(InputContext& ctx, const std::string& ctx_name) {
  bool changed = false;

  int to_remove = -1;
  for (int i = 0; i < (int)ctx.axes.size(); i++) {
    bool want_delete = false;
    changed |= RenderAxisCard(ctx.axes[i], i, ctx_name, want_delete);
    if (want_delete) {
      to_remove = i;
    }
  }
  if (to_remove >= 0) {
    ctx.axes.erase(ctx.axes.begin() + to_remove);
    changed = true;
  }

  ImGui::Spacing();
  if (chip_ns::AddChipButton(ICON_LC_PLUS "  Add Axis")) {
    InputAxisMapping ax;
    ax.name = "New Axis";
    ctx.axes.push_back(std::move(ax));
    changed = true;
  }

  return changed;
}

// --- Context body (right pane) --------------------------------------------

bool RenderContextBody(InputSettings& input, const std::string& name,
                       bool& want_delete_ctx, bool& want_rename_ctx) {
  bool changed = false;
  auto it = input.contexts.find(name);
  if (it == input.contexts.end()) {
    ImGui::TextDisabled("Select a context from the list, or add one.");
    return false;
  }
  InputContext& ctx = it->second;

  // Header row: gamepad glyph + name (frame-padded so they align with the
  // square icon buttons on the right edge).
  const ImGuiStyle& s = ImGui::GetStyle();
  const float btn_w = ImGui::GetFrameHeight();

  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled(ICON_LC_GAMEPAD_2);
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(name.c_str());

  ImGui::SameLine();
  const float controls_w = btn_w * 2.0f + s.ItemSpacing.x;
  RightAlignInChild(controls_w);
  if (button::ToolbarButton(ICON_LC_PENCIL "##rename_ctx")) {
    want_rename_ctx = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Rename context");
  }
  ImGui::SameLine();
  if (button::ToolbarButton(ICON_LC_TRASH_2 "##del_ctx")) {
    want_delete_ctx = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Delete context");
  }

  ImGui::Spacing();
  layout::Separator();
  ImGui::Spacing();

  if (ImGui::BeginTabBar("##input_tabs")) {
    char tab_label[64];
    std::snprintf(tab_label, sizeof(tab_label), "Actions (%d)",
                  (int)ctx.actions.size());
    if (ImGui::BeginTabItem(tab_label)) {
      changed |= RenderActionsTab(ctx, name);
      ImGui::EndTabItem();
    }
    std::snprintf(tab_label, sizeof(tab_label), "Axes (%d)",
                  (int)ctx.axes.size());
    if (ImGui::BeginTabItem(tab_label)) {
      changed |= RenderAxesTab(ctx, name);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  return changed;
}

// --- Sidebar (context list) ------------------------------------------------

// Custom sidebar row: CategoryRow-esque (flat highlight + accent bar) but
// with a trailing "NxN" count badge and double-click-to-rename support.
// Returns true on click.
bool ContextListRow(const char* label, bool selected, int action_count,
                    int axis_count, bool& want_rename, bool& want_delete) {
  const ImGuiStyle& s = ImGui::GetStyle();
  ImGuiWindow* window = ImGui::GetCurrentWindow();

  const float row_h = ImGui::GetFrameHeight();
  const float avail_w = ImGui::GetContentRegionAvail().x;
  const ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::PushID(label);
  const bool clicked =
      ImGui::InvisibleButton("##ctx_row", ImVec2(avail_w, row_h));
  const bool hovered = ImGui::IsItemHovered();
  const bool double_clicked =
      hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  ImGui::PopID();

  if (double_clicked) {
    want_rename = true;
  }

  // Right-click context menu
  if (ImGui::BeginPopupContextItem("##ctx_row_ctx")) {
    if (ImGui::MenuItem("Rename")) {
      want_rename = true;
    }
    if (ImGui::MenuItem("Delete")) {
      want_delete = true;
    }
    ImGui::EndPopup();
  }

  ImDrawList* dl = window->DrawList;
  const layout::WindowEdges edges = layout::GetWindowEdges();

  if (selected || hovered) {
    const ImU32 bg_col = ImGui::GetColorU32(
        selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
    dl->AddRectFilled(ImVec2(edges.left, pos.y),
                      ImVec2(edges.right, pos.y + row_h), bg_col);
  }
  if (selected) {
    dl->AddRectFilled(ImVec2(edges.left, pos.y),
                      ImVec2(edges.left + 2.0f, pos.y + row_h),
                      ImGui::GetColorU32(ImGuiCol_CheckMark));
  }

  const float cy = pos.y + row_h * 0.5f;
  float x = pos.x + s.FramePadding.x;
  draw::IconLabelInline(dl, x, cy, ICON_LC_GAMEPAD_2, label,
                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                        ImGui::GetColorU32(ImGuiCol_Text));

  // Count badge on right (e.g. "3·2")
  char count_buf[32];
  std::snprintf(count_buf, sizeof(count_buf), "%d\xc2\xb7%d", action_count,
                axis_count);
  const ImVec2 count_sz = ImGui::CalcTextSize(count_buf);
  dl->AddText(ImVec2(pos.x + avail_w - s.FramePadding.x - count_sz.x,
                     cy - count_sz.y * 0.5f),
              ImGui::GetColorU32(ImGuiCol_TextDisabled), count_buf);

  return clicked;
}

bool RenderContextsSidebar(InputSettings& input, std::string& selected) {
  bool changed = false;

  // Header row: "Contexts" label + count + right-anchored + icon button.
  const float btn_w = ImGui::GetFrameHeight();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Contexts");
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("(%d)", (int)input.contexts.size());
  ImGui::SameLine();
  RightAlignInChild(btn_w);
  if (button::ToolbarButton(ICON_LC_PLUS "##add_ctx")) {
    State().add_context_mode = true;
    State().add_context_buf[0] = '\0';
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("New context");
  }

  ImGui::Spacing();

  // Inline "add context" input
  if (State().add_context_mode) {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool submit = ImGui::InputTextWithHint(
        "##new_ctx", "Context name", State().add_context_buf,
        sizeof(State().add_context_buf),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (submit && State().add_context_buf[0] != '\0') {
      std::string name = State().add_context_buf;
      if (input.contexts.find(name) == input.contexts.end()) {
        InputContext nc;
        nc.name = name;
        input.contexts[name] = std::move(nc);
        selected = name;
        changed = true;
      }
      State().add_context_mode = false;
      State().add_context_buf[0] = '\0';
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      State().add_context_mode = false;
      State().add_context_buf[0] = '\0';
    }
    ImGui::Spacing();
  }

  // Sidebar rows - push ItemSpacing.y=0 so rows stack flush (each row is
  // already GetFrameHeight tall so they form a continuous list).
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
  std::string rename_from;  // deferred rename to not mutate map during iter
  std::string delete_target;
  for (auto& [name, ctx] : input.contexts) {
    const bool is_selected = (selected == name);
    if (State().rename_target == name) {
      // Inline rename input replaces the row
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
      }
      const bool submit = ImGui::InputText(
          "##rename_ctx", State().rename_buf, sizeof(State().rename_buf),
          ImGuiInputTextFlags_EnterReturnsTrue |
              ImGuiInputTextFlags_AutoSelectAll);
      if (submit && State().rename_buf[0] != '\0') {
        std::string new_name = State().rename_buf;
        if (new_name != name &&
            input.contexts.find(new_name) == input.contexts.end()) {
          rename_from = name;
        }
        State().rename_target.clear();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
          (!ImGui::IsItemActive() && !ImGui::IsItemFocused() &&
           !ImGui::IsItemHovered() &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        State().rename_target.clear();
      }
      continue;
    }

    bool want_rename = false;
    bool want_delete = false;
    if (ContextListRow(name.c_str(), is_selected, (int)ctx.actions.size(),
                       (int)ctx.axes.size(), want_rename, want_delete)) {
      selected = name;
    }
    if (want_rename) {
      State().rename_target = name;
      std::strncpy(State().rename_buf, name.c_str(),
                   sizeof(State().rename_buf) - 1);
      State().rename_buf[sizeof(State().rename_buf) - 1] = '\0';
    }
    if (want_delete) {
      delete_target = name;
    }
  }
  ImGui::PopStyleVar();

  if (!rename_from.empty()) {
    std::string new_name = State().rename_buf;
    auto node = input.contexts.extract(rename_from);
    if (!node.empty()) {
      node.key() = new_name;
      node.mapped().name = new_name;
      input.contexts.insert(std::move(node));
      if (selected == rename_from) {
        selected = new_name;
      }
      changed = true;
    }
  }

  if (!delete_target.empty()) {
    input.contexts.erase(delete_target);
    if (selected == delete_target) {
      selected.clear();
    }
    changed = true;
  }

  return changed;
}

// --- Mouse section --------------------------------------------------------

bool RenderMouseSection(InputSettings& input) {
  bool changed = false;
  ImGui::SeparatorText("Mouse");
  ImGui::SetNextItemWidth(180.0f);
  changed |= ImGui::DragFloat(
      field::PrefixLabel("Sensitivity X").c_str(),
      &input.mouse_sensitivity_x, 1.0f, 1.0f, 500.0f);
  ImGui::SetNextItemWidth(180.0f);
  changed |= ImGui::DragFloat(
      field::PrefixLabel("Sensitivity Y").c_str(),
      &input.mouse_sensitivity_y, 1.0f, 1.0f, 500.0f);

  const int gp_count = Engine::input().GetConnectedGamepadCount();
  if (gp_count > 0) {
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                       ICON_LC_GAMEPAD_2 "  Gamepads: %d connected",
                       gp_count);
  } else {
    ImGui::TextDisabled(ICON_LC_GAMEPAD_2 "  No gamepad connected");
  }
  return changed;
}

}  // namespace

// --- Public entry ----------------------------------------------------------

bool RenderInputSettings(InputSettings& input, std::string& selected_context) {
  bool changed = false;
  changed |= RenderMouseSection(input);

  ImGui::SeparatorText("Contexts");

  // Validate selection
  if (!selected_context.empty() &&
      input.contexts.find(selected_context) == input.contexts.end()) {
    selected_context.clear();
  }

  const float sidebar_w = 200.0f;
  ImGui::BeginChild("##ctx_sidebar", ImVec2(sidebar_w, 0),
                    ImGuiChildFlags_Borders);
  changed |= RenderContextsSidebar(input, selected_context);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("##ctx_body", ImVec2(0, 0));
  bool want_delete_ctx = false;
  bool want_rename_ctx = false;
  changed |= RenderContextBody(input, selected_context, want_delete_ctx,
                               want_rename_ctx);
  ImGui::EndChild();

  if (want_delete_ctx && !selected_context.empty()) {
    input.contexts.erase(selected_context);
    selected_context.clear();
    changed = true;
  }
  if (want_rename_ctx && !selected_context.empty()) {
    State().rename_target = selected_context;
    std::strncpy(State().rename_buf, selected_context.c_str(),
                 sizeof(State().rename_buf) - 1);
    State().rename_buf[sizeof(State().rename_buf) - 1] = '\0';
  }

  changed |= RenderBindPopup(input);

  return changed;
}

}  // namespace wiesel::editor
