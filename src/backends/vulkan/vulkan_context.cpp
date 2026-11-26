#include "vulkan_context.hpp"

#include <error_context.hpp>

#include <cstring>
#include <vector>

namespace voryx::vulkan_backend
{
void Check(VkResult result)
{
    if (result != VK_SUCCESS) {
        throw NativeFailure{result};
    }
}

Error NativeError(VkResult result, std::string_view operation) noexcept
{
    ErrorCode code = ErrorCode::InternalError;
    if (result == VK_ERROR_OUT_OF_HOST_MEMORY
        || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        code = ErrorCode::OutOfMemory;
    } else if (result == VK_ERROR_DEVICE_LOST) {
        code = ErrorCode::DeviceLost;
    } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
        code = ErrorCode::SurfaceIncompatible;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        code = ErrorCode::SwapchainOutOfDate;
    } else if (result == VK_NOT_READY) {
        code = ErrorCode::NotReady;
    } else if (result == VK_TIMEOUT) {
        code = ErrorCode::Timeout;
    } else if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        code = ErrorCode::Unsupported;
    } else if (result == VK_ERROR_INCOMPATIBLE_DRIVER
               || result == VK_ERROR_INITIALIZATION_FAILED
               || result == VK_ERROR_EXTENSION_NOT_PRESENT
               || result == VK_ERROR_LAYER_NOT_PRESENT
               || result == VK_ERROR_FEATURE_NOT_PRESENT) {
        code = ErrorCode::BackendUnavailable;
    }
    return detail::WithOperation(
        {code, {}, {}, static_cast<std::int64_t>(result)}, operation);
}

namespace
{
bool SupportsPresent(VkPhysicalDevice candidate, const SurfaceSource &source)
{
    std::uint32_t count = 0;
    Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count,
                                               nullptr));
    std::vector<VkExtensionProperties> extensions(count);
    Check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count,
                                               extensions.data()));
    extensions.resize(count);
    const auto has = [&](const char *name) {
        for (const auto &extension : extensions) {
            if (std::strcmp(extension.extensionName, name) == 0) {
                return true;
            }
        }
        return false;
    };
    for (const auto *extension : source.deviceExtensions) {
        if (!has(extension)) {
            return false;
        }
    }
    return source.supports(candidate);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
             VkDebugUtilsMessageTypeFlagsEXT,
             const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData)
{
    const auto &log = *static_cast<LogSink *>(userData);
    const auto level =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            ? LogLevel::Error
            : LogLevel::Warning;
    if (data != nullptr && data->pMessage != nullptr) {
        log.Write(level, data->pMessage);
    }
    return VK_FALSE;
}
} // namespace

Context::Context(const DeviceDesc &desc)
    : log(desc.logCallback, desc.logUserData)
{
}

void Context::DestroyDevice() noexcept
{
    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    queue = VK_NULL_HANDLE;
}

Context::~Context()
{
    if (device != VK_NULL_HANDLE) {
        // 同步执行器正常返回前已完成工作；丢失设备也只做尽力清理。
        vkDeviceWaitIdle(device);
        DestroyDevice();
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (messenger != VK_NULL_HANDLE) {
        const auto destroy =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance,
                                      "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr) {
            destroy(instance, messenger, nullptr);
        }
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}

void Context::Initialize(const DeviceDesc &desc, const SurfaceSource *source)
{
    const char *layer = "VK_LAYER_KHRONOS_validation";
    std::vector<const char *> extensions;
    if (source != nullptr) {
        extensions.assign(source->extensions.begin(), source->extensions.end());
    }
    if (desc.enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        std::uint32_t count = 0;
        Check(vkEnumerateInstanceLayerProperties(&count, nullptr));
        std::vector<VkLayerProperties> layers(count);
        Check(vkEnumerateInstanceLayerProperties(&count, layers.data()));
        layers.resize(count);
        bool found = false;
        for (const auto &candidate : layers) {
            if (std::strcmp(candidate.layerName, layer) == 0) {
                found = true;
            }
        }
        if (!found) {
            throw NativeFailure{VK_ERROR_LAYER_NOT_PRESENT};
        }
    }
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "VORYX";
    app.pEngineName = "VORYX";
    app.apiVersion = VK_API_VERSION_1_2;
    VkDebugUtilsMessengerCreateInfoEXT debug{};
    debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug.pfnUserCallback = DebugMessage;
    debug.pUserData = &log;
    const VkValidationFeatureEnableEXT feature =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{};
    validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation.pNext = &debug;
    validation.enabledValidationFeatureCount = 1;
    validation.pEnabledValidationFeatures = &feature;
    VkInstanceCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create.pApplicationInfo = &app;
    create.enabledExtensionCount =
        static_cast<std::uint32_t>(extensions.size());
    create.ppEnabledExtensionNames = extensions.data();
    if (desc.enableValidation) {
        create.enabledLayerCount = 1;
        create.ppEnabledLayerNames = &layer;
        create.pNext = &validation;
    }
    VkInstance createdInstance = VK_NULL_HANDLE;
    Check(vkCreateInstance(&create, nullptr, &createdInstance));
    instance = createdInstance;
    if (desc.enableValidation) {
        const auto make = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (make == nullptr) {
            throw NativeFailure{VK_ERROR_EXTENSION_NOT_PRESENT};
        }
        VkDebugUtilsMessengerEXT createdMessenger = VK_NULL_HANDLE;
        Check(make(instance, &debug, nullptr, &createdMessenger));
        messenger = createdMessenger;
    }
    if (source != nullptr) {
        surface = source->create(instance, source->window);
    }
    std::uint32_t count = 0;
    Check(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    std::vector<VkPhysicalDevice> devices(count);
    Check(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    devices.resize(count);
    int best = -1;
    for (const auto candidate : devices) {
        VkPhysicalDeviceProperties propertiesCandidate{};
        vkGetPhysicalDeviceProperties(candidate, &propertiesCandidate);
        if (propertiesCandidate.apiVersion < VK_API_VERSION_1_2
            || (!desc.allowSoftwareAdapter
                && propertiesCandidate.deviceType
                       == VK_PHYSICAL_DEVICE_TYPE_CPU)) {
            continue;
        }
        if (source != nullptr && !SupportsPresent(candidate, *source)) {
            continue;
        }
        std::uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount,
                                                 nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount,
                                                 families.data());
        for (std::uint32_t family = 0; family < familyCount; ++family) {
            if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0
                || families[family].queueCount == 0) {
                continue;
            }
            if (surface != VK_NULL_HANDLE) {
                VkBool32 canPresent = VK_FALSE;
                Check(vkGetPhysicalDeviceSurfaceSupportKHR(
                    candidate, family, surface, &canPresent));
                if (!canPresent) {
                    continue;
                }
            }
            const int score =
                propertiesCandidate.deviceType
                        == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                    ? 3
                : propertiesCandidate.deviceType
                        == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                    ? 2
                : propertiesCandidate.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU
                    ? 0
                    : 1;
            if (score > best) {
                best = score;
                physical = candidate;
                queueFamily = family;
                properties = propertiesCandidate;
            }
        }
    }
    if (physical == VK_NULL_HANDLE) {
        throw NativeFailure{VK_ERROR_FEATURE_NOT_PRESENT};
    }
    vkGetPhysicalDeviceMemoryProperties(physical, &memoryProperties);
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueCreate{};
    queueCreate.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreate.queueFamilyIndex = queueFamily;
    queueCreate.queueCount = 1;
    queueCreate.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceCreate{};
    deviceCreate.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreate.queueCreateInfoCount = 1;
    deviceCreate.pQueueCreateInfos = &queueCreate;
    if (source != nullptr) {
        deviceCreate.pNext = source->deviceFeatures;
        deviceCreate.enabledExtensionCount =
            static_cast<std::uint32_t>(source->deviceExtensions.size());
        deviceCreate.ppEnabledExtensionNames = source->deviceExtensions.data();
    }
    VkDevice createdDevice = VK_NULL_HANDLE;
    Check(vkCreateDevice(physical, &deviceCreate, nullptr, &createdDevice));
    device = createdDevice;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
}

VkDeviceMemory AllocateImageMemory(const Context &context, VkImage image)
{
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, image, &requirements);
    std::uint32_t selected = UINT32_MAX;
    for (std::uint32_t index = 0;
         index < context.memoryProperties.memoryTypeCount; ++index) {
        if ((requirements.memoryTypeBits & (1u << index)) == 0) {
            continue;
        }
        const auto flags =
            context.memoryProperties.memoryTypes[index].propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            selected = index;
            break;
        }
        if (selected == UINT32_MAX) {
            selected = index;
        }
    }
    if (selected == UINT32_MAX) {
        throw NativeFailure{VK_ERROR_FEATURE_NOT_PRESENT};
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = selected;
    VkDeviceMemory allocated = VK_NULL_HANDLE;
    Check(vkAllocateMemory(context.device, &allocation, nullptr, &allocated));
    return allocated;
}
} // namespace voryx::vulkan_backend
