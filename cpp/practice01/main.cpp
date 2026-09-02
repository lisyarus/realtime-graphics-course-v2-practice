#include <wgpu_app.hpp>
#include <file_utils.hpp>

#include <webgpu.h>

#include <exception>
#include <filesystem>
#include <iostream>

static std::filesystem::path const projectRoot = PROJECT_ROOT;

int main() try {
    WgpuApp app("Practice01", 1280, 720, false);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app.resize(event.window.data1, event.window.data2);
                break;
            }
        }

        std::optional<WGPUSurfaceTexture> surfaceTexture = app.beginFrame();
        if (!surfaceTexture)
            continue;

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        // Frame rendering code goes here

        wgpuTextureViewRelease(targetView);

        wgpuSurfacePresent(app.surface());
        wgpuTextureRelease(surfaceTexture->texture);
    }
} catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
