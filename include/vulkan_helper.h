#pragma once

#include <atomic>

struct VulkanData {
    VkAllocationCallbacks *allocator = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = UINT32_MAX;
    VkQueue queue = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // Default constructor
    VulkanData() noexcept = default;
};

class VulkanHelper final {
  public:
    VulkanHelper() = default;
    ~VulkanHelper() = default;

    static auto check_vk_result(VkResult err) -> void;
    auto IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties, const char *extension) -> bool;
    auto Setup(ImVector<const char *> instance_extensions) -> void;
    auto Cleanup() -> void;

    VulkanData data;
};