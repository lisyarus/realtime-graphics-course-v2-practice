use std::fs;
use std::mem;
use std::path::Path;
use std::time::Instant;
use std::f32::consts::PI;
use glam::{Vec2, Vec3, Vec4, Mat4};
use wgpu::util::DeviceExt;

use wgpu_app::{run, load_obj, ObjVertex, AppConfig, WgpuApp, WgpuState, KeyCode};

const PROJECT_ROOT: &str = env!("CARGO_MANIFEST_DIR");

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct CameraUniforms
{
    view_projection: Mat4,
    position: Vec3,
    padding: f32,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct LightsUniforms
{
    shadow_projection: Mat4,
    ambient_intensity: Vec3,
    padding1: f32,
    light_direction: Vec3,
    padding2: f32,
    light_intensity: Vec3,
    padding3: f32,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct Immediates
{
    model: Mat4,
    albedo: Vec3,
    roughness: f32,
    specular_factor: f32,
    padding: [f32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct DebugDepthTextureImmediates
{
    center: Vec2,
    extent: Vec2,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct ShadowImmediates
{
    shadow_projection: Mat4,
}

fn load_shader_module(
    device: &wgpu::Device,
    path: impl AsRef<Path>,
) -> std::io::Result<wgpu::ShaderModule> {
    let source = fs::read_to_string(path)?;
    Ok(device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: None,
        source: wgpu::ShaderSource::Wgsl(source.into()),
    }))
}

struct Practice09 {
    depth_buffer: Option<wgpu::Texture>,
    depth_buffer_view: Option<wgpu::TextureView>,
    camera_uniform_buffer: wgpu::Buffer,
    lights_uniform_buffer: wgpu::Buffer,
    shadow_map: wgpu::Texture,
    shadow_map_view: wgpu::TextureView,
    shadow_sampler: wgpu::Sampler,
    camera_bind_group: wgpu::BindGroup,
    lights_bind_group: wgpu::BindGroup,
    pipeline: wgpu::RenderPipeline,
    debug_depth_texture_pipeline: wgpu::RenderPipeline,
    shadow_pipeline: wgpu::RenderPipeline,
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
    index_count: u32,
    last_frame_start: Instant,
    time: f32,
    paused: bool,
    camera_distance: f32,
    camera_rotation: f32,
    camera_height: f32,
}

fn load_buffer<T: bytemuck::Pod>(
    device: &wgpu::Device,
    data: &[T],
    usage: wgpu::BufferUsages,
) -> wgpu::Buffer {
    device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
        label: None,
        contents: bytemuck::cast_slice(data),
        usage: wgpu::BufferUsages::COPY_DST | usage,
    })
}

impl WgpuApp for Practice09 {
    fn new(app: &WgpuState) -> Self {
        let shader = load_shader_module(&app.device, Path::new(PROJECT_ROOT).join("shader.wgsl"))
            .expect("failed to load shader");

        let debug_depth_texture_shader = load_shader_module(&app.device, Path::new(PROJECT_ROOT).join("debug_depth_texture_shader.wgsl"))
            .expect("failed to load shader");

        let shadow_shader = load_shader_module(&app.device, Path::new(PROJECT_ROOT).join("shadow_shader.wgsl"))
            .expect("failed to load shader");

        let camera_bind_group_layout = app.device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: None,
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
            ]
        });

        let lights_bind_group_layout = app.device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: None,
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Depth,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ]
        });

        let camera_uniform_buffer = app.device.create_buffer(&wgpu::BufferDescriptor {
            label: None,
            size: mem::size_of::<CameraUniforms>() as wgpu::BufferAddress,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::UNIFORM,
            mapped_at_creation: false,
        });

        let lights_uniform_buffer = app.device.create_buffer(&wgpu::BufferDescriptor {
            label: None,
            size: mem::size_of::<LightsUniforms>() as wgpu::BufferAddress,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::UNIFORM,
            mapped_at_creation: false,
        });

        let camera_bind_group = app.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: None,
            layout: &camera_bind_group_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: camera_uniform_buffer.as_entire_binding(),
                },
            ],
        });

        let shadow_map = app.device.create_texture(&wgpu::TextureDescriptor {
                label: None,
                dimension: wgpu::TextureDimension::D2,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
                size: wgpu::Extent3d {
                    width: 1024,
                    height: 1024,
                    depth_or_array_layers: 1,
                },
                format: wgpu::TextureFormat::Depth24Plus,
                mip_level_count: 1,
                sample_count: 1,
                view_formats: &[],
            });

        let shadow_map_view = shadow_map.create_view(&wgpu::TextureViewDescriptor::default());

        let shadow_sampler = app.device.create_sampler(&wgpu::SamplerDescriptor {
                label: None,
                address_mode_u: wgpu::AddressMode::ClampToEdge,
                address_mode_v: wgpu::AddressMode::ClampToEdge,
                address_mode_w: wgpu::AddressMode::ClampToEdge,
                mag_filter: wgpu::FilterMode::Nearest,
                min_filter: wgpu::FilterMode::Nearest,
                ..Default::default()
            });

        let lights_bind_group = app.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: None,
            layout: &lights_bind_group_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: lights_uniform_buffer.as_entire_binding(),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::TextureView(&shadow_map_view),
                },
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: wgpu::BindingResource::Sampler(&shadow_sampler),
                },
            ],
        });

        let pipeline_layout = app.device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            bind_group_layouts: &[
                Some(&camera_bind_group_layout),
                Some(&lights_bind_group_layout),
            ],
            immediate_size: mem::size_of::<Immediates>() as u32,
            ..Default::default()
        });

        let pipeline = app.device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: None,
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vertexMain"),
                buffers: &[Some(wgpu::VertexBufferLayout {
                    array_stride: mem::size_of::<ObjVertex>() as wgpu::BufferAddress,
                    step_mode: wgpu::VertexStepMode::Vertex, 
                    attributes: &[
                        wgpu::VertexAttribute {
                            offset: mem::offset_of!(ObjVertex, position) as wgpu::BufferAddress,
                            shader_location: 0,
                            format: wgpu::VertexFormat::Float32x3,
                        },
                        wgpu::VertexAttribute {
                            offset: mem::offset_of!(ObjVertex, normal) as wgpu::BufferAddress,
                            shader_location: 1,
                            format: wgpu::VertexFormat::Float32x3,
                        },
                        wgpu::VertexAttribute {
                            offset: mem::offset_of!(ObjVertex, texcoord) as wgpu::BufferAddress,
                            shader_location: 2,
                            format: wgpu::VertexFormat::Float32x2,
                        },
                    ],
                })],
                compilation_options: Default::default(),
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("fragmentMain"),
                targets: &[Some(wgpu::ColorTargetState {
                    format: app.surface_format(),
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
                compilation_options: Default::default(),
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                cull_mode: Some(wgpu::Face::Back),
                ..Default::default()
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth24Plus,
                depth_write_enabled: Some(true),
                depth_compare: Some(wgpu::CompareFunction::Less),
                stencil: Default::default(),
                bias: Default::default(),
            }),
            multisample: Default::default(),
            multiview_mask: None,
            cache: None,
        });

        let debug_depth_texture_pipeline_layout = app.device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            bind_group_layouts: &[
                Some(&lights_bind_group_layout),
            ],
            immediate_size: mem::size_of::<DebugDepthTextureImmediates>() as u32,
            ..Default::default()
        });

        let debug_depth_texture_pipeline = app.device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: None,
            layout: Some(&debug_depth_texture_pipeline_layout),
            vertex: wgpu::VertexState {
                module: &debug_depth_texture_shader,
                entry_point: Some("vertexMain"),
                buffers: &[],
                compilation_options: Default::default(),
            },
            fragment: Some(wgpu::FragmentState {
                module: &debug_depth_texture_shader,
                entry_point: Some("fragmentMain"),
                targets: &[Some(wgpu::ColorTargetState {
                    format: app.surface_format(),
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
                compilation_options: Default::default(),
            }),
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                ..Default::default()
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth24Plus,
                depth_write_enabled: Some(false),
                depth_compare: Some(wgpu::CompareFunction::Always),
                stencil: Default::default(),
                bias: Default::default(),
            }),
            multisample: Default::default(),
            multiview_mask: None,
            cache: None,
        });

        let shadow_pipeline_layout = app.device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            bind_group_layouts: &[],
            immediate_size: mem::size_of::<ShadowImmediates>() as u32,
            ..Default::default()
        });

        let shadow_pipeline = app.device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: None,
            layout: Some(&shadow_pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shadow_shader,
                entry_point: Some("vertexMain"),
                buffers: &[Some(wgpu::VertexBufferLayout {
                    array_stride: mem::size_of::<ObjVertex>() as wgpu::BufferAddress,
                    step_mode: wgpu::VertexStepMode::Vertex, 
                    attributes: &[
                        wgpu::VertexAttribute {
                            offset: mem::offset_of!(ObjVertex, position) as wgpu::BufferAddress,
                            shader_location: 0,
                            format: wgpu::VertexFormat::Float32x3,
                        },
                    ],
                })],
                compilation_options: Default::default(),
            },
            fragment: None,
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                ..Default::default()
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth24Plus,
                depth_write_enabled: Some(true),
                depth_compare: Some(wgpu::CompareFunction::Less),
                stencil: Default::default(),
                bias: Default::default(),
            }),
            multisample: Default::default(),
            multiview_mask: None,
            cache: None,
        });

        let teapot = load_obj(Path::new(PROJECT_ROOT).join("teapot.obj")).unwrap();

        let vertex_buffer = load_buffer(&app.device, &teapot.vertices, wgpu::BufferUsages::VERTEX);
        let index_buffer = load_buffer(&app.device, &teapot.indices, wgpu::BufferUsages::INDEX);

        Self {
            depth_buffer: None,
            depth_buffer_view: None,
            camera_uniform_buffer,
            lights_uniform_buffer,
            shadow_map,
            shadow_map_view,
            shadow_sampler,
            camera_bind_group,
            lights_bind_group,
            pipeline,
            debug_depth_texture_pipeline,
            shadow_pipeline,
            vertex_buffer,
            index_buffer,
            index_count: teapot.indices.len() as u32,
            last_frame_start: Instant::now(),
            time: 0.0,
            paused: false,
            camera_distance: 4.0,
            camera_rotation: PI,
            camera_height: 0.5,
        }
    }

    fn redraw(&mut self, app: &mut WgpuState) {
        let Some(surface_texture) = app.begin_frame() else {
            return;
        };

        if self.depth_buffer.as_ref().is_none_or(|depth_buffer| depth_buffer.width() != app.width() || depth_buffer.height() != app.height()) {
            let new_depth_buffer = app.device.create_texture(&wgpu::TextureDescriptor {
                label: None,
                dimension: wgpu::TextureDimension::D2,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TRANSIENT_ATTACHMENT,
                size: wgpu::Extent3d {
                    width: app.width(),
                    height: app.height(),
                    depth_or_array_layers: 1,
                },
                format: wgpu::TextureFormat::Depth24Plus,
                mip_level_count: 1,
                sample_count: 1,
                view_formats: &[],
            });

            let new_depth_buffer_view = new_depth_buffer.create_view(&wgpu::TextureViewDescriptor::default());

            self.depth_buffer = Some(new_depth_buffer);
            self.depth_buffer_view = Some(new_depth_buffer_view);
        }

        let now = Instant::now();
        let dt = (now - self.last_frame_start).as_secs_f32();
        if !self.paused {
            self.time += dt;
        }
        self.last_frame_start = now;

        if app.keydown.contains(&KeyCode::ArrowLeft) {
            self.camera_rotation += 5.0 * dt;
        }

        if app.keydown.contains(&KeyCode::ArrowRight) {
            self.camera_rotation -= 5.0 * dt;
        }

        if app.keydown.contains(&KeyCode::ArrowUp) {
            self.camera_distance += 7.0 * dt;
        }

        if app.keydown.contains(&KeyCode::ArrowDown) {
            self.camera_distance -= 7.0 * dt;
        }

        let model_matrix = Mat4::IDENTITY;

        let view_matrix = Mat4::from_translation(Vec3::new(0.0, 0.0, - self.camera_distance))
            * Mat4::from_rotation_x(PI / 6.0)
            * Mat4::from_rotation_y(- self.camera_rotation)
            * Mat4::from_translation(Vec3::new(0.0, - self.camera_height, 0.0));

        let projection_matrix = Mat4::perspective_rh(PI / 3.0, app.width() as f32 / app.height() as f32, 0.01, 100.0);

        let target_view = surface_texture.texture.create_view(&wgpu::TextureViewDescriptor::default());

        let light_direction = Vec3::new((self.time * 0.5).sin(), 1.0, (self.time * 0.5).cos()).normalize();

        let shadow_projection = {
            let light_z = - light_direction;
            let light_x = light_z.cross(Vec3::new(0.0, 1.0, 0.0)).normalize();
            let light_y = - light_z.cross(light_x);

            let projection = Mat4::from_cols(
                light_x.extend(0.0),
                light_y.extend(0.0),
                light_z.extend(0.0),
                Vec4::W,
            ).transpose();

            let z_range_fix = Mat4::from_scale(Vec3::new(1.0, 1.0, 0.5)) * Mat4::from_translation(Vec3::new(0.0, 0.0, 1.0));

            z_range_fix * projection
        };

        let camera_uniforms = CameraUniforms {
            view_projection: projection_matrix * view_matrix,
            position: (view_matrix.inverse() * Vec4::new(0.0, 0.0, 0.0, 1.0)).truncate(),
            padding: 0.0,
        };

        app.queue.write_buffer(&self.camera_uniform_buffer, 0, bytemuck::bytes_of(&camera_uniforms));

        let lights_uniforms = LightsUniforms {
            shadow_projection,
            ambient_intensity: Vec3::new(0.05, 0.10, 0.15),
            padding1: 0.0,
            light_direction: light_direction,
            padding2: 0.0,
            light_intensity: Vec3::new(0.8, 0.6, 0.4),
            padding3: 0.0,
        };

        app.queue.write_buffer(&self.lights_uniform_buffer, 0, bytemuck::bytes_of(&lights_uniforms));

        let mut encoder = app.device.create_command_encoder(&Default::default());

        {
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                color_attachments: &[],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &self.shadow_map_view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                ..Default::default()
            });
            render_pass.set_pipeline(&self.shadow_pipeline);

            let immediates = ShadowImmediates {
                shadow_projection,
            };

            render_pass.set_immediates(0, bytemuck::bytes_of(&immediates));
            render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
            render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
            render_pass.draw_indexed(0..self.index_count, 0, 0..1);
        }

        {
            let mut render_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &target_view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color{r: 0.01, g: 0.03, b: 0.05, a: 1.0}),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: self.depth_buffer_view.as_ref().unwrap(),
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Discard,
                    }),
                    stencil_ops: None,
                }),
                ..Default::default()
            });
            render_pass.set_pipeline(&self.pipeline);

            let immediates = Immediates {
                model: model_matrix,
                albedo: Vec3::new(1.0, 1.0, 1.0),
                roughness: 0.05,
                specular_factor: 0.02,
                padding: [0.0, 0.0, 0.0],
            };

            render_pass.set_immediates(0, bytemuck::bytes_of(&immediates));
            render_pass.set_bind_group(0, &self.camera_bind_group, &[]);
            render_pass.set_bind_group(1, &self.lights_bind_group, &[]);
            render_pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
            render_pass.set_index_buffer(self.index_buffer.slice(..), wgpu::IndexFormat::Uint32);
            render_pass.draw_indexed(0..self.index_count, 0, 0..1);

            let debug_immediates = {
                let r = app.width() as f32 / app.height() as f32;
                let s = 0.4 as f32;

                DebugDepthTextureImmediates {
                    center: Vec2::new(- 1.0 + s / r, - 1.0 + s),
                    extent: Vec2::new(s / r, s),
                }
            };

            render_pass.set_pipeline(&self.debug_depth_texture_pipeline);
            render_pass.set_immediates(0, bytemuck::bytes_of(&debug_immediates));
            render_pass.set_bind_group(0, &self.lights_bind_group, &[]);
            render_pass.draw(0..6, 0..1);
        }

        app.queue.submit(std::iter::once(encoder.finish()));
        app.queue.present(surface_texture);
    }

    fn on_keydown(&mut self, _gpu: &mut WgpuState, key: KeyCode) {
        if key == KeyCode::Space {
            self.paused = !self.paused
        }
    }
}

fn main() {
    run::<Practice09>(AppConfig {
        title: "Practice09",
        width: 1280,
        height: 720,
        srgb: true,
    });
}
