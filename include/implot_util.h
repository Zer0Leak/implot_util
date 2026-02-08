#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <vector>

template <class T> class Singleton {
  public:
    static T &instance() {
        static T instance;
        return instance;
    }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;

  protected:
    Singleton() = default;
    ~Singleton() = default;
};

extern auto ImPlotBegin(const std::string &plot_title, const std::string &x_label, const std::string &y_label,
                        std::optional<const std::string> wnd_title = std::nullopt,
                        std::optional<std::tuple<float, float, float, float>> axis_limits = std::nullopt) -> bool;

extern auto ImPlotEnd() -> void;

extern auto ImPlotBeginSub(const std::string &plot_title, std::optional<const std::string> wnd_title, int rows,
                           int cols) -> bool;

extern auto ImPlotEndSub() -> void;

extern auto ImPlotBeginPlot(const std::string &plot_title, const std::string &x_label, const std::string &y_label,
                            std::optional<std::tuple<float, float, float, float>> axis_limits, bool equal_axis = false,
                            ImVec2 size = ImVec2(-1, -1)) -> bool;

extern auto ImPlotEndPlot() -> void;

extern auto AddDashedLine(ImDrawList *draw_list, ImVec2 p1, ImVec2 p2, ImU32 color, float thickness,
                          float dash_len = 6.0f, float gap_len = 4.0f) -> void;

template <std::floating_point T> void ImPlotAddDashedLine(std::span<const T> x, std::span<const T> y) {
    assert(x.size() == y.size());
    const ImVec4 col_f = ImVec4(1.0f, 0.0f, 0.0f, 100.0f / 255.0f);  // alpha = 100
    const ImU32 col_u = ImGui::ColorConvertFloat4ToU32(col_f);
    constexpr double thickness = 5.0f;

    ImPlot::SetNextLineStyle(col_f, thickness);
    ImPlot::PlotDummy("y_ideal");
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    for (auto i = 0; i < x.size() - 1; ++i) {
        ImPlotPoint a = {x[i], y[i]};
        ImPlotPoint b = {x[i + 1], y[i + 1]};
        const ImVec2 pos1 = ImPlot::PlotToPixels(a);
        const ImVec2 pos2 = ImPlot::PlotToPixels(b);
        AddDashedLine(draw_list, pos1, pos2, col_u, thickness);
    }
}

extern inline auto catmull_rom(double p0, double p1, double p2, double p3, double t) -> double;

extern auto spline(const std::vector<double> &xs, const std::vector<double> &ys,
                   int steps = 12) -> std::tuple<std::vector<double>, std::vector<double>>;

extern auto
equalize_axes(const std::tuple<double, double, double, double> &limits) -> std::tuple<double, double, double, double>;

extern auto equalize_axes_to_plot_pixels(const std::tuple<double, double, double, double> &limits,
                                         const ImVec2 plot_px) -> std::tuple<double, double, double, double>;

extern auto plot_h_line(double y, ImVec4 color = ImVec4(1.0, 1.0, 1.0, 0.8f)) -> void;

extern auto plot_v_threshold(double threshold, ImVec4 color_left = ImVec4(0.2f, 0.4f, 1.0f, 0.5f) /* blue, 50% alpha */,
                             ImVec4 color_right = ImVec4(1.0f, 0.3f, 0.3f, 0.5f) /** red, 50% alpha */) -> void;
