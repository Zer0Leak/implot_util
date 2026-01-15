#include "implot_util.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

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

auto ImPlotBeginPlot(const std::string &plot_title, const std::string &x_label, const std::string &y_label,
                     std::optional<std::tuple<float, float, float, float>> axis_limits, bool equal_axis,
                     ImVec2 size) -> bool {

    ImVec2 plot_px;
    if (axis_limits.has_value()) {
        if (equal_axis) {
            // Compute limits BEFORE BeginPlot
            plot_px = ImGui::GetContentRegionAvail();
            // auto [xmin, xmax, ymin, ymax] = equalize_axes_to_plot_pixels(*axis_limits, plot_px);

            // IMPORTANT: call this BEFORE BeginPlot (avoids SetupLocked)
            // ImPlot::SetNextAxesLimits(xmin, xmax, ymin, ymax, ImPlotCond_Always);
        }
    }

    if (!ImPlot::BeginPlot(plot_title.c_str(), size)) {
        return false;
    }

    set_major_grid();

    ImPlot::SetupAxes(x_label.c_str(), y_label.c_str());

    if (axis_limits.has_value()) {
        if (equal_axis) {
            auto [xmin, xmax, ymin, ymax] = equalize_axes_to_plot_pixels(*axis_limits, plot_px);

            // IMPORTANT: call this BEFORE BeginPlot (avoids SetupLocked)
            ImPlot::SetupAxesLimits(xmin, xmax, ymin, ymax, ImPlotCond_Always);
        } else {
            auto [min_x, max_x, min_y, max_y] = *axis_limits;
            ImPlot::SetupAxesLimits(min_x, max_x, min_y, max_y, ImPlotCond_Always);
        }
    }

    return true;
}

auto ImPlotEndPlot() -> void {
    unset_major_grid();
    ImPlot::EndPlot();
}

auto ImPlotBegin(const std::string &plot_title, const std::string &x_label, const std::string &y_label,
                 std::optional<const std::string> wnd_title,
                 std::optional<std::tuple<float, float, float, float>> axis_limits) -> bool {
    auto wnd_name = wnd_title.has_value() ? *wnd_title : plot_title;
    if (!ImGui::Begin(wnd_name.c_str())) {
        ImGui::End();
        return false;
    }

    if (!ImPlotBeginPlot(plot_title, x_label, y_label, axis_limits)) {
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

inline auto catmull_rom(double p0, double p1, double p2, double p3, double t) -> double {
    const double t2 = t * t;
    const double t3 = t2 * t;
    return 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

auto spline(const std::vector<double> &xs, const std::vector<double> &ys,
            int steps) -> std::tuple<std::vector<double>, std::vector<double>> {
    std::vector<double> xs_smooth;
    std::vector<double> ys_smooth;

    const std::size_t n = xs.size();
    if (n < 2 || ys.size() != n)
        return {xs_smooth, ys_smooth};

    xs_smooth.reserve((n - 1) * steps + 1);
    ys_smooth.reserve((n - 1) * steps + 1);

    for (std::size_t i = 0; i < n - 1; ++i) {
        // Clamp endpoints by repetition
        const double x0 = (i == 0) ? xs[i] : xs[i - 1];
        const double x1 = xs[i];
        const double x2 = xs[i + 1];
        const double x3 = (i + 2 < n) ? xs[i + 2] : xs[i + 1];

        const double y0 = (i == 0) ? ys[i] : ys[i - 1];
        const double y1 = ys[i];
        const double y2 = ys[i + 1];
        const double y3 = (i + 2 < n) ? ys[i + 2] : ys[i + 1];

        for (int k = 0; k < steps; ++k) {
            const double t = double(k) / steps;
            xs_smooth.push_back(catmull_rom(x0, x1, x2, x3, t));
            ys_smooth.push_back(catmull_rom(y0, y1, y2, y3, t));
        }
    }

    // Explicitly include the last original point
    xs_smooth.push_back(xs.back());
    ys_smooth.push_back(ys.back());

    return {xs_smooth, ys_smooth};
}

auto equalize_axes(const std::tuple<double, double, double, double> &limits)
    -> std::tuple<double, double, double, double> {
    const auto &[xmin, xmax, ymin, ymax] = limits;

    const double xrange = xmax - xmin;
    const double yrange = ymax - ymin;

    // Handle degenerate cases defensively
    if (xrange <= 0.0 && yrange <= 0.0)
        return limits;

    const double xmid = 0.5 * (xmin + xmax);
    const double ymid = 0.5 * (ymin + ymax);

    const double half = 0.5 * std::max(xrange, yrange);

    return {xmid - half, xmid + half, ymid - half, ymid + half};
}

// limits = {xmin, xmax, ymin, ymax}
// plot_px = ImPlot::GetPlotSize()  (in pixels)
// Returns new limits that preserve 1:1 scale (same units per pixel) under resizing.
auto equalize_axes_to_plot_pixels(const std::tuple<double, double, double, double> &limits,
                                  const ImVec2 plot_px) -> std::tuple<double, double, double, double> {
    auto [xmin, xmax, ymin, ymax] = limits;

    const double xrange = xmax - xmin;
    const double yrange = ymax - ymin;

    // Defensive: degenerate ranges or invalid plot size
    if (!(xrange > 0.0) || !(yrange > 0.0) || plot_px.x <= 0.0f || plot_px.y <= 0.0f)
        return limits;

    const double xmid = 0.5 * (xmin + xmax);
    const double ymid = 0.5 * (ymin + ymax);

    // data aspect (units) and pixel aspect
    const double data_aspect = xrange / yrange;
    const double pixel_aspect = double(plot_px.x) / double(plot_px.y);

    // If plot is wider (pixel_aspect larger), we must expand X (or shrink Y, but we expand).
    if (pixel_aspect > data_aspect) {
        const double new_xrange = yrange * pixel_aspect;
        const double half = 0.5 * new_xrange;
        xmin = xmid - half;
        xmax = xmid + half;
    } else {
        const double new_yrange = xrange / pixel_aspect;
        const double half = 0.5 * new_yrange;
        ymin = ymid - half;
        ymax = ymid + half;
    }

    return {xmin, xmax, ymin, ymax};
}
