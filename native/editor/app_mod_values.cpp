/* The editor's window (app_internal.h): the MOD VALUES view, every mod's add_option rows (SDK 54 option_info), the
 * editor-only ones too, changed live through apply_option. HellForge keeps and saves each value; the view only reads
 * and sets them, so it holds no state of its own beyond the selection. */
#include <math.h>
#include <algorithm>
#include "app_internal.h"

namespace editorapp
{
struct OptRow
{
    std::string id, label;
    uint32_t kind = 0, flags = 0;
    double value = 0, min = 0, max = 1, step = 1;
    std::vector<std::string> choices;
};
struct OptMod { std::string id, name; std::vector<OptRow> rows; };

static std::vector<OptMod> g_optMods;
static ULONGLONG g_optAt;
static volatile LONG g_optChanged;
static int g_optMod, g_optRow;
/* A slider being dragged: its value follows the mouse and is applied on release (each apply edits game records). */
struct OptDrag { std::string mod, id; float x = 0, w = 0; double value = 0; bool on = false; };
static OptDrag g_optDrag;

static bool HasOptions()
{
    return g_host.api->size >= offsetof(HfApi, apply_option) + sizeof(void*) && g_host.api->option_info != nullptr;
}

void ValuesChanged() { InterlockedExchange(&g_optChanged, 1); }

static OptRow RowOf(const HfOptionInfo& info)
{
    OptRow r;
    r.id = info.id; r.label = info.label; r.kind = info.kind; r.flags = info.flags;
    r.value = info.value; r.min = info.min; r.max = info.max; r.step = info.step > 0 ? info.step : 1;
    for (uint32_t i = 0; i < info.choice_count && i < HF_OPTION_MAX_CHOICES; i++) r.choices.push_back(info.choices[i] ? info.choices[i] : "");
    return r;
}

/* The options again (every 500 ms while the view shows, at once after a live mod change): grouped by mod in the order
 * they were added, the selected mod kept. */
static std::vector<OptMod> ReadOptions()
{
    std::vector<OptMod> list;
    for (uint32_t i = 0; i < 4096; i++)
    {
        HfOptionInfo info{};
        info.size = sizeof info;
        if (!g_host.api->option_info(g_host.mod, i, &info)) break;
        if (info.mod_id == nullptr || info.id == nullptr) continue;
        auto at = std::find_if(list.begin(), list.end(), [&](const OptMod& m) { return m.id == info.mod_id; });
        if (at == list.end()) at = list.insert(list.end(), OptMod{info.mod_id, info.mod_name ? info.mod_name : info.mod_id, {}});
        at->rows.push_back(RowOf(info));
    }
    return list;
}

void ScanValues(bool now)
{
    ULONGLONG tick = GetTickCount64();
    if (InterlockedExchange(&g_optChanged, 0)) now = true;
    if (!HasOptions() || (!now && g_optAt != 0 && tick - g_optAt < 500)) return;
    g_optAt = tick;
    std::string keep = g_optMod >= 0 && g_optMod < (int)g_optMods.size() ? g_optMods[g_optMod].id : "";
    g_optMods = ReadOptions();
    g_optMod = 0;
    for (int i = 0; i < (int)g_optMods.size(); i++) if (g_optMods[i].id == keep) g_optMod = i;
}

static OptMod* SelectedValues() { return g_optMod >= 0 && g_optMod < (int)g_optMods.size() ? &g_optMods[g_optMod] : nullptr; }

static void ApplyOption(const OptMod& m, OptRow& r, double value)
{
    if (!g_host.api->apply_option(g_host.mod, m.id.c_str(), r.id.c_str(), value)) { Toast(Refusal("HellForge refused the value"), ERROR_); return; }
    r.value = value; /* until the next read, which has HellForge's clamped and snapped value */
    g_optAt = 0;
    Sound(sounds::CLICK);
}

static double Snap(const OptRow& r, double v)
{
    v = std::clamp(v, r.min, r.max);
    return std::min(r.max, r.min + floor((v - r.min) / r.step + 0.5) * r.step);
}

/* Left and right: a slider's step (shift ten), a toggle flipped, a choice the next or the one before. */
static void Step(const OptMod& m, OptRow& r, int direction, bool shift)
{
    if (r.kind == HF_OPTION_TOGGLE) { ApplyOption(m, r, r.value != 0 ? 0 : 1); return; }
    if (r.kind == HF_OPTION_CHOICE)
    {
        int n = std::max(1, (int)r.choices.size());
        ApplyOption(m, r, (double)(((int)r.value + direction + n) % n));
        return;
    }
    ApplyOption(m, r, Snap(r, r.value + direction * r.step * (shift ? 10 : 1)));
}

static std::string ValueText(const OptRow& r, double v)
{
    if (r.kind == HF_OPTION_TOGGLE) return v != 0 ? "ON" : "OFF";
    if (r.kind == HF_OPTION_CHOICE) { int i = (int)v; return i >= 0 && i < (int)r.choices.size() ? r.choices[i] : "?"; }
    char text[48];
    snprintf(text, sizeof text, "%.6g", v);
    return text;
}

/* A slider's track: a click sets the value under the mouse, a drag follows it, the release applies it. */
static void DrawSlider(const OptMod& m, OptRow& r, float x, float cy, float w, bool selected)
{
    bool dragging = g_optDrag.on && g_optDrag.mod == m.id && g_optDrag.id == r.id;
    if (dragging)
    {
        float ux, uy;
        g_cv.ToUnits(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y, &ux, &uy);
        g_optDrag.value = Snap(r, r.min + (r.max - r.min) * std::clamp((ux - x) / w, 0.0f, 1.0f));
        if (!ImGui::IsMouseDown(0)) { g_optDrag.on = false; if (g_optDrag.value != r.value) ApplyOption(m, r, g_optDrag.value); }
    }
    double v = dragging ? g_optDrag.value : r.value;
    float f = (float)((v - r.min) / std::max(1e-12, r.max - r.min));
    g_cv.Rect(x, cy - 2, w, 4, LINE_STRONG, 3);
    g_cv.Rect(x, cy - 2, w * f, 4, selected || dragging ? GOLD : GOLD_DIM, 3);
    g_cv.Rect(x + w * f - 5, cy - 9, 10, 18, dragging ? PRIMARY_HOVER : GOLD, 4);
    if (g_cv.Hit(x - 6, cy - 12, w + 12, 24, ui::Id(m.id + "/" + r.id + "#slider")))
        g_optDrag = {m.id, r.id, x, w, v, true};
}

/* A toggle's switch or a choice's box: a click flips or picks the next (a right click the one before). */
static void DrawPick(const OptMod& m, OptRow& r, float x, float cy, float w)
{
    uint64_t id = ui::Id(m.id + "/" + r.id + "#pick");
    bool on = r.kind == HF_OPTION_TOGGLE && r.value != 0;
    float bw = r.kind == HF_OPTION_TOGGLE ? 46 : w;
    g_cv.Rect(x, cy - 11, bw, 22, on ? GOLD : g_cv.Hovered() == id ? HOVER : BOX, 3);
    g_cv.Frame(x, cy - 11, bw, 22, on ? GOLD : BOX_EDGE, 4);
    if (r.kind == HF_OPTION_TOGGLE) g_cv.Rect(on ? x + 28 : x + 4, cy - 7, 14, 14, on ? INVERSE : DIM, 5);
    else g_cv.Text(ValueText(r, r.value), x + bw / 2, cy, Raw(T(SZ_SMALL, TEXT, ui::BODY, ui::CENTER, bw - 16, 5)));
    if (g_cv.Hit(x, cy - 12, bw, 24, id)) Step(m, r, 1, false);
    if (g_cv.RightClicked(id)) Step(m, r, -1, false);
}

static void DrawValueRow(OptMod& m, int i, float x, float y, float lw, float rh, bool hovered)
{
    OptRow& r = m.rows[i];
    bool selected = i == g_optRow;
    if (selected) { g_cv.Rect(x + 8, y + 2, lw - 16, rh - 4, SELECT, 2); g_cv.Rect(x + 8, y + 2, 3, rh - 4, MOD, 3); }
    else if (hovered) g_cv.Rect(x + 8, y + 2, lw - 16, rh - 4, HOVER, 2);
    float cy = y + rh / 2;
    g_cv.Text(r.label, x + 22, cy, Raw(T(SZ_LABEL, selected ? TEXT : DIM, ui::BODY, ui::LEFT, 330)));
    if (r.flags & HF_OPTION_EDITOR_ONLY) Chip("EDITOR ONLY", x + 470, cy, FAINT, LINE, true);
    float wx = x + 490, ww = lw - 490 - 150;
    if (r.kind == HF_OPTION_SLIDER) DrawSlider(m, r, wx, cy, ww, selected);
    else DrawPick(m, r, wx, cy, std::min(ww, 260.0f));
    bool dragging = g_optDrag.on && g_optDrag.mod == m.id && g_optDrag.id == r.id;
    if (r.kind == HF_OPTION_SLIDER)
        g_cv.Text(ValueText(r, dragging ? g_optDrag.value : r.value), x + lw - 24, cy, Raw(T(SZ_LABEL, dragging ? PENDING : TEXT, ui::BODY, ui::RIGHT, 120)));
}

static void DrawValueMods(float y0, float h, float LW)
{
    ListOpts o;
    o.x = 0; o.y = y0 + 6; o.w = LW; o.h = h - 12; o.rowH = 46; o.count = (int)g_optMods.size(); o.selected = g_optMod; o.key = "optmods"; o.focused = true;
    o.click = [&](int i) { g_optMod = i; g_optRow = 0; g_scroll["optrows"] = 0; Sound(sounds::CLICK); };
    o.draw = [&](int i, float x, float y, float lw, float rh, bool hovered) {
        const OptMod& m = g_optMods[i];
        DrawModLine(m.name, m.id, {x, y, lw, rh}, i == g_optMod, hovered, 100);
        Chip(std::to_string(m.rows.size()), x + lw - 18, y + rh / 2, MOD, MOD_SOFT, true);
    };
    List(o);
}

void DrawValues(float y0, float h)
{
    ScanValues(false);
    float W = PANEL_W, LW = 380;
    g_cv.Rect(0, y0, LW, h, PANEL_ALT, 1);
    g_cv.Rect(LW - 1, y0, 1, h, LINE, 2);
    g_cv.Rect(LW, y0, W - LW, h, PANEL, 1);
    if (!HasOptions())
    {
        g_cv.Text("This HellForge cannot list its mods' values (its mod loader is older than SDK 54).", 20, y0 + 30, T(SZ_SMALL, FAINT, ui::BODY, ui::LEFT, W - 40));
        return;
    }
    DrawValueMods(y0, h, LW);
    float x0 = LW, rw = W - LW;
    OptMod* m = SelectedValues();
    if (m == nullptr) { g_cv.Text("No mod has added a value (add_option) in this session.", x0 + 24, y0 + 34, T(SZ_SMALL, FAINT)); return; }
    g_cv.Text(m->name, x0 + 24, y0 + 26, Raw(T(20, TEXT, ui::TITLE, ui::LEFT, rw - 48)));
    g_cv.Text("Changes apply at once and are saved by HellForge. They are this machine's: tune solo.", x0 + 24, y0 + 50,
        T(SZ_SMALL, FAINT, ui::BODY, ui::LEFT, rw - 48));
    float hy = y0 + 70;
    g_optRow = std::clamp(g_optRow, 0, std::max(0, (int)m->rows.size() - 1));
    ListOpts r;
    r.x = x0; r.y = hy; r.w = rw; r.h = y0 + h - hy - 4; r.rowH = 40; r.count = (int)m->rows.size(); r.selected = g_optRow; r.key = "optrows";
    r.click = [&](int i) { g_optRow = i; };
    r.draw = [&](int i, float x, float y, float lw, float rh, bool hovered) { DrawValueRow(*m, i, x, y, lw, rh, hovered); };
    List(r);
}

static void ValueRowKeys(OptMod& m, bool shift)
{
    if (Pressed(ImGuiKey_PageUp)) g_optRow = std::max(0, g_optRow - 1);
    if (Pressed(ImGuiKey_PageDown)) g_optRow = std::min((int)m.rows.size() - 1, g_optRow + 1);
    OptRow& r = m.rows[std::clamp(g_optRow, 0, (int)m.rows.size() - 1)];
    if (Pressed(ImGuiKey_LeftArrow) || Pressed(ImGuiKey_RightArrow)) Step(m, r, Pressed(ImGuiKey_LeftArrow) ? -1 : 1, shift);
    if ((Pressed(ImGuiKey_Enter) || Pressed(ImGuiKey_KeypadEnter)) && r.kind != HF_OPTION_SLIDER) Step(m, r, 1, false);
}

void ValuesKeys(bool shift)
{
    int mods = (int)g_optMods.size();
    if (Pressed(ImGuiKey_UpArrow) && g_optMod > 0) { g_optMod--; g_optRow = 0; }
    if (Pressed(ImGuiKey_DownArrow) && g_optMod + 1 < mods) { g_optMod++; g_optRow = 0; }
    if (Pressed(ImGuiKey_F5)) ScanValues(true);
    OptMod* m = SelectedValues();
    if (m != nullptr && !m->rows.empty()) ValueRowKeys(*m, shift);
}
} // namespace editorapp
