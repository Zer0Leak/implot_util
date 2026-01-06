#include "implot_util.h"

#include <algorithm>  // std::min
#include <cmath>      // std::sqrt

#define IMGUI_DEFINE_MATH_OPERATORS
#include <implot.h>

static auto set_major_grid() {
    auto alpha = 0.05f;
    ImPlotStyle &s = ImPlot::GetStyle();

    // Read base grid color (resolve auto if needed)
    ImVec4 grid = s.Colors[ImPlotCol_AxisGrid];
    if (grid.w < 0.0f) {
        // Default auto behavior: alpha% of AxisText
        ImVec4 axis_text = s.Colors[ImPlotCol_AxisText];
        if (axis_text.w < 0.0f)
            axis_text = ImGui::GetStyle().Colors[ImGuiCol_Text];
        grid = axis_text;
        grid.w *= alpha;
    }

    // Set MAJOR alpha
    grid.w = alpha;
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, grid);

    // Set MINOR alpha relative to major
    ImPlot::PushStyleVar(ImPlotStyleVar_MinorAlpha, 1.0f);  // same as major
}

static auto unset_major_grid() {
    ImPlot::PopStyleVar();
    ImPlot::PopStyleColor();
}

auto ImPlotBeginPlot(const std::string &plot_title,
                     std::optional<std::tuple<float, float, float, float>> axis_limits) -> bool {
    if (!ImPlot::BeginPlot(plot_title.c_str(), ImVec2(-1, -1))) {
        return false;
    }

    set_major_grid();

    if (axis_limits.has_value()) {
        auto [min_x, max_x, min_y, max_y] = *axis_limits;
        ImPlot::SetupAxesLimits(min_x, max_x, min_y, max_y, ImPlotCond_Once);
    }

    return true;
}

auto ImPlotEndPlot() -> void {
    unset_major_grid();
    ImPlot::EndPlot();
}

auto ImPlotBegin(const std::string &plot_title, std::optional<const std::string> wnd_title,
                 std::optional<std::tuple<float, float, float, float>> axis_limits) -> bool {
    auto wnd_name = wnd_title.has_value() ? *wnd_title : plot_title;
    if (!ImGui::Begin(wnd_name.c_str())) {
        ImGui::End();
        return false;
    }

    if (!ImPlotBeginPlot(plot_title, axis_limits)) {
        ImGui::End();
        return false;
    }

    return true;
}

auto ImPlotEnd() -> void {
    ImPlotEndPlot();
    ImGui::End();
}

auto ImPlotBeginSub(const std::string &plot_title, std::optional<const std::string> wnd_title, int rows,
                    int cols) -> bool {
    auto wnd_name = wnd_title.has_value() ? *wnd_title : plot_title;
    if (!ImGui::Begin(wnd_name.c_str())) {
        ImGui::End();
        return false;
    }

    if (!ImPlot::BeginSubplots(plot_title.c_str(), rows, cols, ImVec2(-1, -1))) {
        ImGui::End();
        return false;
    }

    return true;
}

auto ImPlotEndSub() -> void {
    ImPlot::EndSubplots();
    ImGui::End();
}

static inline ImVec2 v2_add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
static inline ImVec2 v2_sub(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
static inline ImVec2 v2_mul(ImVec2 v, float s) { return ImVec2(v.x * s, v.y * s); }

static inline float v2_len(ImVec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

auto AddDashedLine(ImDrawList *draw_list, ImVec2 p1, ImVec2 p2, ImU32 color, float thickness, float dash_len,
                   float gap_len) -> void {
    const ImVec2 delta = v2_sub(p2, p1);
    const float len = v2_len(delta);
    if (len <= 0.0f)
        return;

    const ImVec2 dir = v2_mul(delta, 1.0f / len);

    float dist = 0.0f;
    while (dist < len) {
        const float seg_len = std::min(dash_len, len - dist);
        const ImVec2 a = v2_add(p1, v2_mul(dir, dist));
        const ImVec2 b = v2_add(p1, v2_mul(dir, dist + seg_len));
        draw_list->AddLine(a, b, color, thickness);
        dist += dash_len + gap_len;
    }
}
