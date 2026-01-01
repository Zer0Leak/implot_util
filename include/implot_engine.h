#pragma once

#include "implot_util.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <vulkan_helper.h>

struct Entry {
    uint32_t id;
    std::string key;
    std::move_only_function<void()> fn;

    Entry(std::move_only_function<void()> f, std::string k = "")
        : id(UINT32_MAX), key(std::move(k)), fn(std::move(f)) {}
};

using EntryPtr = std::shared_ptr<Entry>;

class ImPlotEngine : public Singleton<ImPlotEngine> {
    friend class Singleton<ImPlotEngine>;

  public:
    auto init(const std::string &title) -> void;
    auto deinit() -> void;
    auto show_async() -> void;
    auto show_stop() -> void;
    auto show_wait() -> void;
    auto show_detach() -> void;
    auto show(std::optional<std::string> title = std::nullopt, bool clear_entries = true) -> void;

    template <class F> auto draw(F &&fn) -> uint32_t { return draw(std::make_shared<Entry>(std::forward<F>(fn))); }

    template <class F> auto draw(std::string key, F &&fn) -> uint32_t {
        return draw(std::make_shared<Entry>(std::forward<F>(fn), std::move(key)));
    }

    auto draw(EntryPtr item) -> uint32_t;
    auto remove_drawer(uint32_t id) -> void;
    auto remove_drawer(const std::string &name) -> void;
    auto remove_drawers() -> void;

  private:
    auto SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width, int height) -> void;
    auto CleanupVulkanWindow() -> void;
    auto FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data) -> void;
    auto FramePresent(ImGui_ImplVulkanH_Window *wd) -> void;

  private:
    uint32_t lastDrawerId_{0};
    std::atomic<std::shared_ptr<const std::vector<EntryPtr>>> drawers_;
    std::recursive_mutex drawers_mutex_;

  private:
    VulkanHelper vulkanHelper_;
    ImGui_ImplVulkanH_Window mainWindowData_;
    uint32_t minImageCount_{2};
    bool swapChainRebuild_{false};
    GLFWwindow *window_{nullptr};

  private:
    std::string title_;
    std::jthread show_thread_;
    std::stop_token stop_token_;
    ImPlotEngine() noexcept = default;
    ~ImPlotEngine() noexcept = default;
};