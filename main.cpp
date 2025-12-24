#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <cstdint>   // Necessary for uint32_t
#include <limits>    // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

#include <fstream>   // for loading the shaders bytecode

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  vk::raii::Context                context;
  vk::raii::Instance               instance =       nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::SurfaceKHR             surface =        nullptr;
  vk::raii::PhysicalDevice         physicalDevice = nullptr;
  vk::raii::Device                 device =         nullptr;
  vk::raii::Queue                  graphicsQueue =  nullptr;
  vk::raii::Queue                  presentQueue =   nullptr;
  vk::raii::SwapchainKHR           swapChain =      nullptr;

  std::vector<vk::Image>           swapChainImages;
  std::vector<vk::raii::ImageView> swapChainImageViews;
  vk::Format                       swapChainImageFormat = vk::Format::eUndefined;
  vk::Extent2D                     swapChainExtent;

  GLFWwindow* window = nullptr;

  std::vector<const char*> requiredDeviceExtensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::KHRCreateRenderpass2ExtensionName
  };

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
  }

  void createInstance() {
    constexpr vk::ApplicationInfo appInfo{
      .pApplicationName =   "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
      .pEngineName =        "No Engine",
      .engineVersion =      VK_MAKE_VERSION( 1, 0, 0 ),
      .apiVersion =         vk::ApiVersion14
    };

    // Get validation layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if layers are supported
    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(
      requiredLayers,
      [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(
          layerProperties,
          [requiredLayer] (auto const& layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
          }
        );
      }
    )) {
      throw std::runtime_error("One or more required layers are not supported!");
    }

    // Get required extensions
    auto requiredExtensions =  getRequiredExtensions();
    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    // Print extensions
    std::cout << "available extensions:\n";

    for (const auto& extension : extensionProperties) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    // Check if extensions are supported
    for (const auto& extension : requiredExtensions) {
      if (std::ranges::none_of(
        extensionProperties,
        [extension] (auto const& extensionProperty) { return strcmp(extensionProperty.extensionName, extension) == 0; }
      )) {
        throw std::runtime_error("Required GLFW extensions not supported: " + std::string(extension));
      }
    }

    vk::InstanceCreateInfo createInfo{
      .pApplicationInfo =        &appInfo,
      .enabledLayerCount =       static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames =     requiredLayers.data(),
      .enabledExtensionCount =   static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data()
    };

    instance = vk::raii::Instance(context, createInfo);
  }

  std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =         glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
    // explanation for configuration at: https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/02_Validation_layers.html#_message_callback
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError  );
    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral  | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
    vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{
      .messageSeverity = severityFlags,
      .messageType = messageTypeFlags,
      .pfnUserCallback = &debugCallback
      };
    debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  void pickPhysicalDevice() {
    auto devices = instance.enumeratePhysicalDevices();

    if (devices.empty()) {
      throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    const auto devIter = std::ranges::find_if(devices, [&](const auto& device) { return isDeviceSuitable(device); });

    if (devIter == devices.end()) {
      throw std::runtime_error("failed to find a suitable GPU!");
    } else {
      physicalDevice = *devIter;
    }
  }

  bool isDeviceSuitable(vk::raii::PhysicalDevice device) {
    auto queueFamilies = device.getQueueFamilyProperties();
    bool isSuitable =    device.getProperties().apiVersion >= VK_API_VERSION_1_3;
    const auto qfpIter = std::ranges::find_if( queueFamilies, [](vk::QueueFamilyProperties const & qfp) {
      return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
    });

    isSuitable = isSuitable && (qfpIter != queueFamilies.end());

    auto extensions = device.enumerateDeviceExtensionProperties();
    bool found =      true;

    for (const auto& extension : requiredDeviceExtensions) {
      auto extensionIter = std::ranges::find_if(extensions, [extension](const auto& ext) { return strcmp(ext.extensionName, extension) == 0; });
      found = found && extensionIter != extensions.end();
    }

    auto features                 = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters && features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering && features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

    return isSuitable && found;
  }

  void createLogicalDevice() {
    // index of first queue family with graphics support
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // get first index of qfp with graphics support & check if it supports presentation
    auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](const auto &qfp) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
    uint32_t graphicsIndex =           static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
    uint32_t presentIndex =            physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface) ? graphicsIndex : static_cast<uint32_t>(queueFamilyProperties.size());
    float queuePriority =              0.5f;

    // card doesn't support presentation
    if (presentIndex == queueFamilyProperties.size()) {
      // look for another card that supports graphics and presentation
      for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
        if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
          graphicsIndex = static_cast<uint32_t>(i);
          presentIndex  = graphicsIndex;
          break;
        }
      }
      // no card supports both
      if (presentIndex == queueFamilyProperties.size()) {
        // look for a card that supports only presentation
        for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
          if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
            presentIndex = static_cast<uint32_t>( i );
            break;
          }
        }
      }
    }

    if ((graphicsIndex == queueFamilyProperties.size()) || (presentIndex == queueFamilyProperties.size())) {
      throw std::runtime_error("Could not find a queue for graphics or present -> terminating");
    }

    // Create a chain of feature structures
    // old way shown here: https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html
    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
      {},                               // vk::PhysicalDeviceFeatures2
      { .shaderDrawParameters = true }, // shader drawing from vulkan 11 (not mentioned in tutorial)
      { .dynamicRendering = true },     // dynamic rendering from vulkan 1.3
      { .extendedDynamicState = true }  // dynamic state from the extension
    };

    vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
      .queueFamilyIndex = graphicsIndex,
      .queueCount =       1,
      .pQueuePriorities = &queuePriority
    };

    vk::DeviceCreateInfo deviceCreateInfo {
      .pNext =                   &featureChain.get<vk::PhysicalDeviceFeatures2>(), // Vulkan wil see all features because of the chain
      .queueCreateInfoCount =    1,
      .pQueueCreateInfos =       &deviceQueueCreateInfo,
      .enabledExtensionCount =   static_cast<uint32_t>(requiredDeviceExtensions.size()),
      .ppEnabledExtensionNames = requiredDeviceExtensions.data()
    };

    device =        vk::raii::Device(physicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
    presentQueue =  vk::raii::Queue(device, presentIndex, 0);
  }

  void createSwapChain() {
    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto minImageCount =       std::max(3u, surfaceCapabilities.minImageCount);
    auto surfaceFormat =       chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
    auto extent =              chooseSwapExtent(surfaceCapabilities);

    minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) ? surfaceCapabilities.maxImageCount : minImageCount;

    vk::SwapchainCreateInfoKHR swapChainCreateInfo {
      .flags =            vk::SwapchainCreateFlagsKHR(),
      .surface =          surface,
      .minImageCount =    minImageCount,
      .imageFormat =      surfaceFormat.format,
      .imageColorSpace =  surfaceFormat.colorSpace,
      .imageExtent =      extent,
      .imageArrayLayers = 1,
      .imageUsage =       vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform =     surfaceCapabilities.currentTransform,
      .compositeAlpha =   vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode =      chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(surface)),
      .clipped =          true,
      .oldSwapchain =     nullptr
    };

    swapChain =            vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages =      swapChain.getImages();
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent =      extent;
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
      std::clamp<uint32_t>(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
  }

  void createImageViews() {
    swapChainImageViews.clear();

    vk::ImageViewCreateInfo imageViewCreateInfo {
      .viewType =         vk::ImageViewType::e2D,
      .format =           swapChainImageFormat,
      .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    };

    for (auto image : swapChainImages) {
      imageViewCreateInfo.image = image;
      swapChainImageViews.emplace_back( device, imageViewCreateInfo);
    }
  }

  void createGraphicsPipeline() {
    auto shaderCode = readFile("shaders/slang.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shaderModule,
      .pName = "vertMain"
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shaderModule,
      .pName = "fragMain"
    };

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
  }

  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    vk::raii::ShaderModule shaderModule{
      device,
      createInfo
    };

    return shaderModule;
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    swapChain = nullptr; // same issue
    surface =   nullptr; // to avoid a SEGFAULT at DestroySurfaceKHR
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary); // begin reading at end of file

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg()); // allocate buffer with length of file

    file.seekg(0, std::ios::beg); // go to beginning of file
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())); // read all bytes

    file.close();

    return buffer;
  }
};

int main() {
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
