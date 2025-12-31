#include "implot_util.h"

auto ImPlotBegin(const std::string &plot_title, std::optional<const std::string> wnd_title,
                 std::optional<std::tuple<float, float, float, float>> axis_limits) -> int {
    auto wnd_name = wnd_title.has_value() ? *wnd_title : plot_title;
    if (!ImGui::Begin(wnd_name.c_str())) {
        return false;
    }
    if (!ImPlot::BeginPlot(plot_title.c_str())) {
        ImGui::End();
        return false;
    }

    if (axis_limits.has_value()) {
        auto [min_x, max_x, min_y, max_y] = *axis_limits;
        ImPlot::SetupAxesLimits(min_x, max_x, min_y, max_y, ImPlotCond_Always);
    }
    return true;
}

auto ImPlotEnd() -> void {
    ImPlot::EndPlot();
    ImGui::End();
}