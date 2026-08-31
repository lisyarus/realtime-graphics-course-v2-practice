#pragma once

#include <SDL3/SDL.h>
#include <webgpu.h>

#include <string_view>
#include <optional>

class WgpuApp {
public:
    WgpuApp(std::string_view title, int width, int height, bool srgb);
    ~WgpuApp();

    WgpuApp(const WgpuApp &) = delete;
    WgpuApp &operator=(const WgpuApp &) = delete;

    SDL_Window *window() const { return window_; }
    WGPUDevice device() const { return device_; }
    WGPUQueue queue() const { return queue_; }
    WGPUTextureFormat surfaceFormat() const { return config_.format; }
    int width() const { return static_cast<int>(config_.width); }
    int height() const { return static_cast<int>(config_.height); }

    void resize(int width, int height);

    std::optional<WGPUSurfaceTexture> beginFrame();
    WGPUSurface surface() const { return surface_; }

private:
    void init(std::string_view title, int width, int height, bool srgb);
    void destroy();

    SDL_Window *window_ = nullptr;

#ifdef __APPLE__
    SDL_MetalView metalView_ = nullptr;
#endif

    WGPUInstance instance_ = nullptr;
    WGPUSurface surface_ = nullptr;
    WGPUAdapter adapter_ = nullptr;
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;
    WGPUSurfaceConfiguration config_{};
};
