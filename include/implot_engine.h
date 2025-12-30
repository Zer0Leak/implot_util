#pragma once

#include "implot_util.h"
#include <atomic>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vulkan_helper.h>

extern VulkanHelper vulkanHelper;

class ImPlotEngine : public Singleton<ImPlotEngine> {
    friend class Singleton<ImPlotEngine>;

  public:
    auto init(const std::string &title) -> void;
    auto deinit() -> void;
    auto SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width, int height) -> void;
    auto CleanupVulkanWindow() -> void;
    auto FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data) -> void;
    auto FramePresent(ImGui_ImplVulkanH_Window *wd) -> void;
    auto show_async() -> void;
    auto show_stop() -> void;
    auto show_wait() -> void;
    auto show_detach() -> void;
    auto show() -> void;

  private:
    VulkanHelper vulkanHelper_;
    ImGui_ImplVulkanH_Window mainWindowData_;
    uint32_t minImageCount_{2};
    bool swapChainRebuild_{false};
    GLFWwindow *window_{nullptr};

  private:
    std::jthread show_thread_;
    std::stop_token stop_token_;
    ImPlotEngine() noexcept = default;
    ~ImPlotEngine() noexcept = default;
};