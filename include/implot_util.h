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
                        std::optional<std::tuple<float, float, float, float>> axis_limits = std::nullopt) -> int;

extern auto ImPlotEnd() -> void;
