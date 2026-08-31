#include <wgpu_app.hpp>

#include <wgpu.h>

#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <wayland-client.h>
#endif

namespace {

std::string toString(WGPUStringView view) {
    if (!view.data) {
        return {};
    }
    if (view.length == WGPU_STRLEN) {
        return std::string(view.data);
    }
    return std::string(view.data, view.length);
}

bool isSrgbFormat(WGPUTextureFormat format) {
    switch (format) {
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
    case WGPUTextureFormat_ASTC4x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x4UnormSrgb:
    case WGPUTextureFormat_ASTC5x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x5UnormSrgb:
    case WGPUTextureFormat_ASTC6x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x5UnormSrgb:
    case WGPUTextureFormat_ASTC8x6UnormSrgb:
    case WGPUTextureFormat_ASTC8x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x5UnormSrgb:
    case WGPUTextureFormat_ASTC10x6UnormSrgb:
    case WGPUTextureFormat_ASTC10x8UnormSrgb:
    case WGPUTextureFormat_ASTC10x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x10UnormSrgb:
    case WGPUTextureFormat_ASTC12x12UnormSrgb:
        return true;
    default:
        return false;
    }
}

WGPUTextureFormat selectSurfaceFormat(WGPUSurfaceCapabilities const &capabilities, bool srgb) {
    for (size_t i = 0; i < capabilities.formatCount; ++i) {
        if (isSrgbFormat(capabilities.formats[i]) == srgb) {
            return capabilities.formats[i];
        }
    }
    throw std::runtime_error(srgb ? "Surface has no sRGB format"
                                 : "Surface has no linear format");
}

WGPUAdapter requestAdapter(WGPUInstance instance, WGPUSurface surface) {
    struct Result {
        WGPUAdapter adapter = nullptr;
        std::string message;
    } result;

    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.compatibleSurface = surface;

    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.userdata1 = &result;
    callback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           WGPUStringView message, void *userdata, void *) {
        auto &result = *static_cast<Result *>(userdata);
        if (status == WGPURequestAdapterStatus_Success) {
            result.adapter = adapter;
        } else {
            result.message = toString(message);
        }
    };

    wgpuInstanceRequestAdapter(instance, &options, callback);
    if (!result.adapter) {
        throw std::runtime_error("No compatible wgpu adapter: " + result.message);
    }
    return result.adapter;
}

WGPUDevice requestDevice(WGPUAdapter adapter) {
    struct Result {
        WGPUDevice device = nullptr;
        std::string message;
    } result;

    WGPUFeatureName const features[] = {
        static_cast<WGPUFeatureName>(WGPUNativeFeature_Immediates),
    };
    WGPULimits limits = WGPU_LIMITS_INIT;
    limits.maxImmediateSize = 128;

    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.label = {"device", WGPU_STRLEN};
    descriptor.requiredFeatureCount = 1;
    descriptor.requiredFeatures = features;
    descriptor.requiredLimits = &limits;

    WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.userdata1 = &result;
    callback.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                           WGPUStringView message, void *userdata, void *) {
        auto &result = *static_cast<Result *>(userdata);
        if (status == WGPURequestDeviceStatus_Success) {
            result.device = device;
        } else {
            result.message = toString(message);
        }
    };

    wgpuAdapterRequestDevice(adapter, &descriptor, callback);
    if (!result.device) {
        throw std::runtime_error("Failed to create wgpu device: " + result.message);
    }
    return result.device;
}

}  // namespace

WgpuApp::WgpuApp(std::string_view title, int width, int height, bool srgb) {
    try {
        init(title, width, height, srgb);
    } catch (...) {
        destroy();
        throw;
    }
}

void WgpuApp::init(std::string_view title, int width, int height, bool srgb) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
    }

    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    #if defined(__APPLE__)
        windowFlags |= SDL_WINDOW_METAL;
    #else
        windowFlags |= SDL_WINDOW_VULKAN;
    #endif

    std::string titleStr(title);
    window_ = SDL_CreateWindow(titleStr.c_str(), width, height, windowFlags);
    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    }

    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window_);

    WGPUInstanceExtras instanceExtras = {};
    instanceExtras.chain.sType = static_cast<WGPUSType>(WGPUSType_InstanceExtras);
    instanceExtras.backends = WGPUInstanceBackend_Primary;

    WGPUInstanceDescriptor instanceDescriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
    instanceDescriptor.nextInChain = &instanceExtras.chain;

    instance_ = wgpuCreateInstance(&instanceDescriptor);
    if (!instance_) {
        throw std::runtime_error("Failed to create wgpu instance");
    }

#if defined(_WIN32)
    WGPUSurfaceDescriptor surfaceDescriptor = WGPU_SURFACE_DESCRIPTOR_INIT;

    HWND hwnd = (HWND)SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    WGPUSurfaceSourceWindowsHWND surfaceSource = {};
    surfaceSource.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    surfaceSource.hinstance = hinstance;
    surfaceSource.hwnd = hwnd;

    surface_ = wgpuInstanceCreateSurface(instance_, &surfaceDescriptor);
#elif defined(__APPLE__)
    WGPUSurfaceDescriptor surfaceDescriptor = WGPU_SURFACE_DESCRIPTOR_INIT;

    metalView_ = SDL_Metal_CreateView(window_);
    if (!metalView_) {
        throw std::runtime_error(std::string("SDL_Metal_CreateView: ") + SDL_GetError());
    }

    WGPUSurfaceSourceMetalLayer surfaceSource = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
    surfaceSource.layer = SDL_Metal_GetLayer(metalView_);

    surfaceDescriptor.nextInChain = &surfaceSource.chain;

    surface_ = wgpuInstanceCreateSurface(instance_, &surfaceDescriptor);
#elif defined(__linux__)
    if (auto *x11Display = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr)) {
        auto x11Window = SDL_GetNumberProperty(windowProperties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

        WGPUSurfaceSourceXlibWindow surfaceSource = {};
        surfaceSource.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        surfaceSource.display = static_cast<Display*>(x11Display);
        surfaceSource.window = static_cast<Window>(x11Window);

        WGPUSurfaceDescriptor surfaceDescriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
        surfaceDescriptor.nextInChain = &surfaceSource.chain;

        surface_ = wgpuInstanceCreateSurface(instance_, &surfaceDescriptor);
    } else if (auto* waylandDisplay = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
        auto* waylandSurface = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);

        WGPUSurfaceSourceWaylandSurface surfaceSource = {};
        surfaceSource.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        surfaceSource.display = static_cast<wl_display*>(waylandDisplay);
        surfaceSource.surface = static_cast<wl_surface*>(waylandSurface);

        WGPUSurfaceDescriptor surfaceDescriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
        surfaceDescriptor.nextInChain = &surfaceSource.chain;

        surface_ = wgpuInstanceCreateSurface(instance_, &surfaceDescriptor);
    } else {
        throw std::runtime_error("Unknown linux display");
    }
#else
    #error Unsupported system
#endif

    adapter_ = requestAdapter(instance_, surface_);
    device_ = requestDevice(adapter_);
    queue_ = wgpuDeviceGetQueue(device_);

    WGPUSurfaceCapabilities capabilities{};
    wgpuSurfaceGetCapabilities(surface_, adapter_, &capabilities);
    if (capabilities.formatCount == 0) {
        wgpuSurfaceCapabilitiesFreeMembers(capabilities);
        throw std::runtime_error("Surface has no supported formats");
    }

    config_ = WGPU_SURFACE_CONFIGURATION_INIT;
    config_.device = device_;
    config_.format = selectSurfaceFormat(capabilities, srgb);
    config_.usage = WGPUTextureUsage_RenderAttachment;
    config_.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);

    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window_, &pixelWidth, &pixelHeight);
    resize(pixelWidth, pixelHeight);
}

WgpuApp::~WgpuApp() {
    destroy();
}

void WgpuApp::destroy() {
    if (surface_) wgpuSurfaceUnconfigure(surface_);
    if (queue_) wgpuQueueRelease(queue_);
    if (device_) wgpuDeviceRelease(device_);
    if (adapter_) wgpuAdapterRelease(adapter_);
    if (surface_) wgpuSurfaceRelease(surface_);
#ifdef __APPLE__
    if (metalView_) SDL_Metal_DestroyView(metalView_);
#endif
    if (window_) SDL_DestroyWindow(window_);
    if (instance_) wgpuInstanceRelease(instance_);
    SDL_Quit();
}

void WgpuApp::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    config_.width = static_cast<uint32_t>(width);
    config_.height = static_cast<uint32_t>(height);
    wgpuSurfaceConfigure(surface_, &config_);
}

std::optional<WGPUSurfaceTexture> WgpuApp::beginFrame() {
    WGPUSurfaceTexture surfaceTexture{};
    wgpuSurfaceGetCurrentTexture(surface_, &surfaceTexture);

    if (static_cast<uint32_t>(surfaceTexture.status) == WGPUSurfaceGetCurrentTextureStatus_Occluded) {
        if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
        return std::nullopt;
    }

    switch (surfaceTexture.status) {
    case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
    case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
        return surfaceTexture;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
        return std::nullopt;
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost: {
        if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window_, &width, &height);
        resize(width, height);
        return std::nullopt;
    }
    default:
        if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
        throw std::runtime_error("Failed to get current surface texture: " + std::to_string(surfaceTexture.status));
    }
}
