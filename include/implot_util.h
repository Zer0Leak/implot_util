#pragma once

#include <optional>
#include <string>

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

extern auto ImPlotBegin(const std::string &plot_title, std::optional<const std::string> wnd_title = std::nullopt,
                        std::optional<std::tuple<float, float, float, float>> axis_limits = std::nullopt) -> bool;

extern auto ImPlotEnd() -> void;

extern auto ImPlotBeginSub(const std::string &plot_title, std::optional<const std::string> wnd_title, int rows,
                           int cols) -> bool;

extern auto ImPlotEndSub() -> void;

extern auto ImPlotBeginPlot(const std::string &plot_title,
                            std::optional<std::tuple<float, float, float, float>> axis_limits) -> bool;

extern auto ImPlotEndPlot() -> void;

extern auto AddDashedLine(ImDrawList *draw_list, ImVec2 p1, ImVec2 p2, ImU32 color, float thickness,
                          float dash_len = 6.0f, float gap_len = 4.0f) -> void;
