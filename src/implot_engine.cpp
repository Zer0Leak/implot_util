#include "implot_engine.h"

#include <cassert>
#include <thread>

// Include ImGui / ImPlot headers
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>
#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // abort
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scope_helper.h"
#include "vulkan_helper.h"

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

auto ImPlotEngine::init(const std::string &title) -> void {
    std::scoped_lock guard(drawers_mutex_);
    if (this->window_) {
        return;
    }

    this->title_ = title;

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());  // Valid on GLFW 3.3+ only
    this->window_ =
        glfwCreateWindow((int)(1600 * main_scale), (int)(1000 * main_scale), title.c_str(), nullptr, nullptr);
    if (!glfwVulkanSupported()) {
        throw std::runtime_error("GLFW: Vulkan Not Supported");
    }

    ImVector<const char *> extensions;
    uint32_t extensions_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions.push_back(glfw_extensions[i]);
    this->vulkanHelper_.Setup(extensions);
    ScopeFail rollback([&]() { this->vulkanHelper_.Cleanup(); });

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(this->vulkanHelper_.data.instance, this->window_,
                                           this->vulkanHelper_.data.allocator, &surface);
    VulkanHelper::check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(this->window_, &w, &h);
    ImGui_ImplVulkanH_Window *wd = &this->mainWindowData_;
    this->SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImPlot::CreateContext();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                      // changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;  // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this
                                      // unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(this->window_, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    // init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion,
    // otherwise will default to header version.
    init_info.Instance = this->vulkanHelper_.data.instance;
    init_info.PhysicalDevice = this->vulkanHelper_.data.physicalDevice;
    init_info.Device = this->vulkanHelper_.data.device;
    init_info.QueueFamily = this->vulkanHelper_.data.queueFamily;
    init_info.Queue = this->vulkanHelper_.data.queue;
    init_info.PipelineCache = this->vulkanHelper_.data.pipelineCache;
    init_info.DescriptorPool = this->vulkanHelper_.data.descriptorPool;
    init_info.MinImageCount = this->minImageCount_;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = this->vulkanHelper_.data.allocator;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = VulkanHelper::check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
    // ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your
    // application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale
    // better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
    // backslash \\ !
    // style.FontSizeBase = 20.0f;
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    // IM_ASSERT(font != nullptr);
}

auto ImPlotEngine::deinit() -> void {
    std::scoped_lock guard(drawers_mutex_);
    if (!this->window_) {
        return;
    }

    // Cleanup
    VkResult err = vkDeviceWaitIdle(this->vulkanHelper_.data.device);
    VulkanHelper::check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    this->vulkanHelper_.Cleanup();

    glfwDestroyWindow(this->window_);
    this->window_ = nullptr;
    glfwTerminate();
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
auto ImPlotEngine::SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width,
                                     int height) -> void {
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(this->vulkanHelper_.data.physicalDevice, this->vulkanHelper_.data.queueFamily,
                                         wd->Surface, &res);
    if (res != VK_TRUE) {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        this->vulkanHelper_.data.physicalDevice, wd->Surface, requestSurfaceImageFormat,
        (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
                                        VK_PRESENT_MODE_FIFO_KHR};
#else
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(this->vulkanHelper_.data.physicalDevice, wd->Surface,
                                                          &present_modes[0], IM_ARRAYSIZE(present_modes));
    // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(this->minImageCount_ >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(this->vulkanHelper_.data.instance, this->vulkanHelper_.data.physicalDevice,
                                           this->vulkanHelper_.data.device, wd, this->vulkanHelper_.data.queueFamily,
                                           this->vulkanHelper_.data.allocator, width, height, this->minImageCount_, 0);
}

auto ImPlotEngine::CleanupVulkanWindow() -> void {
    ImGui_ImplVulkanH_DestroyWindow(this->vulkanHelper_.data.instance, this->vulkanHelper_.data.device,
                                    &this->mainWindowData_, this->vulkanHelper_.data.allocator);
}

auto ImPlotEngine::FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data) -> void {
    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(this->vulkanHelper_.data.device, wd->Swapchain, UINT64_MAX,
                                         image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        this->swapChainRebuild_ = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        VulkanHelper::check_vk_result(err);

    ImGui_ImplVulkanH_Frame *fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(this->vulkanHelper_.data.device, 1, &fd->Fence, VK_TRUE,
                              UINT64_MAX);  // wait indefinitely instead of periodically checking
        VulkanHelper::check_vk_result(err);

        err = vkResetFences(this->vulkanHelper_.data.device, 1, &fd->Fence);
        VulkanHelper::check_vk_result(err);
    }
    {
        err = vkResetCommandPool(this->vulkanHelper_.data.device, fd->CommandPool, 0);
        VulkanHelper::check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        VulkanHelper::check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        VulkanHelper::check_vk_result(err);
        err = vkQueueSubmit(this->vulkanHelper_.data.queue, 1, &info, fd->Fence);
        VulkanHelper::check_vk_result(err);
    }
}

auto ImPlotEngine::FramePresent(ImGui_ImplVulkanH_Window *wd) -> void {
    if (this->swapChainRebuild_)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(this->vulkanHelper_.data.queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        this->swapChainRebuild_ = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        VulkanHelper::check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;  // Now we can use the next set of semaphores
}

auto ImPlotEngine::show_async() -> void {
    show_thread_ = std::jthread([this](std::stop_token st) {
        this->stop_token_ = st;
        this->show();
    });
}

auto ImPlotEngine::show_stop() -> void {
    if (show_thread_.joinable()) {
        show_thread_.request_stop();
    }
}

auto ImPlotEngine::show_wait() -> void {
    if (show_thread_.joinable()) {
        show_thread_.join();
    }
}

auto ImPlotEngine::show_detach() -> void {
    if (show_thread_.joinable()) {
        show_thread_.detach();
    }
}

auto ImPlotEngine::show(std::optional<std::string> title, bool clear_entries) -> void {
    {
        std::scoped_lock guard(drawers_mutex_);
        if (title.has_value()) {
            this->title_ = title.value();
            if (this->window_) {
                glfwSetWindowTitle(this->window_, title->c_str());
            }
        }
        if (!this->window_) {
            this->init(this->title_);
        }
    }
    // Our state
    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(this->window_)) {
        if (this->stop_token_.stop_requested()) {
            break;
        }
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
        // inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or
        // clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
        // clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and
        // hide them from your application based on those two flags.
        glfwPollEvents();

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(this->window_, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 &&
            (this->swapChainRebuild_ || this->mainWindowData_.Width != fb_width ||
             this->mainWindowData_.Height != fb_height)) {
            ImGui_ImplVulkan_SetMinImageCount(this->minImageCount_);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                this->vulkanHelper_.data.instance, this->vulkanHelper_.data.physicalDevice,
                this->vulkanHelper_.data.device, &this->mainWindowData_, this->vulkanHelper_.data.queueFamily,
                this->vulkanHelper_.data.allocator, fb_width, fb_height, this->minImageCount_, 0);
            this->mainWindowData_.FrameIndex = 0;
            this->swapChainRebuild_ = false;
        }
        if (glfwGetWindowAttrib(this->window_, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code
        // to learn more about Dear ImGui!).
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
            ImPlot::ShowDemoWindow();
        }

        auto snap = this->drawers_.load(std::memory_order_acquire);
        if (!snap)
            break;

        for (const auto &item : *snap) {
            item->fn();
        }

        // Rendering
        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            this->mainWindowData_.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            this->mainWindowData_.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            this->mainWindowData_.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            this->mainWindowData_.ClearValue.color.float32[3] = clear_color.w;
            FrameRender(&this->mainWindowData_, draw_data);
            FramePresent(&this->mainWindowData_);
        }
    }

    {
        std::scoped_lock guard(drawers_mutex_);
        this->deinit();
        if (clear_entries) {
            this->remove_drawers();
        }
    }
}

auto ImPlotEngine::draw(EntryPtr item) -> uint32_t {
    std::scoped_lock guard(drawers_mutex_);
    auto snap = this->drawers_.load(std::memory_order_acquire);

    std::shared_ptr<std::vector<EntryPtr>> next;
    if (!snap)
        next = std::make_shared<std::vector<EntryPtr>>();
    else
        next = std::make_shared<std::vector<EntryPtr>>(*snap);

    item->id = ++this->lastDrawerId_;
    next->push_back(std::move(item));

    drawers_.store(std::shared_ptr<const std::vector<EntryPtr>>(std::move(next)), std::memory_order_release);

    return this->lastDrawerId_;
}

auto ImPlotEngine::remove_drawer(uint32_t id) -> void {
    std::scoped_lock guard(drawers_mutex_);
    auto snap = this->drawers_.load(std::memory_order_acquire);

    if (!snap)
        return;

    auto next = std::make_shared<std::vector<EntryPtr>>(*snap);

    bool erased = false;
    for (auto it = next->begin(); it != next->end();) {
        if ((*it)->id == id) {
            it = next->erase(it);
            erased = true;
            break;
        } else {
            ++it;
        }
    }

    if (erased) {
        drawers_.store(std::shared_ptr<const std::vector<EntryPtr>>(std::move(next)), std::memory_order_release);
    }
}

auto ImPlotEngine::remove_drawer(const std::string &name) -> void {
    std::scoped_lock guard(drawers_mutex_);
    auto snap = this->drawers_.load(std::memory_order_acquire);

    if (!snap)
        return;

    auto next = std::make_shared<std::vector<EntryPtr>>(*snap);

    auto old_size = next->size();
    std::erase_if(*next, [&](const EntryPtr &item) { return item->key == name; });

    if (next->size() != old_size) {
        drawers_.store(std::shared_ptr<const std::vector<EntryPtr>>(std::move(next)), std::memory_order_release);
    }
}

auto ImPlotEngine::remove_drawers() -> void {
    std::scoped_lock guard(drawers_mutex_);

    drawers_.store(std::make_shared<const std::vector<EntryPtr>>(), std::memory_order_release);
}