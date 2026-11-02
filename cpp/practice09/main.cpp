#include <wgpu_app.hpp>
#include <file_utils.hpp>
#include <image_utils.hpp>
#include <obj_loader.hpp>
#include <math/math.hpp>

#include <webgpu.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <unordered_set>

static std::filesystem::path const projectRoot = PROJECT_ROOT;

struct CameraUniforms
{
    math::matrix4f viewProjectionTranspose;
    math::vector3f position;
    float padding = 0.f;
};

struct LightsUniforms
{
    math::matrix4f shadowProjectionTranspose;
    math::vector3f ambientIntensity;
    float padding = 0.f;
    math::vector3f lightDirection;
    float padding2 = 0.f;
    math::vector3f lightIntensity;
    float padding3 = 0.f;
};

struct Immediates
{
    math::matrix4f modelTranspose;
    math::vector3f albedo;
    float roughness;
    float specularFactor;
};

struct DebugDepthTextureImmediates
{
    math::vector2f center;
    math::vector2f extent;
};

struct ShadowImmediates
{
    math::matrix4f shadowProjectionTranspose;
};

WGPUShaderModule createShaderModule(WGPUDevice device, std::filesystem::path const & path) {
    auto const source = loadFile(path);

    WGPUShaderSourceWGSL shaderSourceWGSL = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSourceWGSL.code = {source.data(), source.size()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain = &shaderSourceWGSL.chain;

    return wgpuDeviceCreateShaderModule(device, &shaderModuleDescriptor);
}

WGPUBuffer createBuffer(WGPUDevice device, std::size_t size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.size = size;
    bufferDescriptor.usage = usage | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &bufferDescriptor);
}

template <typename T>
WGPUBuffer loadBuffer(WGPUDevice device, WGPUQueue queue, std::vector<T> const & data, WGPUBufferUsage usage) {
    auto buffer = createBuffer(device, data.size() * sizeof(T), usage);
    wgpuQueueWriteBuffer(queue, buffer, 0, data.data(), data.size() * sizeof(T));
    return buffer;
}

WGPUBindGroupLayout createCameraBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry layoutEntries[1] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    layoutEntries[0].binding = 0;
    layoutEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    layoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindGroupLayoutDescriptor.entries = layoutEntries;
    bindGroupLayoutDescriptor.entryCount = std::size(layoutEntries);

    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
}

WGPUBindGroupLayout createLightsBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry layoutEntries[3] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    layoutEntries[0].binding = 0;
    layoutEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    layoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    layoutEntries[1].binding = 1;
    layoutEntries[1].visibility = WGPUShaderStage_Fragment;
    layoutEntries[1].texture.sampleType = WGPUTextureSampleType_Depth;
    layoutEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    layoutEntries[2].binding = 2;
    layoutEntries[2].visibility = WGPUShaderStage_Fragment;
    layoutEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindGroupLayoutDescriptor.entries = layoutEntries;
    bindGroupLayoutDescriptor.entryCount = std::size(layoutEntries);

    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
}

WGPUBindGroup createCameraBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUBuffer uniformBuffer) {
    WGPUBindGroupEntry entries[1] = {WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].size = WGPU_WHOLE_SIZE;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

WGPUBindGroup createLightsBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUBuffer uniformBuffer, WGPUTextureView shadowMap, WGPUSampler sampler) {
    WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].size = WGPU_WHOLE_SIZE;
    entries[1].binding = 1;
    entries[1].textureView = shadowMap;
    entries[2].binding = 2;
    entries[2].sampler = sampler;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

WGPURenderPipeline createPipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat,
        WGPUBindGroupLayout cameraBindGroupLayout, WGPUBindGroupLayout lightsBindGroupLayout) {
    WGPUBindGroupLayout bindGroupLayouts[2] = {
        cameraBindGroupLayout,
        lightsBindGroupLayout,
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(Immediates);
    pipelineLayoutDescriptor.bindGroupLayouts = bindGroupLayouts;
    pipelineLayoutDescriptor.bindGroupLayoutCount = std::size(bindGroupLayouts);

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUVertexAttribute attributes[3] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = 12;
    attributes[1].shaderLocation = 1;
    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[2].offset = 24;
    attributes[2].shaderLocation = 2;
    attributes[2].format = WGPUVertexFormat_Float32x2;

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.arrayStride = sizeof(ObjVertex);
    vertexBufferLayout.attributeCount = std::size(attributes);
    vertexBufferLayout.attributes = attributes;
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.primitive.cullMode = WGPUCullMode_Back;
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPURenderPipeline createDebugDepthTexturePipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat,
        WGPUBindGroupLayout lightsBindGroupLayout) {
    WGPUBindGroupLayout bindGroupLayouts[1] = {
        lightsBindGroupLayout,
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(DebugDepthTextureImmediates);
    pipelineLayoutDescriptor.bindGroupLayouts = bindGroupLayouts;
    pipelineLayoutDescriptor.bindGroupLayoutCount = std::size(bindGroupLayouts);

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_False;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPURenderPipeline createShadowPipeline(WGPUDevice device, WGPUShaderModule shaderModule) {
    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(ShadowImmediates);

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUVertexAttribute attributes[1] = {WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.arrayStride = sizeof(ObjVertex);
    vertexBufferLayout.attributeCount = std::size(attributes);
    vertexBufferLayout.attributes = attributes;
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPUTexture createShadowMap(WGPUDevice device, std::uint32_t size, WGPUTextureFormat format)
{
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.size = {size, size, 1};
    descriptor.format = format;
    return wgpuDeviceCreateTexture(device, &descriptor);
}

WGPUSampler createShadowSampler(WGPUDevice device)
{
    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.addressModeU = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeV = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeW = WGPUAddressMode_ClampToEdge;
    descriptor.magFilter = WGPUFilterMode_Nearest;
    descriptor.minFilter = WGPUFilterMode_Nearest;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    return wgpuDeviceCreateSampler(device, &descriptor);
}

int main() try {
    WgpuApp app("Practice09", 1280, 720, true);

    WGPUBindGroupLayout cameraBindGroupLayout = createCameraBindGroupLayout(app.device());
    WGPUBindGroupLayout lightsBindGroupLayout = createLightsBindGroupLayout(app.device());

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPUShaderModule debugDepthTextureShaderModule = createShaderModule(app.device(), projectRoot / "debug_depth_texture_shader.wgsl");
    WGPUShaderModule shadowShaderModule = createShaderModule(app.device(), projectRoot / "shadow_shader.wgsl");

    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat(), cameraBindGroupLayout, lightsBindGroupLayout);
    WGPURenderPipeline debugDepthTextureRenderPipeline = createDebugDepthTexturePipeline(app.device(), debugDepthTextureShaderModule, app.surfaceFormat(), lightsBindGroupLayout);
    WGPURenderPipeline shadowPipeline = createShadowPipeline(app.device(), shadowShaderModule);

    WGPUBuffer cameraUniformBuffer = createBuffer(app.device(), sizeof(CameraUniforms), WGPUBufferUsage_Uniform);
    WGPUBuffer lightsUniformBuffer = createBuffer(app.device(), sizeof(LightsUniforms), WGPUBufferUsage_Uniform);

    WGPUTexture shadowMap = createShadowMap(app.device(), 1024, WGPUTextureFormat_Depth24Plus);
    WGPUTextureView shadowMapView = wgpuTextureCreateView(shadowMap, nullptr);
    WGPUSampler shadowSampler = createShadowSampler(app.device());

    WGPUBindGroup cameraBindGroup = createCameraBindGroup(app.device(), cameraBindGroupLayout, cameraUniformBuffer);
    WGPUBindGroup lightsBindGroup = createLightsBindGroup(app.device(), lightsBindGroupLayout, lightsUniformBuffer, shadowMapView, shadowSampler);

    ObjMesh teapot = loadObj(projectRoot / "teapot.obj");

    auto vertexBuffer = loadBuffer(app.device(), app.queue(), teapot.vertices, WGPUBufferUsage_Vertex);
    auto indexBuffer = loadBuffer(app.device(), app.queue(), teapot.indices, WGPUBufferUsage_Index);

    WGPUTexture depthBuffer = nullptr;
    WGPUTextureView depthBufferView = nullptr;

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    float camera_distance = 4.f;
    float camera_rotation = std::numbers::pi_v<float>;
    float camera_height = 0.5f;

    bool paused = false;

    std::unordered_set<SDL_Keycode> keydown;

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
            case SDL_EVENT_KEY_DOWN:
                keydown.insert(event.key.key);
                if (event.key.key == SDLK_SPACE)
                    paused ^= true;
                break;
            case SDL_EVENT_KEY_UP:
                keydown.erase(event.key.key);
                break;
            }
        }

        std::optional<WGPUSurfaceTexture> surfaceTexture = app.beginFrame();
        if (!surfaceTexture) {
            continue;
        }

        if (!depthBuffer || wgpuTextureGetWidth(depthBuffer) != app.width() || wgpuTextureGetHeight(depthBuffer) != app.height()) {
            if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
            if (depthBuffer) wgpuTextureRelease(depthBuffer);

            WGPUTextureDescriptor depthBufferDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
            depthBufferDescriptor.usage = WGPUTextureUsage_RenderAttachment;
            depthBufferDescriptor.dimension = WGPUTextureDimension_2D;
            depthBufferDescriptor.size = {(std::uint32_t)app.width(), (std::uint32_t)app.height(), 1};
            depthBufferDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBuffer = wgpuDeviceCreateTexture(app.device(), &depthBufferDescriptor);

            WGPUTextureViewDescriptor depthBufferViewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
            depthBufferViewDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBufferViewDescriptor.dimension = WGPUTextureViewDimension_2D;
            depthBufferViewDescriptor.mipLevelCount = 1;
            depthBufferViewDescriptor.arrayLayerCount = 1;
            depthBufferViewDescriptor.aspect = WGPUTextureAspect_DepthOnly;
            depthBufferViewDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TransientAttachment;
            depthBufferView = wgpuTextureCreateView(depthBuffer, &depthBufferViewDescriptor);
        }

        auto const now = std::chrono::high_resolution_clock::now();
        float const dt = std::chrono::duration<float>(now - lastFrameStart).count();
        if (!paused) time += dt;
        lastFrameStart = now;

        if (keydown.contains(SDLK_LEFT)) camera_rotation += 5.f * dt;
        if (keydown.contains(SDLK_RIGHT)) camera_rotation -= 5.f * dt;
        if (keydown.contains(SDLK_UP)) camera_distance += 7.f * dt;
        if (keydown.contains(SDLK_DOWN)) camera_distance -= 7.f * dt;

        math::matrix4f const model = math::matrix4f::identity();
        math::matrix4f const view = math::translation(math::vector{0.f, 0.f, - camera_distance})
            * math::rotation_yz(std::numbers::pi_v<float> / 6.f)
            * math::rotation_xz(camera_rotation)
            * math::translation(math::vector{0.f, - camera_height, 0.f});
        math::matrix4f const projection = math::perspective(std::numbers::pi_v<float> / 3.f, app.width() * 1.f / app.height(), 0.01f, 100.f);

        math::vector3f const lightDirection = math::normalized(math::vector{std::cos(time * 0.5f), 1.f, std::sin(time * 0.5f)});

        math::matrix4f shadowProjection;
        {
            math::vector3f lightZ = -lightDirection;
            math::vector3f lightX = math::normalized(math::cross(lightZ, math::vector3f{0.f, 1.f, 0.f}));
            math::vector3f lightY = - math::cross(lightZ, lightX);

            shadowProjection = math::matrix4f::from_rows(
                math::append_zero(lightX),
                math::append_zero(lightY),
                math::append_zero(lightZ),
                math::append_one(math::vector3f::zero())
            );

            shadowProjection = math::scale(math::vector{1.f, 1.f, 0.5f}) * math::translation(math::vector{0.f, 0.f, 1.f}) * shadowProjection;
        }

        CameraUniforms const cameraUniforms {
            .viewProjectionTranspose = math::transpose(projection * view),
            .position = math::head<3>(math::inverse(view).value() * math::vector{0.f, 0.f, 0.f, 1.f}),
        };

        wgpuQueueWriteBuffer(app.queue(), cameraUniformBuffer, 0, &cameraUniforms, sizeof(cameraUniforms));

        LightsUniforms const lightsUniforms {
            .shadowProjectionTranspose = math::transpose(shadowProjection),
            .ambientIntensity = {0.05f, 0.10f, 0.15f},
            .lightDirection = lightDirection,
            .lightIntensity = {0.8f, 0.6f, 0.4f},
        };

        wgpuQueueWriteBuffer(app.queue(), lightsUniformBuffer, 0, &lightsUniforms, sizeof(lightsUniforms));

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        {
            WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
            depthStencilAttachment.view = shadowMapView;
            depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
            depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
            depthStencilAttachment.depthClearValue = 1.f;
            depthStencilAttachment.depthReadOnly = WGPU_FALSE;

            WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
            renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;
            WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

            wgpuRenderPassEncoderSetPipeline(renderPass, shadowPipeline);
            ShadowImmediates const immediates {
                .shadowProjectionTranspose = math::transpose(shadowProjection),
            };
            wgpuRenderPassEncoderSetImmediates(renderPass, 0, &immediates, sizeof(immediates));
            wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(renderPass, teapot.indices.size(), 1, 0, 0, 0);

            wgpuRenderPassEncoderEnd(renderPass);
            wgpuRenderPassEncoderRelease(renderPass);
        }

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.01, 0.03, 0.05, 1.0};

        WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depthStencilAttachment.view = depthBufferView;
        depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
        depthStencilAttachment.depthClearValue = 1.f;
        depthStencilAttachment.depthReadOnly = WGPU_FALSE;

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, cameraBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 1, lightsBindGroup, 0, nullptr);

        Immediates const immediates {
            .modelTranspose = math::matrix4f::identity(),
            .albedo = math::vector{1.f, 1.f, 1.f},
            .roughness = 0.05f,
            .specularFactor = 0.02f,
        };
        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &immediates, sizeof(immediates));
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderDrawIndexed(renderPass, teapot.indices.size(), 1, 0, 0, 0);

        float const R = app.width() * 1.f / app.height();
        float const S = 0.4f;

        wgpuRenderPassEncoderSetPipeline(renderPass, debugDepthTextureRenderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, lightsBindGroup, 0, nullptr);
        DebugDepthTextureImmediates const debugImmediates {
            .center = {- 1.f + S / R, - 1.f + S},
            .extent = {S / R, S},
        };
        wgpuRenderPassEncoderSetImmediates(renderPass, 0, &debugImmediates, sizeof(debugImmediates));
        wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(app.queue(), 1, &commandBuffer);
        wgpuCommandBufferRelease(commandBuffer);

        wgpuSurfacePresent(app.surface());

        wgpuTextureViewRelease(targetView);
        wgpuTextureRelease(surfaceTexture->texture);
    }

    if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
    if (depthBuffer) wgpuTextureRelease(depthBuffer);

    wgpuBufferRelease(vertexBuffer);
    wgpuBufferRelease(indexBuffer);

    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);
} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
