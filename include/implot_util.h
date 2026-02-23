#pragma once

#include <cmath>
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

struct ImPlotClassScatterStyleScope {
    int vars = 0;
    int colors = 0;

    static inline ImVec4 hsv_to_imvec4(float h, float s, float v, float a = 1.0f) {
        float c = v * s;
        float x = c * (1.0f - std::fabsf(std::fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r = 0, g = 0, b = 0;

        if (h < 60) {
            r = c;
            g = x;
            b = 0;
        } else if (h < 120) {
            r = x;
            g = c;
            b = 0;
        } else if (h < 180) {
            r = 0;
            g = c;
            b = x;
        } else if (h < 240) {
            r = 0;
            g = x;
            b = c;
        } else if (h < 300) {
            r = x;
            g = 0;
            b = c;
        } else {
            r = c;
            g = 0;
            b = x;
        }

        return ImVec4(r + m, g + m, b + m, a);
    }

    static inline std::vector<ImVec4> rainbow_colors(int N, float alpha = 1.0f) {
        std::vector<ImVec4> colors;
        colors.reserve(N);

        for (int i = 0; i < N; ++i) {
            float h = 300.0f * float(i) / float(N - 1);  // 0° → 300°
            colors.push_back(hsv_to_imvec4(h, 1.0f, 1.0f, alpha));
        }
        return colors;
    }

    static inline float wrap360(float h) {
        h = std::fmod(h, 360.0f);
        if (h < 0.0f)
            h += 360.0f;
        return h;
    }

    // Optional: avoid a hue band (e.g., skip yellow-ish if your background is light)
    static inline bool hue_in_band(float h, float lo, float hi) {
        // assumes lo<=hi and both in [0,360)
        return (h >= lo && h <= hi);
    }

    static inline std::vector<ImVec4> rainbow_colors_high_contrast(int N, float alpha = 1.0f) {
        std::vector<ImVec4> colors;
        colors.reserve(N);

        constexpr float golden = 137.50776f;  // golden angle
        float h = 0.0f;                       // start at red; you can change start hue

        for (int i = 0; i < N; ++i) {
            // Step by golden angle
            h = wrap360(h + golden);

            // Optional: keep only 0..300 range like you had (avoid wrap back toward red)
            // If you want that, map 360->300 scale:
            float h300 = (h / 360.0f) * 300.0f;

            // Strong saturation; slightly reduce value to avoid neon glare on white backgrounds
            float s = 0.95f;
            float v = 0.95f;

            // Optional extra contrast: alternate brightness a bit (useful when lines overlap)
            // v = (i & 1) ? 0.85f : 1.0f;

            colors.push_back(hsv_to_imvec4(h300, s, v, alpha));
        }

        return colors;
    }

    explicit ImPlotClassScatterStyleScope(int index) {
        static const auto colors_fg = rainbow_colors_high_contrast(17, 1.0f);
        static const auto colors_bg = rainbow_colors_high_contrast(17, 0.5f);
        // Okabe–Ito extended to 16 (light variants)
        static const ImVec4 OKABE_ITO_16[16] = {
            // Original 8
            ImVec4(0.000f, 0.450f, 0.698f, 1.0f),  // Blue
            ImVec4(0.902f, 0.624f, 0.000f, 1.0f),  // Orange
            ImVec4(0.000f, 0.620f, 0.451f, 1.0f),  // Bluish green
            ImVec4(0.941f, 0.894f, 0.259f, 1.0f),  // Yellow
            ImVec4(0.800f, 0.475f, 0.655f, 1.0f),  // Reddish purple
            ImVec4(1.000f, 1.000f, 1.000f, 1.0f),  // White

            // Light variants (≈ 30% toward white)
            ImVec4(0.300f, 0.615f, 0.788f, 1.0f),  // Light blue
            ImVec4(0.931f, 0.736f, 0.300f, 1.0f),  // Light orange
            ImVec4(0.300f, 0.735f, 0.605f, 1.0f),  // Light bluish green
            ImVec4(0.965f, 0.926f, 0.490f, 1.0f),  // Light yellow
            ImVec4(0.860f, 0.640f, 0.765f, 1.0f),  // Light reddish purple
            ImVec4(0.400f, 0.400f, 0.400f, 1.0f)   // Gray (light black)
        };

        const int mark_symbol = index % ImPlotMarker_COUNT;
        const auto num_colors = sizeof(OKABE_ITO_16) / sizeof(OKABE_ITO_16[0]);
        const auto &fg_color = OKABE_ITO_16[index % num_colors];
        auto bg_color = fg_color;
        bg_color.w = 0.5f;

        ImPlot::PushStyleVar(ImPlotStyleVar_Marker, (int)mark_symbol);
        vars++;
        ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 6.0f);
        vars++;
        ImPlot::PushStyleColor(ImPlotCol_MarkerFill, bg_color);
        colors++;
        ImPlot::PushStyleColor(ImPlotCol_MarkerOutline, fg_color);
        colors++;
    }

    ImPlotClassScatterStyleScope(const ImPlotClassScatterStyleScope &) = delete;
    ImPlotClassScatterStyleScope &operator=(const ImPlotClassScatterStyleScope &) = delete;

    ~ImPlotClassScatterStyleScope() noexcept {
        if (vars)
            ImPlot::PopStyleVar(vars);
        if (colors)
            ImPlot::PopStyleColor(colors);
    }
};